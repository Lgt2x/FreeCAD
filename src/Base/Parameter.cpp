// SPDX-License-Identifier: LGPL-2.1-or-later

/***************************************************************************
 *   Copyright (c) 2002 Jürgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Library General Public License (LGPL)   *
 *   as published by the Free Software Foundation; either version 2 of     *
 *   the License, or (at your option) any later version.                   *
 *   for detail see the LICENCE text file.                                 *
 *                                                                         *
 *   FreeCAD is distributed in the hope that it will be useful,            *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with FreeCAD; if not, write to the Free Software        *
 *   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  *
 *   USA                                                                   *
 *                                                                         *
 ***************************************************************************/


#include "Handle.h"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include <FCConfig.h>

#ifdef FC_OS_LINUX
# include <unistd.h>
#endif

#include <boost/algorithm/string.hpp>
#include <fmt/printf.h>

#include "Parameter.h"
#include "ParameterScheme.h"
#include "Console.h"
#include "Exception.h"
#include "FileInfo.h"
#include "XMLParser.h"
#include "FileLock.h"
#include "Tools.h"

FC_LOG_LEVEL_INIT("Parameter", true, true)

using namespace Base;
namespace
{
constexpr int BASE = 10;
}

ParameterGrp::ParameterGrp(Base::XMLElement* groupNode, const std::string& name, ParameterGrp* Parent)
    : groupName(name)
    , groupNode(groupNode)
    , parent(Parent)
{
    // TODO
    // assert(groupNode != nullptr);
    // assert(Parent != nullptr);
    // assert(Parent->Manager() != nullptr);
    if (parent) {
        manager = parent->manager;
    }

    if (groupNode) {
        groupNode->attrs["Name"] = name;
    }
}

std::string ParameterGrp::GetGroupName()
{
    return this->groupName;
}


void ParameterGrp::copyTo(const Base::Reference<ParameterGrp>& Group)
{
    if (Group == this) {
        return;
    }

    // delete previous content
    Group->Clear(true);

    // copy all
    insertTo(Group);
}

void ParameterGrp::insertTo(const Base::Reference<ParameterGrp>& Group)
{
    if (Group == this) {
        return;
    }
    for (const auto& group : GetGroups()) {
        group->insertTo(Group->GetGroup(group->groupName));
    }
    for (const auto& it : GetASCIIMap()) {
        Group->SetASCII(it.first, it.second);
    }
    for (const auto& it : GetBoolMap()) {
        Group->SetBool(it.first, it.second);
    }
    for (const auto& it : GetIntMap()) {
        Group->SetInt(it.first, it.second);
    }
    for (const auto& it : GetFloatMap()) {
        Group->SetFloat(it.first, it.second);
    }
    for (const auto& it : GetUnsignedMap()) {
        Group->SetUnsigned(it.first, it.second);
    }
}

void ParameterGrp::exportTo(const char* FileName)
{
    auto Mngr = ParameterManager::Create();

    Mngr->CreateDocument();

    // copy all into the new document
    insertTo(Base::Reference<ParameterGrp>(Mngr));

    Mngr->SaveDocument(FileName);
}

void ParameterGrp::importFrom(const char* FileName)
{
    auto Mngr = ParameterManager::Create();

    if (Mngr->LoadDocument(FileName) != 1) {
        throw FileException("ParameterGrp::import() cannot load document", FileName);
    }

    Mngr->copyTo(Base::Reference<ParameterGrp>(this));
}

void ParameterGrp::revert(const Base::Reference<ParameterGrp>& Group)
{
    if (Group == this) {
        return;
    }

    for (auto& grp : Group->GetGroups()) {
        if (HasGroup(grp->groupName)) {
            GetGroup(grp->groupName)->revert(grp);
        }
    }

    for (const auto& v : Group->GetASCIIMap()) {
        if (GetASCII(v.first, v.second.c_str()) == v.second) {
            RemoveASCII(v.first);
        }
    }

    for (const auto& v : Group->GetBoolMap()) {
        if (GetBool(v.first, v.second) == v.second) {
            RemoveBool(v.first);
        }
    }

    for (const auto& v : Group->GetIntMap()) {
        if (GetInt(v.first, v.second) == v.second) {
            RemoveInt(v.first);
        }
    }

    for (const auto& v : Group->GetUnsignedMap()) {
        if (GetUnsigned(v.first, v.second) == v.second) {
            RemoveUnsigned(v.first);
        }
    }

    for (const auto& v : Group->GetFloatMap()) {
        if (GetFloat(v.first, v.second) == v.second) {
            RemoveFloat(v.first);
        }
    }
}

