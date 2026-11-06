# libcairo technical debt

### Functional

- [Cairo affine matrices](../../../core/libs/prx/libcairo/Matrix.cpp) - Ten pure affine operations implement the documented [Cairo matrix API](https://www.cairographics.org/manual/cairo-cairo-matrix-t.html), including multiplication with aliased outputs, singular inversion status and point/distance transforms. The 48-byte `cairo_matrix_t` layout and C signatures are public; the PS5 library revision and native behavior have not been measured. These operations do not depend on rendering or font functionality.

### Functional image and surface exports

- [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp) implements 19 Cairo image/surface/stride exports with caller-owned and internally-owned pixel buffers, reference-counted surfaces, format and stride handling, device scales, subsurfaces and error surfaces. CPU image flush/mark-dirty require no deferred device synchronization. Windows and Linux sanitizer tests and native Cairo 1.18.0 differential metadata checks passed; no PS5 console comparison or complete rasterizer is claimed.

### Functional font options

- [FontOptions.cpp](../../../core/libs/prx/libcairo/FontOptions.cpp) implements eight font-option lifecycle, antialias and hint-setting helpers with Cairo-compatible owned state and a protected nil object. Tested on Windows and Linux sanitizers.

### Functional context and coordinate transforms

- [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implements seventeen Cairo context lifecycle, nested matrix save/restore, and user/device coordinate transform exports, with an owned surface reference, a sticky invalid-matrix/invalid-restore state and System V guest ABI. Drawing and rasterization remain pending; this does not provide a functioning Cairo renderer. The context code depends on the image/surface lifecycles above and shares the affine matrix primitives.

- [Graphics state in Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implements fifteen getters/setters for antialias, fill rule, line cap, line join, line width, miter limit and owned dash arrays. Save/restore duplicates dash memory and restores these parameters alongside the CTM; error inputs set sticky INVALID_DASH. The stored state is observable but no rasterizer consumes it yet. Platform rendering and clipping remain unimplemented.

- [Path data in Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implements eleven path lifecycle and current-point exports with Cairo 24.8 device-coordinate quantization, target device scale, forward-collinear line canonicalization, move/line/cubic/close elements, deep-copied public cairo_path_t arrays, and guest ABI ownership. Append converts user paths through the current CTM and target device scale, while save/restore preserves the independent path. Rendering and path extents are not yet implemented; arc/rectangle constructors remain pending.

### Callable exports

- [Callable exports](../../../core/libs/prx/libcairo/Export.cpp) - 81 remaining callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libcairo.c) and [selected export metadata](https://gist.github.com/kyon-eth/a0bb48f2b3070a5eac8a638810f195f2/a3838ac54ebcc0bbfeb3cc46002a9a2dcce4dbdc) from two version-labelled images (SHA-256 `ec2158e0c5875316d200037d7f4bc48861626451752523f6930d687307c75ef8`). The labels are not authenticated. Prototypes and behavior of these remaining declarations are unverified; they still throw through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `cairo_scaled_font_get_font_face` | `+BL3-h8zGDo` |
| `cairo_paint` | `+PEo5Z7DOlY` |
| `cairo_set_font_options` | `-aqFIBn0h78` |
| `cairo_font_face_set_user_data` | `1GGpdjyZslI` |
| `cairo_stroke_extents` | `1K5ly63m6fo` |
| `cairo_arc_negative` | `2w5H3b3yPqw` |
| `cairo_image_surface_create_from_png_stream` | `4dvdsqyLM8c` |
| `cairo_ft_scaled_font_lock_face` | `4gDTaBZH8lo` |
| `cairo_glyph_path` | `54HrgUIQHOk` |
| `cairo_set_source_rgb` | `6DvK+OdIPW4` |
| `cairo_scaled_font_destroy` | `6Y3HHedGzlw` |
| `cairo_pattern_reference` | `6qRL52tLOII` |
| `cairo_scaled_font_get_font_options` | `8SOxQrQ7AfQ` |
| `cairo_in_stroke` | `8zMm0QOWZAA` |
| `cairo_scaled_font_get_ctm` | `A++pFuyxZPg` |
| `cairo_set_font_size` | `A-0StSi2AwA` |
| `cairo_surface_write_to_png_stream` | `A3FLVZ4FHbY` |
| `cairo_pattern_set_extend` | `Ax867+0HUgA` |
| `cairo_set_source_rgba` | `BZFvjfb9xDk` |
| `cairo_ft_font_face_create_for_pattern` | `By6b1vjSVok` |
| `cairo_mask` | `D1KYCZ80Sac` |
| `cairo_rectangle` | `E5LkpnAY7po` |
| `cairo_scaled_font_extents` | `FbWFbLUQOys` |
| `cairo_show_glyphs` | `FnhwGKKZOjU` |
| `cairo_mesh_pattern_curve_to` | `H9bwCsWw7LA` |
| `cairo_scaled_font_get_font_matrix` | `HKjYBSCx3mY` |
| `cairo_mesh_pattern_line_to` | `HTB1uUwgSVQ` |
| `cairo_region_get_rectangle` | `I5wa74016LQ` |
| `cairo_font_face_destroy` | `IgBFcd4cF28` |
| `cairo_scaled_font_text_extents` | `Jk9paxbMQsk` |
| `cairo_pattern_create_linear` | `K62jNcRGQdY` |
| `cairo_clip_extents` | `LJNqwiDWTm0` |
| `cairo_set_scaled_font` | `LUh-6rZhniA` |
| `cairo_region_create` | `MGMwq9DNMyE` |
| `cairo_fill` | `Ndvm8s28Gyc` |
| `cairo_pop_group_to_source` | `Oyzf6kDk6Ns` |
| `cairo_mesh_pattern_set_corner_color_rgba` | `P+Hm5enx01Y` |
| `cairo_clip` | `PkGZZi1G2Gw` |
| `cairo_pattern_create_for_surface` | `Q6xyruheFCE` |
| `cairo_arc` | `R0TePTSHd2k` |
| `cairo_stroke_preserve` | `RzM4ZhlKzPg` |
| `cairo_path_extents` | `S6ieIssLpCA` |
| `cairo_ft_scaled_font_unlock_face` | `SJMLSxaYvWU` |
| `cairo_mesh_pattern_move_to` | `T0JOV6EajrA` |
| `cairo_paint_with_alpha` | `TAIhaLjmAlg` |
| `cairo_mesh_pattern_begin_patch` | `Ux6XxakVN+8` |
| `cairo_mesh_pattern_end_patch` | `VBA3H7mYG70` |
| `cairo_region_union_rectangle` | `Ya+vgrJjBQE` |
| `cairo_region_reference` | `Yarxt1LHaf0` |
| `cairo_scaled_font_status` | `a5WkqZXnG2w` |
| `cairo_in_fill` | `akimK5gpdho` |
| `cairo_set_source` | `bU0QXekwNes` |
| `cairo_font_face_get_user_data` | `bXupa3+NYIs` |
| `cairo_pattern_create_mesh` | `bz5MfOeWZw4` |
| `cairo_stroke` | `cdr2T4SoIyU` |
| `cairo_set_operator` | `dc4bb-f5YEE` |
| `cairo_fill_preserve` | `dewyjTlk+u8` |
| `cairo_fill_extents` | `dkoQNd4kBlQ` |
| `cairo_ft_font_face_create_for_ft_face` | `dqEhIWRTjuo` |
| `cairo_pattern_destroy` | `eZpt6dzI9oo` |
| `cairo_pattern_set_matrix` | `fDNhBmmwSNs` |
| `cairo_surface_set_user_data` | `fFoWwCW2rDA` |
| `cairo_pattern_set_filter` | `gsqESe70WD0` |
| `cairo_push_group` | `hlh15JjNtWg` |
| `cairo_ft_font_options_substitute` | `ii895dyBjMo` |
| `cairo_glyph_free` | `lAEbTbSUTTo` |
| `cairo_select_font_face` | `ldk9m5h+-no` |
| `cairo_scaled_font_reference` | `mb4bdMeFRs8` |
| `cairo_font_face_reference` | `nqmUW6Z8xLE` |
| `cairo_scaled_font_text_to_glyphs` | `ojbXRC0CmUo` |
| `cairo_scaled_font_glyph_extents` | `oy7NcAkex1I` |
| `cairo_show_text` | `psdqLsaPnkk` |
| `cairo_set_font_face` | `qzEwpfbLVxI` |
| `cairo_get_operator` | `tYTJIi4s6t0` |
| `cairo_scaled_font_create` | `uKNPDudJTmo` |
| `cairo_pattern_create_radial` | `uYFjM3ReDRQ` |
| `cairo_set_source_surface` | `vcN1gg+gWRg` |
| `cairo_region_destroy` | `vnHq+jcDKBM` |
| `cairo_device_destroy` | `wO1D4B90k0k` |
| `cairo_pattern_add_color_stop_rgba` | `xXWrbaaPNNg` |
| `cairo_region_num_rectangles` | `xoe4ss9GmYc` |
