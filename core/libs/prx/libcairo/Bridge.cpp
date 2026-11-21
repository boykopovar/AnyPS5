#include "prx/libcairo/include/Bridge.hpp"

extern "C" {

CairoSurface* CairoImageSurfaceCreate_nid_no_patch(int format, int width, int height) {
    return cairo_image_surface_create(format, width, height);
}

CairoContext* CairoCreate_nid_no_patch(CairoSurface* target) {
    return cairo_create(target);
}

void CairoDestroy_nid_no_patch(CairoContext* context) {
    cairo_destroy(context);
}

void CairoSave_nid_no_patch(CairoContext* context) {
    cairo_save(context);
}

void CairoRestore_nid_no_patch(CairoContext* context) {
    cairo_restore(context);
}

void CairoTranslate_nid_no_patch(CairoContext* context, double tx, double ty) {
    cairo_translate(context, tx, ty);
}

void CairoScale_nid_no_patch(CairoContext* context, double sx, double sy) {
    cairo_scale(context, sx, sy);
}

void CairoSetMatrix_nid_no_patch(CairoContext* context, const CairoMatrix* matrix) {
    cairo_set_matrix(context, matrix);
}

void CairoGetMatrix_nid_no_patch(CairoContext* context, CairoMatrix* matrix) {
    cairo_get_matrix(context, matrix);
}

void CairoIdentityMatrix_nid_no_patch(CairoContext* context) {
    cairo_identity_matrix(context);
}

void CairoSetAntialias_nid_no_patch(CairoContext* context, int antialias) {
    cairo_set_antialias(context, antialias);
}

void CairoSetLineWidth_nid_no_patch(CairoContext* context, double width) {
    cairo_set_line_width(context, width);
}

void CairoSetOperator_nid_no_patch(CairoContext* context, int operation) {
    cairo_set_operator(context, operation);
}

void CairoSetSourceRgba_nid_no_patch(CairoContext* context, double red, double green, double blue, double alpha) {
    cairo_set_source_rgba(context, red, green, blue, alpha);
}

void CairoSetSourceSurface_nid_no_patch(CairoContext* context, CairoSurface* surface, double x, double y) {
    cairo_set_source_surface(context, surface, x, y);
}

void CairoRectangle_nid_no_patch(CairoContext* context, double x, double y, double width, double height) {
    cairo_rectangle(context, x, y, width, height);
}

void CairoNewPath_nid_no_patch(CairoContext* context) {
    cairo_new_path(context);
}

void CairoMoveTo_nid_no_patch(CairoContext* context, double x, double y) {
    cairo_move_to(context, x, y);
}

void CairoLineTo_nid_no_patch(CairoContext* context, double x, double y) {
    cairo_line_to(context, x, y);
}

void CairoCurveTo_nid_no_patch(CairoContext* context, double x1, double y1, double x2, double y2, double x3, double y3) {
    cairo_curve_to(context, x1, y1, x2, y2, x3, y3);
}

void CairoClosePath_nid_no_patch(CairoContext* context) {
    cairo_close_path(context);
}

int CairoHasCurrentPoint_nid_no_patch(CairoContext* context) {
    return cairo_has_current_point(context);
}

void CairoGetCurrentPoint_nid_no_patch(CairoContext* context, double* x, double* y) {
    cairo_get_current_point(context, x, y);
}

CairoPath* CairoCopyPath_nid_no_patch(CairoContext* context) {
    return cairo_copy_path(context);
}

void CairoPathExtents_nid_no_patch(CairoContext* context, double* x1, double* y1, double* x2, double* y2) {
    cairo_path_extents(context, x1, y1, x2, y2);
}

void CairoAppendPath_nid_no_patch(CairoContext* context, const CairoPath* path) {
    cairo_append_path(context, path);
}

void CairoPathDestroy_nid_no_patch(CairoPath* path) {
    cairo_path_destroy(path);
}

void CairoFill_nid_no_patch(CairoContext* context) {
    cairo_fill(context);
}

void CairoStroke_nid_no_patch(CairoContext* context) {
    cairo_stroke(context);
}

void CairoClip_nid_no_patch(CairoContext* context) {
    cairo_clip(context);
}

}