const Base::XMLElement* ParameterGrp::GetRootNode() const
{
    if (this->detached && this->parent) {
        this->parent->GetGroup(this->groupName);  // Recreate the group
    }
    return this->groupNode;
}

Base::XMLElement* ParameterGrp::GetRootNode()
{
    return const_cast<Base::XMLElement*>(const_cast<const ParameterGrp*>(this)->GetRootNode());
}

Base::XMLElement* ParameterGrp::CreateElement(
    Base::XMLElement* start,
    const std::string& Type,
    const std::string& Name
)
{
    if (start->tag != "FCParamGroup" && start->tag != "FCParameters") {
        throw Base::TypeError(
            std::format("CreateElement: {} cannot have the element {} of type {}\n", start->tag, Name, Type)
        );
    }

    auto newElement = std::make_unique<XMLElement>();
    newElement->tag = Type;
    newElement->attrs = {{"Name", Name}};
    start->children.emplace_back(std::move(newElement));
    return start->children.back().get();

    // TODO notify
}

Base::Reference<ParameterGrp> ParameterGrp::GetGroup(const std::string& Name)
{
    if (Name.empty()) {
        throw Base::ValueError("Empty group name");
    }

    Base::Reference<ParameterGrp> hGrp = this;
    std::string_view path = Name;
    while (!path.empty()) {
        const auto pos = path.find('/');
        auto token = path.substr(0, pos);

        // trim
        const auto first = token.find_first_not_of(" \t\n\r");
        const auto last = token.find_last_not_of(" \t\n\r");

        if (first != std::string_view::npos) {
            token = token.substr(first, last - first + 1);
            hGrp = hGrp->GetOrCreateGroup(std::string(token));
        }

        if (pos == std::string_view::npos) {
            break;
        }

        path.remove_prefix(pos + 1);
    }

    if (hGrp == this) {
        throw Base::ValueError("Empty group name");
    }
    return hGrp;
}

Base::Reference<ParameterGrp> ParameterGrp::GetOrCreateGroup(const std::string& Name)
{
    if (groupMap.contains(Name) && groupMap[Name].isValid() && !groupMap[Name]->detached) {
        return groupMap[Name];
    }

    Base::XMLElement* pcTemp = FindElement(GetRootNode(), "FCParamGroup", Name);
    if (!pcTemp) {
        Base::XMLElement* newGroup = CreateElement(GetRootNode(), "FCParamGroup", Name);
        pcTemp = newGroup;
        // TODO notify?
    }

    // Reattach group
    if (groupMap.contains(Name) && groupMap[Name]->detached) {
        groupMap[Name]->groupNode = pcTemp;
        groupMap[Name]->detached = false;
    }

    // create and register handle
    if (!groupMap.contains(Name) || !groupMap[Name].isValid()) {
        auto rParamGrp = Base::Reference<ParameterGrp>(new ParameterGrp(pcTemp, Name, this));
        groupMap[Name] = rParamGrp;
    }

    return groupMap[Name];
}

std::string ParameterGrp::GetPath() const
{
    std::string path;
    if (parent && parent != manager) {
        path = parent->GetPath();
    }
    if (!path.empty() && !groupName.empty()) {
        path += "/";
    }
    path += groupName;
    return path;
}


std::vector<Base::Reference<ParameterGrp>> ParameterGrp::GetGroups()
{
    std::vector<Base::Reference<ParameterGrp>> vrParamGrp;
    auto groups = FindAllElements(GetRootNode(), "FCParamGroup");
    vrParamGrp.reserve(groups.size());
    for (const auto& it : groups) {
        vrParamGrp.push_back(GetOrCreateGroup(it->attrs["Name"]));
    }

    return vrParamGrp;
}

/// test if this group is empty
bool ParameterGrp::IsEmpty() const
{
    return GetRootNode()->children.empty();
}

/// test if a special sub group is in this group
bool ParameterGrp::HasGroup(const std::string& Name) const
{
    if (groupMap.contains(Name)) {
        return true;
    }

    return FindElement(GetRootNode(), "FCParamGroup", Name);
}

