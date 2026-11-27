# libcairo

## add Functional

- [Pattern.cpp](../../../core/libs/prx/libcairo/Pattern.cpp) - console behaviour of `cairo_pattern_create_linear`, `cairo_pattern_create_radial`, `cairo_pattern_reference`, `cairo_pattern_destroy`, `cairo_pattern_add_color_stop_rgba`, `cairo_pattern_set_matrix`, `cairo_pattern_set_filter` and `cairo_pattern_set_extend` is unmeasured. They forward to the bundled Cairo 1.18.0, which clamps stop offsets and colours to [0, 1], puts a pattern in `CAIRO_STATUS_INVALID_MATRIX` for a non-invertible matrix and stores extend and filter values without a range check. Null or foreign pattern pointers reach Cairo unchanged. An unreadable matrix pointer throws through `NotImplemented_nid_no_patch`, because the console's response to it is unknown.

## change [SurfacePattern.cpp]

- [SurfacePattern.cpp](../../../core/libs/prx/libcairo/SurfacePattern.cpp) - console behaviour of `cairo_pattern_create_for_surface`, `cairo_set_source` and `cairo_mask` is unmeasured. They forward to the bundled Cairo 1.18.0: a null or failed surface gives a pattern in that error, and a null or failed pattern puts the context in that error and draws nothing; a null context reaches Cairo unchanged. Sony's `CAIRO_STATUS_NULL_SURFACE_IMAGE` patch changes only the span renderer for a solid mask, when pixman cannot create a surface source's image. Cairo draws a mask it treats as opaque, such as an opaque surface pattern with `CAIRO_EXTEND_REPEAT`, `CAIRO_EXTEND_REFLECT` or `CAIRO_EXTEND_PAD` or an opaque gradient, as a paint, so `cairo_mask` with a surface source reaches that renderer as `cairo_paint` does and differs from the console only when that allocation fails.

## change implements `cairo_set_source_rgb`

- [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implements `cairo_set_source_rgb`, `cairo_set_source_rgba`, `cairo_set_source_surface`, `cairo_paint`, `cairo_paint_with_alpha`, `cairo_fill`, `cairo_fill_preserve`, `cairo_fill_extents`, `cairo_stroke`, `cairo_stroke_preserve`, `cairo_stroke_extents`, `cairo_in_stroke`, `cairo_clip`, `cairo_clip_extents`, `cairo_push_group`, `cairo_pop_group_to_source`, `cairo_arc` and `cairo_arc_negative` by forwarding to Cairo 1.18.0. Pixels come from this build's pixman, so antialiased edges may differ from the console if its pixman differs. Arc paths are split into Bézier curves by Cairo's tolerance, so their control points match the console only if its Cairo splits them the same way. Unverified on a console.

## remove `2w5H3b3yPqw`

## remove `6qRL52tLOII`

## remove `Ax867+0HUgA`

## remove `K62jNcRGQdY`

## remove `R0TePTSHd2k`

## remove `eZpt6dzI9oo`

## remove `fDNhBmmwSNs`

## remove `gsqESe70WD0`

## remove `uYFjM3ReDRQ`

## remove `xXWrbaaPNNg`
