#pragma once

#include "SceTypes.hpp"
#include <cstdint>

struct cairo_font_options_t;

struct cairo_glyph_t {
    std::uint64_t index;
    double x;
    double y;
};

enum cairo_antialias_t : int {
    CAIRO_ANTIALIAS_DEFAULT,
    CAIRO_ANTIALIAS_NONE,
    CAIRO_ANTIALIAS_GRAY,
    CAIRO_ANTIALIAS_SUBPIXEL,
    CAIRO_ANTIALIAS_FAST,
    CAIRO_ANTIALIAS_GOOD,
    CAIRO_ANTIALIAS_BEST,
};

enum cairo_subpixel_order_t : int {
    CAIRO_SUBPIXEL_ORDER_DEFAULT,
    CAIRO_SUBPIXEL_ORDER_RGB,
    CAIRO_SUBPIXEL_ORDER_BGR,
    CAIRO_SUBPIXEL_ORDER_VRGB,
    CAIRO_SUBPIXEL_ORDER_VBGR,
};

enum cairo_hint_style_t : int {
    CAIRO_HINT_STYLE_DEFAULT,
    CAIRO_HINT_STYLE_NONE,
    CAIRO_HINT_STYLE_SLIGHT,
    CAIRO_HINT_STYLE_MEDIUM,
    CAIRO_HINT_STYLE_FULL,
};

enum cairo_hint_metrics_t : int {
    CAIRO_HINT_METRICS_DEFAULT,
    CAIRO_HINT_METRICS_OFF,
    CAIRO_HINT_METRICS_ON,
};

struct CairoRegion;

struct CairoRectangleInt {
    int x;
    int y;
    int width;
    int height;
};

extern "C" {

int APS5_VABI cairo_scaled_font_get_font_face();
int APS5_VABI cairo_paint();
int APS5_VABI cairo_set_font_options();
int APS5_VABI cairo_font_face_set_user_data();
int APS5_VABI cairo_stroke_extents();
int APS5_VABI cairo_arc_negative();
int APS5_VABI cairo_image_surface_create_from_png_stream();
int APS5_VABI cairo_ft_scaled_font_lock_face();
int APS5_VABI cairo_glyph_path();
int APS5_VABI cairo_set_source_rgb();
int APS5_VABI cairo_scaled_font_destroy();
int APS5_VABI cairo_pattern_reference();
int APS5_VABI cairo_scaled_font_get_font_options();
int APS5_VABI cairo_in_stroke();
int APS5_VABI cairo_scaled_font_get_ctm();
int APS5_VABI cairo_set_font_size();
int APS5_VABI cairo_pattern_set_extend();
int APS5_VABI cairo_set_source_rgba();
int APS5_VABI cairo_ft_font_face_create_for_pattern();
int APS5_VABI cairo_mask();
int APS5_VABI cairo_scaled_font_extents();
int APS5_VABI cairo_show_glyphs();
int APS5_VABI cairo_mesh_pattern_curve_to();
int APS5_VABI cairo_scaled_font_get_font_matrix();
int APS5_VABI cairo_mesh_pattern_line_to();
int APS5_VABI cairo_font_face_destroy();
int APS5_VABI cairo_scaled_font_text_extents();
int APS5_VABI cairo_pattern_create_linear();
int APS5_VABI cairo_clip_extents();
int APS5_VABI cairo_set_scaled_font();
int APS5_VABI cairo_fill();
int APS5_VABI cairo_pop_group_to_source();
int APS5_VABI cairo_mesh_pattern_set_corner_color_rgba();
int APS5_VABI cairo_clip();
int APS5_VABI cairo_pattern_create_for_surface();
int APS5_VABI cairo_arc();
int APS5_VABI cairo_stroke_preserve();
int APS5_VABI cairo_ft_scaled_font_unlock_face();
int APS5_VABI cairo_mesh_pattern_move_to();
int APS5_VABI cairo_paint_with_alpha();
int APS5_VABI cairo_mesh_pattern_begin_patch();
int APS5_VABI cairo_mesh_pattern_end_patch();
int APS5_VABI cairo_scaled_font_status();
int APS5_VABI cairo_in_fill();
int APS5_VABI cairo_set_source();
int APS5_VABI cairo_font_face_get_user_data();
int APS5_VABI cairo_pattern_create_mesh();
int APS5_VABI cairo_stroke();
int APS5_VABI cairo_fill_preserve();
int APS5_VABI cairo_fill_extents();
int APS5_VABI cairo_ft_font_face_create_for_ft_face();
int APS5_VABI cairo_pattern_destroy();
int APS5_VABI cairo_pattern_set_matrix();
int APS5_VABI cairo_pattern_set_filter();
int APS5_VABI cairo_push_group();
int APS5_VABI cairo_ft_font_options_substitute();
int APS5_VABI cairo_select_font_face();
int APS5_VABI cairo_scaled_font_reference();
int APS5_VABI cairo_font_face_reference();
int APS5_VABI cairo_scaled_font_text_to_glyphs();
int APS5_VABI cairo_scaled_font_glyph_extents();
int APS5_VABI cairo_show_text();
int APS5_VABI cairo_set_font_face();
int APS5_VABI cairo_scaled_font_create();
int APS5_VABI cairo_pattern_create_radial();
int APS5_VABI cairo_set_source_surface();
int APS5_VABI cairo_device_destroy();
int APS5_VABI cairo_pattern_add_color_stop_rgba();

void APS5_VABI cairo_glyph_free(cairo_glyph_t* glyphs);

cairo_font_options_t* APS5_VABI cairo_font_options_create();
cairo_font_options_t* APS5_VABI cairo_font_options_copy(const cairo_font_options_t* original);
void APS5_VABI cairo_font_options_destroy(cairo_font_options_t* options);
void APS5_VABI cairo_font_options_set_antialias(cairo_font_options_t* options, cairo_antialias_t antialias);
cairo_antialias_t APS5_VABI cairo_font_options_get_antialias(const cairo_font_options_t* options);
void APS5_VABI cairo_font_options_set_hint_style(cairo_font_options_t* options, cairo_hint_style_t hint_style);
void APS5_VABI cairo_font_options_set_hint_metrics(cairo_font_options_t* options, cairo_hint_metrics_t hint_metrics);
void APS5_VABI cairo_font_options_set_subpixel_order(cairo_font_options_t* options, cairo_subpixel_order_t subpixel_order);

CairoRegion* APS5_VABI cairo_region_create();
CairoRegion* APS5_VABI cairo_region_reference(CairoRegion* region);
void APS5_VABI cairo_region_destroy(CairoRegion* region);
int APS5_VABI cairo_region_union_rectangle(CairoRegion* region, const CairoRectangleInt* rectangle);
void APS5_VABI cairo_region_get_rectangle(const CairoRegion* region, int nth, CairoRectangleInt* rectangle);
int APS5_VABI cairo_region_num_rectangles(const CairoRegion* region);

}