std::string ParameterGrp::TypeName(ParamType Type)
{
    switch (Type) {
        case ParamType::FCBool:
            return "FCBool";
        case ParamType::FCInt:
            return "FCInt";
        case ParamType::FCUInt:
            return "FCUInt";
        case ParamType::FCText:
            return "FCText";
        case ParamType::FCFloat:
            return "FCFloat";
        case ParamType::FCGroup:
            return "FCParamGroup";
        default:
            return "";
    }
}

ParameterGrp::ParamType ParameterGrp::TypeValue(const std::string& Name)
{
    if (Name == "FCBool") {
        return ParamType::FCBool;
    }
    if (Name == "FCInt") {
        return ParamType::FCInt;
    }
    if (Name == "FCUInt") {
        return ParamType::FCUInt;
    }
    if (Name == "FCText") {
        return ParamType::FCText;
    }
    if (Name == "FCFloat") {
        return ParamType::FCFloat;
    }
    if (Name == "FCParamGroup") {
        return ParamType::FCGroup;
    }
    return ParamType::FCInvalid;
}

void ParameterGrp::SetAttribute(ParamType Type, const std::string& Name, const std::string& Value)
{
    switch (Type) {
        case ParamType::FCBool:
        case ParamType::FCInt:
        case ParamType::FCUInt:
        case ParamType::FCFloat:
            SetAttributeInternal(Type, Name, Value);
            break;
        case ParamType::FCText:
            SetASCII(Name, Value);
            break;
        case ParamType::FCGroup:
            RenameGrp(Name, Value);
            break;
        default:
            break;
    }
}

std::string ParameterGrp::GetAttribute(
    ParamType Type,
    const std::string& Name,
    std::string& Value,
    const std::string& Default
) const
{
    const std::string T = TypeName(Type);
    if (T.empty()) {
        Value = Default;
        return Default;
    }

    auto pcElem = FindElement(GetRootNode(), T, Name);
    if (!pcElem) {
        Value = Default;
        return Default;
    }

    if (Type == ParamType::FCText) {
        Value = GetASCII(Name, Default.c_str());
    }
    else if (Type != ParamType::FCGroup) {
        Value = pcElem->attrs.at("Value");
    }
    return Value;
}

std::map<std::string, std::string> ParameterGrp::GetAttributeMap(
    ParamType Type,
    const std::string& sFilter
) const
{
    std::map<std::string, std::string> res;

    const std::string typeName = TypeName(Type);
    if (typeName.empty()) {
        return res;
    }

    std::string Name;

    for (const auto& element : FindAllElements(GetRootNode(), typeName)) {
        Name = element->attrs["Name"];
        // check on filter condition
        if (sFilter.empty() || Name.find(sFilter) != std::string::npos) {
            if (Type == ParamType::FCGroup) {
                res[Name] = "";
            }
            else if (Type == ParamType::FCText) {
                res[Name] = GetASCII(Name);
            }
            else {
                res[Name] = element->attrs["Value"];
            }
        }
    }
    return res;
}

void ParameterGrp::RemoveAttribute(ParamType Type, const std::string& Name)
{
    switch (Type) {
        case ParamType::FCBool:
            RemoveBool(Name);
            break;
        case ParamType::FCInt:
            RemoveInt(Name);
            break;
        case ParamType::FCUInt:
            RemoveUnsigned(Name);
            break;
        case ParamType::FCText:
            RemoveASCII(Name);
            break;
        case ParamType::FCFloat:
            RemoveFloat(Name);
            break;
        case ParamType::FCGroup:
            RemoveGrp(Name);
            break;
        default:
            break;
    }
}

void ParameterGrp::NotifyChange(ParamType Type, const std::string& Name, const std::string& Value)
{
    this->Manager()->signalParamChanged(this, Type, Name, Value);
}

void ParameterGrp::SetAttributeInternal(ParamType T, const std::string& Name, const std::string& Value)
{
    const std::string Type = TypeName(T);
    if (Type.empty()) {
        return;
    }

    Base::XMLElement* pcElem = FindOrCreateElement(GetRootNode(), Type, Name);
    if (!pcElem->attrs.contains("Value") || pcElem->attrs.at("Value") != Value) {
        {
            pcElem->attrs["Value"] = Value;
            NotifyChange(T, Name, Value);
        }
    }

    // For backward compatibility, old observer gets notified regardless of
    // value changes or not.
    Notify(Name.c_str());
}

