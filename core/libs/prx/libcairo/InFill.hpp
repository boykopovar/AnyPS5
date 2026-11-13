#pragma once

#include <cstdint>

struct CairoFixedPoint {
    std::int32_t x;
    std::int32_t y;
};

struct CairoInFillState {
    bool onEdge;
    int winding;
    CairoFixedPoint query;
    bool hasCurrentPoint;
    CairoFixedPoint currentPoint;
    CairoFixedPoint firstPoint;
};

std::int32_t CairoFixedFromDouble_nid_no_patch(double value);
CairoInFillState CairoInFillBegin_nid_no_patch(CairoFixedPoint query);
void CairoInFillMoveTo_nid_no_patch(CairoInFillState& state, CairoFixedPoint point);
void CairoInFillLineTo_nid_no_patch(CairoInFillState& state, CairoFixedPoint point);
void CairoInFillCurveTo_nid_no_patch(CairoInFillState& state, CairoFixedPoint b,
                                     CairoFixedPoint c, CairoFixedPoint d);
void CairoInFillClosePath_nid_no_patch(CairoInFillState& state);
