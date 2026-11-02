#pragma once

#include "SceTypes.hpp"

struct CairoSurface;

extern "C" {

int APS5_VABI cairo_format_stride_for_width(int format, int width);

CairoSurface* APS5_VABI cairo_image_surface_create(int format, int width, int height);
CairoSurface* APS5_VABI cairo_image_surface_create_for_data(unsigned char* data, int format,
                                                            int width, int height, int stride);
unsigned char* APS5_VABI cairo_image_surface_get_data(CairoSurface* surface);
int APS5_VABI cairo_image_surface_get_format(CairoSurface* surface);
int APS5_VABI cairo_image_surface_get_width(CairoSurface* surface);
int APS5_VABI cairo_image_surface_get_height(CairoSurface* surface);
int APS5_VABI cairo_image_surface_get_stride(CairoSurface* surface);

CairoSurface* APS5_VABI cairo_surface_create_similar(CairoSurface* other, int content,
                                                     int width, int height);
CairoSurface* APS5_VABI cairo_surface_create_for_rectangle(CairoSurface* target,
                                                           double x, double y,
                                                           double width, double height);
void APS5_VABI cairo_surface_destroy(CairoSurface* surface);
void APS5_VABI cairo_surface_flush(CairoSurface* surface);
int APS5_VABI cairo_surface_get_content(CairoSurface* surface);
void APS5_VABI cairo_surface_get_device_scale(CairoSurface* surface,
                                              double* x_scale, double* y_scale);
int APS5_VABI cairo_surface_get_type(CairoSurface* surface);
void APS5_VABI cairo_surface_mark_dirty_rectangle(CairoSurface* surface,
                                                  int x, int y, int width, int height);
CairoSurface* APS5_VABI cairo_surface_reference(CairoSurface* surface);
void APS5_VABI cairo_surface_set_device_scale(CairoSurface* surface, double x_scale,
                                              double y_scale);
int APS5_VABI cairo_surface_status(CairoSurface* surface);

}