bool ParameterGrp::GetBool(const std::string& Name, bool bPreset) const
{
    const Base::XMLElement* pcElem = FindElement(GetRootNode(), "FCBool", Name);
    return pcElem ? pcElem->attrs.at("Value") == "1" : bPreset;
}

void ParameterGrp::SetBool(const std::string& Name, bool bValue)
{
    SetAttributeInternal(ParamType::FCBool, Name, bValue ? "1" : "0");
}

std::vector<bool> ParameterGrp::GetBools(const char* sFilter) const
{
    std::vector<bool> vrValues;
    for (const auto& group : FindAllElements(GetRootNode(), "FCBool")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues.push_back(group->attrs.at("Value") == "1");
        }
    }
    return vrValues;
}

std::map<std::string, bool> ParameterGrp::GetBoolMap(const char* sFilter) const
{
    std::map<std::string, bool> vrValues;
    for (const auto& group : FindAllElements(GetRootNode(), "FCBool")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues[name] = group->attrs.at("Value") == "1";
        }
    }

    return vrValues;
}

long ParameterGrp::GetInt(const std::string& Name, long lPreset) const
{
    const Base::XMLElement* pcElem = FindElement(GetRootNode(), "FCInt", Name);
    return pcElem ? atol(pcElem->attrs.at("Value").c_str()) : lPreset;
}

void ParameterGrp::SetInt(const std::string& Name, long lValue)
{
    std::string buf = fmt::sprintf("%li", lValue);
    SetAttributeInternal(ParamType::FCInt, Name, buf);
}

std::vector<long> ParameterGrp::GetInts(const char* sFilter) const
{
    std::vector<long> vrValues;

    for (const auto& group : FindAllElements(GetRootNode(), "FCInt")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues.emplace_back(atol(group->attrs.at("Value").c_str()));
        }
    }

    return vrValues;
}

std::map<std::string, long> ParameterGrp::GetIntMap(const char* sFilter) const
{
    std::map<std::string, long> vrValues;

    for (const auto& group : FindAllElements(GetRootNode(), "FCInt")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues[name] = atol(group->attrs.at("Value").c_str());
        }
    }

    return vrValues;
}

unsigned long ParameterGrp::GetUnsigned(const std::string& Name, unsigned long lPreset) const
{
    const Base::XMLElement* pcElem = FindElement(GetRootNode(), "FCUInt", Name);
    return pcElem ? strtoul(pcElem->attrs.at("Value").c_str(), nullptr, ::BASE) : lPreset;
}

void ParameterGrp::SetUnsigned(const std::string& Name, unsigned long lValue)
{
    std::string buf = fmt::sprintf("%lu", lValue);
    SetAttributeInternal(ParamType::FCUInt, Name, buf);
}

std::vector<unsigned long> ParameterGrp::GetUnsigneds(const char* sFilter) const
{
    std::vector<unsigned long> vrValues;

    for (const auto& group : FindAllElements(GetRootNode(), "FCUInt")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues.emplace_back(strtoul(group->attrs.at("Value").c_str(), nullptr, ::BASE));
        }
    }

    return vrValues;
}

std::map<std::string, unsigned long> ParameterGrp::GetUnsignedMap(const char* sFilter) const
{
    std::map<std::string, unsigned long> vrValues;

    for (const auto& group : FindAllElements(GetRootNode(), "FCUInt")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues[name] = strtoul(group->attrs.at("Value").c_str(), nullptr, ::BASE);
        }
    }

    return vrValues;
}

double ParameterGrp::GetFloat(const std::string& Name, double dPreset) const
{
    const Base::XMLElement* pcElem = FindElement(GetRootNode(), "FCFloat", Name);
    return pcElem ? atof(pcElem->attrs.at("Value").c_str()) : dPreset;
}

void ParameterGrp::SetFloat(const std::string& Name, double dValue)
{
    // use %.12f instead of %f to handle values < 1.0e-6
    std::string buf = fmt::sprintf("%.12f", dValue);
    SetAttributeInternal(ParamType::FCFloat, Name, buf);
}

std::vector<double> ParameterGrp::GetFloats(const char* sFilter) const
{
    std::vector<double> vrValues;

    for (const auto& group : FindAllElements(GetRootNode(), "FCFloat")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues.emplace_back(atof(group->attrs.at("Value").c_str()));
        }
    }

    return vrValues;
}

