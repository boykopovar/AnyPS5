#pragma once

#include "prx/libcairo/include/Context.hpp"
#include "prx/libcairo/include/Matrix.hpp"
#include "prx/libcairo/include/Path.hpp"
#include "prx/libcairo/include/Surface.hpp"

extern "C" {

CairoSurface* CairoImageSurfaceCreate_nid_no_patch(int format, int width, int height);
CairoContext* CairoCreate_nid_no_patch(CairoSurface* target);
void CairoDestroy_nid_no_patch(CairoContext* context);
void CairoSave_nid_no_patch(CairoContext* context);
void CairoRestore_nid_no_patch(CairoContext* context);
void CairoTranslate_nid_no_patch(CairoContext* context, double tx, double ty);
void CairoScale_nid_no_patch(CairoContext* context, double sx, double sy);
void CairoSetMatrix_nid_no_patch(CairoContext* context, const CairoMatrix* matrix);
void CairoGetMatrix_nid_no_patch(CairoContext* context, CairoMatrix* matrix);
void CairoIdentityMatrix_nid_no_patch(CairoContext* context);
void CairoSetAntialias_nid_no_patch(CairoContext* context, int antialias);
void CairoSetLineWidth_nid_no_patch(CairoContext* context, double width);
void CairoSetOperator_nid_no_patch(CairoContext* context, int operation);
void CairoSetSourceRgba_nid_no_patch(CairoContext* context, double red, double green, double blue, double alpha);
void CairoSetSourceSurface_nid_no_patch(CairoContext* context, CairoSurface* surface, double x, double y);
void CairoRectangle_nid_no_patch(CairoContext* context, double x, double y, double width, double height);
void CairoNewPath_nid_no_patch(CairoContext* context);
void CairoMoveTo_nid_no_patch(CairoContext* context, double x, double y);
void CairoLineTo_nid_no_patch(CairoContext* context, double x, double y);
void CairoCurveTo_nid_no_patch(CairoContext* context, double x1, double y1, double x2, double y2, double x3, double y3);
void CairoClosePath_nid_no_patch(CairoContext* context);
int CairoHasCurrentPoint_nid_no_patch(CairoContext* context);
void CairoGetCurrentPoint_nid_no_patch(CairoContext* context, double* x, double* y);
CairoPath* CairoCopyPath_nid_no_patch(CairoContext* context);
void CairoPathExtents_nid_no_patch(CairoContext* context, double* x1, double* y1, double* x2, double* y2);
void CairoAppendPath_nid_no_patch(CairoContext* context, const CairoPath* path);
void CairoPathDestroy_nid_no_patch(CairoPath* path);
void CairoFill_nid_no_patch(CairoContext* context);
void CairoStroke_nid_no_patch(CairoContext* context);
void CairoClip_nid_no_patch(CairoContext* context);
void CairoPushGroup_nid_no_patch(CairoContext* context);
void CairoPopGroupToSource_nid_no_patch(CairoContext* context);
void CairoPaintWithAlpha_nid_no_patch(CairoContext* context, double alpha);

}
