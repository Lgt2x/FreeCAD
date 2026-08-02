#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>

#include "Base/XMLParser.h"
#include "Base/ParameterScheme.h"
namespace fs = std::filesystem;

class XMLParserTest: public ::testing::Test
{
protected:
    void SetUp() override
    {}

    void TearDown() override
    {}
};

TEST_F(XMLParserTest, TestLoadDocument)
{
    const fs::path base = fs::temp_directory_path();
    const fs::path tmpfile = base / "TestDoc.xml";

    std::ofstream fout(tmpfile);
    fout << R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>)"
         << "<FCParameters>"
         << R"(<FCParamGroup Name="LogLevels">)"
         << R"(<FCInt Name="Default" Value="2"/>)"
         << R"(<FCText Name="AutoloadModule">PartDesignWorkbench</FCText>)"
         << "</FCParamGroup>"
         << "</FCParameters>";
    fout.close();

    auto parsedXML = Base::parseXMLFile(tmpfile);
    ASSERT_TRUE(parsedXML->tag == "FCParameters");
    ASSERT_TRUE(parsedXML->attrs.size() == 0);
    ASSERT_TRUE(parsedXML->content.size() == 0);
    ASSERT_TRUE(parsedXML->children.size() == 1);

    auto&& paramGroup = parsedXML->children[0];
    ASSERT_TRUE(paramGroup->tag == "FCParamGroup");
    ASSERT_TRUE(paramGroup->attrs.size() == 1);
    ASSERT_TRUE(paramGroup->attrs["Name"] == "LogLevels");
    ASSERT_TRUE(paramGroup->content.size() == 0);
    ASSERT_TRUE(paramGroup->children.size() == 2);

    auto&& intParam = paramGroup->children[0];
    ASSERT_TRUE(intParam->tag == "FCInt");
    ASSERT_TRUE(intParam->attrs.size() == 2);
    ASSERT_TRUE(intParam->attrs["Name"] == "Default");
    ASSERT_TRUE(intParam->attrs["Value"] == "2");
    ASSERT_TRUE(intParam->content.size() == 0);
    ASSERT_TRUE(intParam->children.size() == 0);

    auto&& strParam = paramGroup->children[1];
    ASSERT_TRUE(strParam->tag == "FCText");
    ASSERT_TRUE(strParam->attrs.size() == 1);
    ASSERT_TRUE(strParam->attrs["Name"] == "AutoloadModule");
    ASSERT_TRUE(strParam->content == "PartDesignWorkbench");
    ASSERT_TRUE(strParam->children.size() == 0);

    auto res = Base::checkXMLDocument(*parsedXML, ParameterScheme);
    ASSERT_FALSE(res.has_value());

    const fs::path saveTo = base / "TestDocWrite.xml";
    Base::saveXMLFile(saveTo, *parsedXML);

    std::ifstream fin(saveTo);
    std::string line;
    std::getline(fin, line);
    ASSERT_TRUE(line == R"(<?xml version="1.0" encoding="UTF-8" standalone="no" ?>)");
    std::getline(fin, line);
    ASSERT_TRUE(line == R"(<FCParameters>)");
    std::getline(fin, line);
    ASSERT_TRUE(line == "");
    std::getline(fin, line);
    line.erase(0, line.find_first_not_of(' '));
    ASSERT_TRUE(line == R"(<FCParamGroup Name="LogLevels">)");
    std::getline(fin, line);
    line.erase(0, line.find_first_not_of(' '));
    ASSERT_TRUE(line == R"(<FCInt Name="Default" Value="2"/>)");
    std::getline(fin, line);
    line.erase(0, line.find_first_not_of(' '));
    ASSERT_TRUE(line == R"(<FCText Name="AutoloadModule">PartDesignWorkbench</FCText>)");
    std::getline(fin, line);
    line.erase(0, line.find_first_not_of(' '));
    ASSERT_TRUE(line == R"(</FCParamGroup>)");
    std::getline(fin, line);
    ASSERT_TRUE(line == "");
    std::getline(fin, line);
    ASSERT_TRUE(line == "</FCParameters>");
}