std::map<std::string, double> ParameterGrp::GetFloatMap(const char* sFilter) const
{
    std::map<std::string, double> vrValues;

    for (const auto& group : FindAllElements(GetRootNode(), "FCFloat")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues[name] = atof(group->attrs.at("Value").c_str());
        }
    }

    return vrValues;
}


void ParameterGrp::SetASCII(const std::string& Name, const std::string& sValue)
{
    Base::XMLElement* pcElem = FindElement(GetRootNode(), "FCText", Name);
    if (!pcElem) {
        pcElem = CreateElement(GetRootNode(), "FCText", Name);
    }
    if (pcElem->content != sValue) {
        pcElem->content = sValue;  // TODO: UTF-8
        NotifyChange(ParamType::FCText, Name, sValue);
        Notify(Name.c_str());
    }
}

std::string ParameterGrp::GetASCII(const std::string& Name, const char* pPreset) const
{
    const Base::XMLElement* pcElem = FindElement(GetRootNode(), "FCText", Name);
    std::string preset = (pPreset ? std::string(pPreset) : "");  // Avoid conversion of nullptr to
                                                                 // std::string
    return pcElem ? pcElem->content : preset;
}

std::vector<std::string> ParameterGrp::GetASCIIs(const char* sFilter) const
{
    std::vector<std::string> vrValues;
    for (const auto& group : FindAllElements(GetRootNode(), "FCText")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues.emplace_back(group->content);
        }
    }

    return vrValues;
}

std::map<std::string, std::string> ParameterGrp::GetASCIIMap(const char* sFilter) const
{
    std::map<std::string, std::string> vrValues;
    for (const auto& group : FindAllElements(GetRootNode(), "FCText")) {
        std::string name = group->attrs.at("Name");
        if (!sFilter || name.find(sFilter) != std::string::npos) {
            vrValues[name] = group->content;
        }
    }
    return vrValues;
}

//**************************************************************************
// Access methods

void ParameterGrp::RemoveASCII(const std::string& Name)
{
    for (size_t i = 0; i < GetRootNode()->children.size(); i++) {
        if (GetRootNode()->children[i]->tag == "FCText"
            && GetRootNode()->children[i]->attrs.at("Name") == Name) {
            GetRootNode()->children.erase(GetRootNode()->children.begin() + static_cast<int>(i));
            NotifyChange(ParamType::FCBool, Name, "");
            Notify(Name.c_str());
            return;
        }
    }
}

void ParameterGrp::RemoveBool(const std::string& Name)
{
    for (size_t i = 0; i < GetRootNode()->children.size(); i++) {
        if (GetRootNode()->children[i]->tag == "FCBool"
            && GetRootNode()->children[i]->attrs.at("Name") == Name) {
            GetRootNode()->children.erase(GetRootNode()->children.begin() + static_cast<int>(i));
            NotifyChange(ParamType::FCBool, Name, "");
            Notify(Name.c_str());
            return;
        }
    }
}


void ParameterGrp::RemoveFloat(const std::string& Name)
{
    for (size_t i = 0; i < GetRootNode()->children.size(); i++) {
        if (GetRootNode()->children[i]->tag == "FCFloat"
            && GetRootNode()->children[i]->attrs.at("Name") == Name) {
            GetRootNode()->children.erase(GetRootNode()->children.begin() + static_cast<int>(i));
            NotifyChange(ParamType::FCFloat, Name, "");
            Notify(Name.c_str());
            return;
        }
    }
}

void ParameterGrp::RemoveInt(const std::string& Name)
{
    for (size_t i = 0; i < GetRootNode()->children.size(); i++) {
        if (GetRootNode()->children[i]->tag == "FCInt"
            && GetRootNode()->children[i]->attrs.at("Name") == Name) {
            GetRootNode()->children.erase(GetRootNode()->children.begin() + static_cast<int>(i));
            NotifyChange(ParamType::FCInt, Name, "");
            Notify(Name.c_str());
            return;
        }
    }
}

void ParameterGrp::RemoveUnsigned(const std::string& Name)
{
    for (size_t i = 0; i < GetRootNode()->children.size(); i++) {
        if (GetRootNode()->children[i]->tag == "FCUInt"
            && GetRootNode()->children[i]->attrs.at("Name") == Name) {
            GetRootNode()->children.erase(GetRootNode()->children.begin() + static_cast<int>(i));
            NotifyChange(ParamType::FCUInt, Name, "");
            Notify(Name.c_str());
            return;
        }
    }
}

