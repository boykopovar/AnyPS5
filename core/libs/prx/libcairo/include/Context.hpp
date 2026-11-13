#pragma once

#include "SceTypes.hpp"
#include "prx/libcairo/include/Matrix.hpp"
#include "prx/libcairo/include/Surface.hpp"

struct CairoContext;

extern "C" {
CairoContext* APS5_VABI cairo_create(CairoSurface* target);
CairoContext* APS5_VABI cairo_reference(CairoContext* context);
void APS5_VABI cairo_destroy(CairoContext* context);
void APS5_VABI cairo_save(CairoContext* context);
void APS5_VABI cairo_restore(CairoContext* context);
CairoSurface* APS5_VABI cairo_get_target(CairoContext* context);

void APS5_VABI cairo_translate(CairoContext* context, double tx, double ty);
void APS5_VABI cairo_scale(CairoContext* context, double sx, double sy);
void APS5_VABI cairo_rotate(CairoContext* context, double radians);
void APS5_VABI cairo_transform(CairoContext* context, const CairoMatrix* matrix);
void APS5_VABI cairo_set_matrix(CairoContext* context, const CairoMatrix* matrix);
void APS5_VABI cairo_get_matrix(CairoContext* context, CairoMatrix* matrix);
void APS5_VABI cairo_identity_matrix(CairoContext* context);
void APS5_VABI cairo_user_to_device(CairoContext* context, double* x, double* y);
void APS5_VABI cairo_user_to_device_distance(CairoContext* context, double* dx, double* dy);
void APS5_VABI cairo_device_to_user(CairoContext* context, double* x, double* y);
void APS5_VABI cairo_device_to_user_distance(CairoContext* context, double* dx, double* dy);

void APS5_VABI cairo_set_antialias(CairoContext* context, int antialias);
int APS5_VABI cairo_get_antialias(CairoContext* context);
void APS5_VABI cairo_set_fill_rule(CairoContext* context, int fillRule);
int APS5_VABI cairo_get_fill_rule(CairoContext* context);
void APS5_VABI cairo_set_line_cap(CairoContext* context, int cap);
int APS5_VABI cairo_get_line_cap(CairoContext* context);
void APS5_VABI cairo_set_line_join(CairoContext* context, int join);
int APS5_VABI cairo_get_line_join(CairoContext* context);
void APS5_VABI cairo_set_line_width(CairoContext* context, double width);
double APS5_VABI cairo_get_line_width(CairoContext* context);
void APS5_VABI cairo_set_miter_limit(CairoContext* context, double limit);
double APS5_VABI cairo_get_miter_limit(CairoContext* context);
void APS5_VABI cairo_set_dash(CairoContext* context, const double* dashes,
                             int numDashes, double offset);
void APS5_VABI cairo_get_dash(CairoContext* context, double* dashes, double* offset);
int APS5_VABI cairo_get_dash_count(CairoContext* context);
void APS5_VABI cairo_set_operator(CairoContext* context, int operation);
int APS5_VABI cairo_get_operator(CairoContext* context);
void APS5_VABI cairo_rectangle(CairoContext* context, double x, double y,
                               double width, double height);
}
