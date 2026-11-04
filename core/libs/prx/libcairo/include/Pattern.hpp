#pragma once

#include "prx/libcairo/include/InventoryExports.hpp"

#include <atomic>
#include <vector>

struct CairoColorStop {
    double offset;
    double red;
    double green;
    double blue;
    double alpha;
};

struct CairoMeshPoint {
    double x{};
    double y{};
};

struct CairoMeshColor {
    double red{};
    double green{};
    double blue{};
    double alpha{};
};

struct CairoMeshPatch {
    CairoMeshPoint points[4][4]{};
    CairoMeshColor colors[4]{};
};

struct CairoPattern {
    std::atomic<int> refCount{1};
    int status{0};
    int type{0};
    int extend{0};
    int filter{0};
    CairoMatrix matrix{1.0, 0.0, 0.0, 1.0, 0.0, 0.0};
    double geometry[6]{};
    std::vector<CairoColorStop> stops;
    std::vector<CairoMeshPatch> patches;
    int currentSide{-2};
    bool patchActive{};
    bool hasColor[4]{};
};

constexpr int CairoStatusSuccess = 0;
constexpr int CairoStatusNoMemory = 1;
constexpr int CairoStatusPatternTypeMismatch = 14;
constexpr int CairoStatusInvalidIndex = 21;
constexpr int CairoStatusInvalidMeshConstruction = 36;

constexpr int CairoPatternTypeLinear = 2;
constexpr int CairoPatternTypeRadial = 3;
constexpr int CairoPatternTypeMesh = 4;

constexpr int CairoExtendPad = 3;
constexpr int CairoFilterGood = 1;

constexpr int CairoReferenceInvalid = -1;