Base::Color ParameterGrp::GetColor(const std::string& Name, Base::Color lPreset) const
{
    auto packed = GetUnsigned(Name, lPreset.getPackedValue());

    return Color(static_cast<uint32_t>(packed));
}

void ParameterGrp::SetColor(const std::string& Name, Base::Color lValue)
{
    SetUnsigned(Name, lValue.getPackedValue());
}

std::vector<Base::Color> ParameterGrp::GetColors(const char* sFilter) const
{
    auto packed = GetUnsigneds(sFilter);
    std::vector<Base::Color> result;

    std::transform(
        packed.begin(),
        packed.end(),
        std::back_inserter(result),
        [](const unsigned long lValue) { return Color(static_cast<uint32_t>(lValue)); }
    );

    return result;
}

std::map<std::string, Base::Color> ParameterGrp::GetColorMap(const char* sFilter) const
{
    std::map<std::string, Base::Color> result;
    for (const auto& color : GetUnsignedMap(sFilter)) {
        result[color.first] = Color(static_cast<uint32_t>(color.second));
    }
    return result;
}

void ParameterGrp::RemoveColor(const std::string& Name)
{
    RemoveUnsigned(Name);
}

void ParameterGrp::RemoveGrp(const std::string& Name)
{
    auto it = groupMap.find(Name);
    if (it == groupMap.end()) {
        return;
    }
    Base::Reference<ParameterGrp> removedGroup = it->second;

    // If this or any of its children is referenced by an observer we do not
    // delete the handle, just in case the group is later added again, or else
    // those existing observer won't get any notification. BUT, we DO delete
    // the underlying xml elements, so that we don't save the empty group
    // later.
    removedGroup->Clear();
    removedGroup->detached = true;
    for (size_t i = 0; i < GetRootNode()->children.size(); i++) {
        if (GetRootNode()->children[i]->tag == "FCParamGroup"
            && GetRootNode()->children[i]->attrs.at("Name") == Name) {
            GetRootNode()->children.erase(GetRootNode()->children.begin() + static_cast<int>(i));
            break;
        }
    }

    // trigger observer
    Notify(Name.c_str());
}

bool ParameterGrp::RenameGrp(const std::string& OldName, const std::string& NewName)
{
    auto it = groupMap.find(OldName);
    if (it == groupMap.end()) {
        return false;
    }
    auto jt = groupMap.find(NewName);
    if (jt != groupMap.end()) {
        return false;
    }

    // rename group handle and attribute
    groupMap[NewName] = groupMap[OldName];
    groupMap.erase(OldName);
    groupMap[NewName]->GetRootNode()->attrs["Name"] = NewName;

    NotifyChange(ParamType::FCGroup, NewName, OldName);
    return true;
}

void ParameterGrp::Clear(bool notify)
{
    Base::StateLocker guard(clearing);

    // early trigger notification of group removal when all its children
    // hierarchies are intact.
    NotifyChange(ParamType::FCGroup, "", "");

    // checking on references
    for (auto it = groupMap.begin(); it != groupMap.end();) {
        // If a group handle is referenced by some observer, then do not remove
        // it but clear it, so that any existing observer can still get
        // notification if the group is later on add back. We do remove the
        // underlying xml element from its parent so that we won't save this
        // empty group.
        it->second->Clear(notify);
        if (!it->second->detached) {
            it->second->detached = true;
            for (size_t i = 0; i < GetRootNode()->children.size(); i++) {
                if (GetRootNode()->children[i]->tag == "FCParamGroup"
                    && GetRootNode()->children[i]->attrs.at("Name") == it->second->groupName) {
                    GetRootNode()->children.erase(
                        GetRootNode()->children.begin() + static_cast<int>(i)
                    );
                    break;
                }
            }
        }
        if (!it->second->ShouldRemove()) {
            ++it;
        }
        else {
            it = groupMap.erase(it);
        }
    }

    // Remove the rest of non-group nodes;
    std::map<ParamType, std::string> params;
    for (auto& child : GetRootNode()->children) {
        ParamType type = TypeValue(child->tag);
        if (type != ParamType::FCInvalid && type != ParamType::FCGroup) {
            params[type] = child->attrs.at("Name");
        }
    }

    GetRootNode()->children.erase(
        std::remove_if(
            GetRootNode()->children.begin(),
            GetRootNode()->children.end(),
            [](const auto& e) { return e->tag != "FCParamGroup"; }
        ),
        GetRootNode()->children.end()
    );

    for (auto& v : params) {
        NotifyChange(v.first, v.second, "");
        if (notify) {
            Notify(v.second.c_str());
        }
    }

    // trigger observer
    Notify("");
}

