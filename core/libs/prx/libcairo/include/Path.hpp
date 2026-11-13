#pragma once

#include "prx/libcairo/include/Context.hpp"

#include <cstddef>

union CairoPathData {
    struct {
        int type;
        int length;
    } header;
    struct {
        double x;
        double y;
    } point;
};

struct CairoPath {
    int status;
    CairoPathData* data;
    int num_data;
};

static_assert(sizeof(CairoPathData) == 16);
static_assert(sizeof(CairoPath) == 24);
static_assert(offsetof(CairoPath, data) == 8);

extern "C" {
void APS5_VABI cairo_new_path(CairoContext* context);
void APS5_VABI cairo_new_sub_path(CairoContext* context);
void APS5_VABI cairo_move_to(CairoContext* context, double x, double y);
void APS5_VABI cairo_line_to(CairoContext* context, double x, double y);
void APS5_VABI cairo_curve_to(CairoContext* context, double x1, double y1,
                              double x2, double y2, double x3, double y3);
void APS5_VABI cairo_close_path(CairoContext* context);
int APS5_VABI cairo_has_current_point(CairoContext* context);
void APS5_VABI cairo_get_current_point(CairoContext* context, double* x, double* y);
CairoPath* APS5_VABI cairo_copy_path(CairoContext* context);
void APS5_VABI cairo_path_extents(CairoContext* context, double* x1, double* y1,
                                 double* x2, double* y2);
void APS5_VABI cairo_append_path(CairoContext* context, const CairoPath* path);
void APS5_VABI cairo_path_destroy(CairoPath* path);
}
