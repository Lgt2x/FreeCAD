// SPDX-License-Identifier: LGPL-2.1-or-later
/**
 * \file AreaParamsExpanded.h
 * \brief Expanded C++ struct definitions replacing macro-generated parameter structs
 *
 * This file contains expanded C++ struct definitions that replace the macro-generated
 * CAreaParams and AreaParams structs. These structs use C++20 default member initialization
 * instead of constructor initialization.
 *
 * Phase 1 of PARAM_MACRO_REFACTORING: Create expanded structs with identical field names
 * and default values to the macro-generated versions.
 */

#pragma once

#include <Precision.hxx>

// Enum types for AreaParams - match the macro-generated anonymous enum order
// TODO Phase 2: Uncomment these enum classes and update usage sites

// /** Fill mode for output wires */
// enum class FillType : short
// {
//     None = 0,
//     Face = 1,
//     Auto = 2
// };
//
// /** Coplanar checking mode */
// enum class CoplanarType : short
// {
//     None = 0,
//     Check = 1,
//     Force = 2
// };
//
// /** Open wire handling mode */
// enum class OpenModeType : short
// {
//     None = 0,
//     Union = 1,
//     Edges = 2
// };
//
// /** Pocket toolpath pattern mode */
// enum class PocketModeType : short
// {
//     None = 0,
//     ZigZag = 1,
//     Offset = 2,
//     Spiral = 3,
//     ZigZagOffset = 4,
//     Line = 5,
//     Grid = 6,
//     Triangle = 7
// };
//
// /** Section offset coordinate mode */
// enum class SectionModeType : short
// {
//     Absolute = 0,
//     BoundBox = 1,
//     Workplane = 2
// };
//
// /** Arc drawing plane */
// enum class ArcPlaneType : short
// {
//     None = 0,
//     Auto = 1,
//     XY = 2,
//     ZX = 3,
//     YZ = 4,
//     Variable = 5
// };
//
// /** Loop orientation */
// enum class OrientationType : short
// {
//     Normal = 0,
//     Reversed = 1
// };
//
// /** Wire sorting mode */
// enum class SortModeType : short
// {
//     None = 0,
//     TwoPointFive = 1,  // 2D5
//     ThreeD = 2,        // 3D
//     Greedy = 3
// };
//
// /** Path direction enforcement */
// enum class DirectionType : short
// {
//     None = 0,
//     XPositive = 1,
//     XNegative = 2,
//     YPositive = 3,
//     YNegative = 4,
//     ZPositive = 5,
//     ZNegative = 6
// };
//
// /** Retraction axis */
// enum class RetractAxisType : short
// {
//     X = 0,
//     Y = 1,
//     Z = 2
// };
//
// /** Polygon fill type (maps to ClipperLib::PolyFillType) */
// enum class PolyFillType : short
// {
//     NonZero = 0,   // converts to ClipperLib::pftNonZero (1)
//     EvenOdd = 1,   // converts to ClipperLib::pftEvenOdd (0)
//     Positive = 2,  // converts to ClipperLib::pftPositive (2)
//     Negative = 3   // converts to ClipperLib::pftNegative (3)
// };
//
// /** Join type for offsetting (maps to ClipperLib::JoinType) */
// enum class JoinTypeType : short
// {
//     Round = 0,   // converts to ClipperLib::jtRound (1)
//     Square = 1,  // converts to ClipperLib::jtSquare (0)
//     Miter = 2    // converts to ClipperLib::jtMiter (2)
// };
//
// /** End type for offsetting (maps to ClipperLib::EndType) */
// enum class EndTypeType : short
// {
//     OpenRound = 0,      // converts to ClipperLib::etOpenRound (4)
//     ClosedPolygon = 1,  // converts to ClipperLib::etClosedPolygon (0)
//     ClosedLine = 2,     // converts to ClipperLib::etClosedLine (1)
//     OpenSquare = 3,     // converts to ClipperLib::etOpenSquare (3)
//     OpenButt = 4        // converts to ClipperLib::etOpenButt (2)
// };

/**
 * \brief Expanded C++ struct for libarea algorithm configuration
 *
 * This struct replaces the macro-generated CAreaParams struct.
 * Field names match exactly: Tolerance, FitArcs, ClipperSimple, etc.
 */
struct PathExport CAreaParams
{
    // Field names match the PropertyNames (element 2) from AREA_PARAMS_CAREA
    double Tolerance = Precision::Confusion();
    bool FitArcs = true;
    bool Simplify = false;       // was clipper_simple
    double CleanDistance = 0.0;  // was clipper_clean_distance
    double Accuracy = 0.01;
    double Unit = 1.0;  // was units
    short MinArcPoints = 4;
    short MaxArcPoints = 100;
    double ClipperScale = 1e7;

    // Equality operators
    bool operator==(const CAreaParams& other) const = default;
    bool operator!=(const CAreaParams& other) const = default;

    // Constructor
    CAreaParams() = default;
};

/**
 * \brief Expanded C++ struct for all Area configurations
 *
 * This struct replaces the macro-generated AreaParams struct.
 * It inherits from CAreaParams and adds fields from AREA_PARAMS_AREA.
 */
struct PathExport AreaParams: CAreaParams
{
    // From AREA_PARAMS_BASE (including AREA_PARAMS_DEFLECTION and AREA_PARAMS_CLIPPER_FILL)
    short Fill = 2;      // TODO Phase 2: change to FillType::Auto
    short Coplanar = 2;  // TODO Phase 2: change to CoplanarType::Force
    bool Reorient = true;
    bool Outline = false;
    bool Explode = false;
    short OpenMode = 0;  // TODO Phase 2: change to OpenModeType::None
    double Deflection = 0.01;
    // enum2 fields use AreaParams enum order, converted to ClipperLib when needed
    short SubjectFill = 0;  // TODO Phase 2: change to PolyFillType::NonZero
    short ClipFill = 0;     // TODO Phase 2: change to PolyFillType::NonZero

    // From AREA_PARAMS_OFFSET
    double Offset = 0.0;
    long ExtraPass = 0;
    double Stepover = 0.0;

    // From AREA_PARAMS_OFFSET_CONF
    // Note: Algo field is conditional on AREA_OFFSET_ALGO macro, omitting for now
    // enum2 fields use AreaParams enum order, converted to ClipperLib when needed
    short JoinType = 0;  // TODO Phase 2: change to JoinTypeType::Round
    short EndType = 0;   // TODO Phase 2: change to EndTypeType::OpenRound
    double MiterLimit = 2.0;
    double RoundPrecision = 0.0;

    // From AREA_PARAMS_POCKET
    short PocketMode = 0;  // TODO Phase 2: change to PocketModeType::None
    double ToolRadius = 1.0;
    double PocketExtraOffset = 0.0;
    double PocketStepover = 0.0;
    bool FromCenter = false;
    double Angle = 45.0;
    double AngleShift = 0.0;
    double Shift = 0.0;
    bool ForceMaxStepover = false;

    // From AREA_PARAMS_POCKET_CONF
    bool Thicken = false;

    // From AREA_PARAMS_SECTION (including AREA_PARAMS_SECTION_EXTRA)
    long SectionCount = 0;
    double Stepdown = 1.0;
    double SectionOffset = 0.0;
    double SectionTolerance = 1e-6;
    short SectionMode = 2;  // TODO Phase 2: change to SectionModeType::Workplane
    bool Project = false;

    // Equality operators
    bool operator==(const AreaParams& other) const = default;
    bool operator!=(const AreaParams& other) const = default;

    // Methods
    void dump(const char*) const;
    AreaParams() = default;
};