//**************************************************************************
// Access methods

bool ParameterGrp::ShouldRemove() const
{
    if (this->getRefCount() > 1) {
        return false;
    }

    return std::all_of(groupMap.cbegin(), groupMap.cend(), [](const auto& it) {
        return it.second->ShouldRemove();
    });
}

const Base::XMLElement* ParameterGrp::FindElement(
    const Base::XMLElement* start,
    const std::string& Type,
    const std::string& Name
) const
{
    if (start->tag != "FCParamGroup" && start->tag != "FCParameters") {
        Base::Console().warning(
            fmt::format("FindElement: {} cannot have the element {} of type {}\n", start->tag, Name, Type)
                .c_str()
        );
        return nullptr;
    }

    for (const auto& childElem : start->children) {
        if (childElem->tag == Type
            && (Name.empty()
                || (childElem->attrs.contains("Name") && childElem->attrs.at("Name") == Name))) {
            return childElem.get();
        }
    }

    return nullptr;
}

Base::XMLElement* ParameterGrp::FindElement(
    const Base::XMLElement* Start,
    const std::string& Type,
    const std::string& Name
)
{
    return const_cast<Base::XMLElement*>(
        const_cast<const ParameterGrp*>(this)->FindElement(Start, Type, Name)
    );
}

std::vector<Base::XMLElement*> ParameterGrp::FindAllElements(
    const Base::XMLElement* start,
    const std::string& Type
) const
{
    std::vector<Base::XMLElement*> allElems;
    for (const auto& childElem : start->children) {
        if (childElem->tag == Type) {
            allElems.emplace_back(childElem.get());
        }
    }
    return allElems;
}

Base::XMLElement* ParameterGrp::FindOrCreateElement(
    Base::XMLElement* Start,
    const std::string& Type,
    const std::string& Name
)
{
    Base::XMLElement* pcElem = FindElement(Start, Type, Name);
    if (!pcElem) {
        pcElem = CreateElement(Start, Type, Name);
    }
    return pcElem ? pcElem : CreateElement(Start, Type, Name);
}


std::optional<std::string> ParameterGrp::FindAttribute(Base::XMLElement& Node, const std::string& Name) const
{
    if (Node.attrs.contains("Name")) {
        return Node.attrs[Name];
    }
    return std::nullopt;
}

std::vector<std::pair<ParameterGrp::ParamType, std::string>> ParameterGrp::GetParameterNames(
    const std::string& sFilter
) const
{
    std::vector<std::pair<ParameterGrp::ParamType, std::string>> res;
    for (const auto& childElem : GetRootNode()->children) {
        if (sFilter.empty() || childElem->attrs.at("Name").find(sFilter) != std::string::npos) {
            res.emplace_back(TypeValue(childElem->tag), childElem->attrs.at("Name"));
        }
    }
    return res;
}

void ParameterGrp::NotifyAll()
{
    // get all ints and notify
    std::map<std::string, long> IntMap = GetIntMap();
    for (const auto& it : IntMap) {
        Notify(it.first.c_str());
    }

    // get all booleans and notify
    std::map<std::string, bool> BoolMap = GetBoolMap();
    for (const auto& it : BoolMap) {
        Notify(it.first.c_str());
    }

    // get all Floats and notify
    std::map<std::string, double> FloatMap = GetFloatMap();
    for (const auto& it : FloatMap) {
        Notify(it.first.c_str());
    }

    // get all strings and notify
    std::map<std::string, std::string> StringMap = GetASCIIMap();
    for (const auto& it : StringMap) {
        Notify(it.first.c_str());
    }

    // get all uints and notify
    std::map<std::string, unsigned long> UIntMap = GetUnsignedMap();
    for (const auto& it : UIntMap) {
        Notify(it.first.c_str());
    }
}

//**************************************************************************
//**************************************************************************
// ParameterSerializer
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
ParameterSerializer::ParameterSerializer(std::string fn)
    : filename(std::move(fn))
{}

ParameterSerializer::~ParameterSerializer() = default;

void ParameterSerializer::SaveDocument(const ParameterManager& mgr)
{
    mgr.SaveDocument(filename);
}

int ParameterSerializer::LoadDocument(ParameterManager& mgr)
{
    return mgr.LoadDocument(filename);
}

bool ParameterSerializer::LoadOrCreateDocument(ParameterManager& mgr)
{
    return mgr.LoadOrCreateDocument(filename);
}


/** Destruction
 * complete destruction of the object
 */
ParameterManager::~ParameterManager()
{
    // TODO: smart ptr
    delete paramSerializer;
}

ParameterManager::ParameterManager()
    : ParameterGrp(nullptr, "Root")
{
    manager = this;
    groupName = "Root";
}


Base::Reference<ParameterManager> ParameterManager::Create()
{
    auto mgr = new ParameterManager();
    mgr->CreateDocument();
    return mgr;
}

//**************************************************************************
// Serializer handling

void ParameterManager::SetSerializer(ParameterSerializer* ps)
{
    if (paramSerializer != ps) {
        delete paramSerializer;
    }
    paramSerializer = ps;
}

bool ParameterManager::HasSerializer() const
{
    return (paramSerializer != nullptr);
}


void ParameterManager::SetIgnoreSave(bool value)
{
    gIgnoreSave = value;
}

bool ParameterManager::IgnoreSave() const
{
    return gIgnoreSave;
}

namespace
{
std::string getLockFile(const Base::FileInfo& file)
{
    return Base::FileInfo::getTempPath() + file.fileName() + ".lock";
}

int getTimeout()
{
    const int timeout = 5000;
    return timeout;
}
}  // namespace

//**************************************************************************
// Document handling

bool ParameterManager::LoadOrCreateDocument(const std::string& sFileName)
{
    Base::FileInfo file(sFileName);
    if (file.exists()) {
        this->LoadDocument(sFileName);
        return false;
    }

    CreateDocument();
    return true;
}

int ParameterManager::LoadDocument(const std::string& sFileName)
{
    try {
        Base::FileInfo file(sFileName);
        Base::FileLock lock(getLockFile(file));
        if (!lock.tryLock(getTimeout())) {
            // Continue with empty config
            CreateDocument();
            SetIgnoreSave(true);
            std::cerr << "Failed to access file for reading: " << sFileName << '\n';
            return 1;
        }
        this->XMLDocument = Base::parseXMLFile(sFileName);
    }
    catch (const Base::Exception& e) {
        std::cerr << e.what() << "std::endl";
        throw;
    }
    catch (...) {
        std::cerr << "An error occurred during parsing\n " << '\n';
        throw;
    }
    return 1;
}

void ParameterManager::SaveDocument(const std::string& sFileName) const
{
    try {
        Base::saveXMLFile(sFileName, *XMLDocument);
    }
    catch (XMLBaseException& e) {  // TODO: special except
        std::cerr << "An error occurred during creation of output transcoder. Msg is:" << '\n'
                  << e.getMessage() << '\n';
    }
}

const std::string& ParameterManager::GetSerializeFileName() const
{
    static const std::string _dummy;
    return paramSerializer ? paramSerializer->GetFileName() : _dummy;
}

int ParameterManager::LoadDocument()
{
    if (paramSerializer) {
        return paramSerializer->LoadDocument(*this);
    }

    return -1;
}

bool ParameterManager::LoadOrCreateDocument()
{
    if (paramSerializer) {
        return paramSerializer->LoadOrCreateDocument(*this);
    }

    return false;
}

void ParameterManager::SaveDocument() const
{
    if (paramSerializer) {
        paramSerializer->SaveDocument(*this);
    }
}


void ParameterManager::CreateDocument()
{
    XMLDocument = std::make_unique<XMLElement>();
    XMLDocument->tag = "FCParameters";  // TODO: constant somewhere
    auto root = std::make_unique<XMLElement>();
    root->tag = "FCParamGroup";
    root->attrs["Name"] = "Root";
    XMLDocument->children.emplace_back(std::move(root));
}

bool ParameterManager::CheckDocument() const
{
    auto res = Base::checkXMLDocument(*XMLDocument, ParameterScheme);
    if (res.has_value()) {
        Base::Console().error(res.value().c_str());
        return false;
    }

    return true;
}

const Base::XMLElement* ParameterManager::GetRootNode() const
{
    // TODO check created
    return XMLDocument->children.front().get();
}
