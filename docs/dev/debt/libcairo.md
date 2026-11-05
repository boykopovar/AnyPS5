# libcairo technical debt

### Functional

- [Mesh pattern construction](../../../core/libs/prx/libcairo/Patterns.cpp) implements seven source-backed construction exports for 4×4 tensor-product patches, including cubic and straight sides, implicit closing sides, clamped corner RGBA and Coons interior controls. The types and guest System V signatures follow the public Cairo interface, and completed patches and sticky error statuses were differential-checked with native Cairo 1.18.0. The existing gradient-pattern storage and reference counting are reused. The public PS5 image has not been measured; Cairo rendering and non-exported mesh getter APIs are not implemented. Native Cairo 1.18.0 can expose uninitialized corner-zero data after automatic closure if the caller omitted that color, whereas this implementation initializes untouched color storage to transparent black.

- [Pattern semantics](../../../core/libs/prx/libcairo/Patterns.cpp) follow the public [Cairo pattern source](https://chromium.googlesource.com/chromiumos/third_party/cairo/+/master/src/cairo-pattern.c): refcounted lifetime, a shared immortal error pattern with `CAIRO_STATUS_NO_MEMORY` when allocation fails, `CAIRO_STATUS_PATTERN_TYPE_MISMATCH` for color stops on a non-gradient pattern and the `cairo_matrix_invert` status for a non-invertible matrix. The prototypes are the public Cairo ones and are unverified against the PS5 image. `extend` and `filter` values are stored without validation as upstream does. No status or getter export for patterns is in the catalogue, so the stored state is only observable through the reference-counted object.

- [Cairo affine matrices](../../../core/libs/prx/libcairo/Matrix.cpp) - Ten pure affine operations implement the documented [Cairo matrix API](https://www.cairographics.org/manual/cairo-cairo-matrix-t.html), including multiplication with aliased outputs, singular inversion status and point/distance transforms. The 48-byte `cairo_matrix_t` layout and C signatures are public; the PS5 library revision and native behavior have not been measured.

### Functional image and surface exports

- [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp) implements 19 Cairo image/surface/stride exports with caller-owned and internally-owned pixel buffers, reference-counted surfaces, format and stride handling, device scales, subsurfaces and error surfaces. CPU image flush/mark-dirty require no deferred device synchronization. Windows and Linux sanitizer tests and native Cairo 1.18.0 differential metadata checks passed; no PS5 console comparison or complete rasterizer is claimed.

### Callable exports

- [Callable exports](../../../core/libs/prx/libcairo/Export.cpp) - 117 remaining callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libcairo.c) and [selected export metadata](https://gist.github.com/kyon-eth/a0bb48f2b3070a5eac8a638810f195f2/a3838ac54ebcc0bbfeb3cc46002a9a2dcce4dbdc) from two version-labelled images (SHA-256 `ec2158e0c5875316d200037d7f4bc48861626451752523f6930d687307c75ef8`). The labels are not authenticated. Fifteen pattern exports are implemented in [Patterns.cpp](../../../core/libs/prx/libcairo/Patterns.cpp), and nineteen surface exports are implemented in [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp). Prototypes and behavior of the remaining declarations are unverified; they still throw through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `cairo_scaled_font_get_font_face` | `+BL3-h8zGDo` |
| `cairo_paint` | `+PEo5Z7DOlY` |
| `cairo_font_options_set_hint_metrics` | `+TktRPDfw0Y` |
| `cairo_set_font_options` | `-aqFIBn0h78` |
| `cairo_device_to_user_distance` | `-v5CZVGkN3k` |
| `cairo_font_face_set_user_data` | `1GGpdjyZslI` |
| `cairo_stroke_extents` | `1K5ly63m6fo` |
| `cairo_arc_negative` | `2w5H3b3yPqw` |
| `cairo_new_sub_path` | `36Wm5aIJUFY` |
| `cairo_user_to_device` | `3grQ1jCgta8` |
| `cairo_reference` | `3pmBdB-bl8w` |
| `cairo_set_antialias` | `3urTbaMTNyE` |
| `cairo_rotate` | `4FH5hyIXo9k` |
| `cairo_image_surface_create_from_png_stream` | `4dvdsqyLM8c` |
| `cairo_get_fill_rule` | `4e7O-m-JWVM` |
| `cairo_ft_scaled_font_lock_face` | `4gDTaBZH8lo` |
| `cairo_glyph_path` | `54HrgUIQHOk` |
| `cairo_get_dash` | `5OZ1YwHyHd8` |
| `cairo_set_source_rgb` | `6DvK+OdIPW4` |
| `cairo_scaled_font_destroy` | `6Y3HHedGzlw` |
| `cairo_scaled_font_get_font_options` | `8SOxQrQ7AfQ` |
| `cairo_in_stroke` | `8zMm0QOWZAA` |
| `cairo_scaled_font_get_ctm` | `A++pFuyxZPg` |
| `cairo_set_font_size` | `A-0StSi2AwA` |
| `cairo_surface_write_to_png_stream` | `A3FLVZ4FHbY` |
| `cairo_restore` | `AoAG2JE8GIo` |
| `cairo_set_source_rgba` | `BZFvjfb9xDk` |
| `cairo_ft_font_face_create_for_pattern` | `By6b1vjSVok` |
| `cairo_mask` | `D1KYCZ80Sac` |
| `cairo_get_line_width` | `D45IcsszyQI` |
| `cairo_curve_to` | `DOCnSv1cXx8` |
| `cairo_copy_path` | `E2E+DDLhCRg` |
| `cairo_rectangle` | `E5LkpnAY7po` |
| `cairo_get_matrix` | `EItmXRLen7w` |
| `cairo_user_to_device_distance` | `F2ZShEhLU8Y` |
| `cairo_scaled_font_extents` | `FbWFbLUQOys` |
| `cairo_show_glyphs` | `FnhwGKKZOjU` |
| `cairo_scaled_font_get_font_matrix` | `HKjYBSCx3mY` |
| `cairo_get_line_cap` | `Hkp7M+pJILk` |
| `cairo_region_get_rectangle` | `I5wa74016LQ` |
| `cairo_font_options_set_hint_style` | `IDmch1bKy34` |
| `cairo_set_miter_limit` | `IKnEk6g6ZV0` |
| `cairo_translate` | `IdKE2DrNBck` |
| `cairo_font_face_destroy` | `IgBFcd4cF28` |
| `cairo_scaled_font_text_extents` | `Jk9paxbMQsk` |
| `cairo_has_current_point` | `JwQxC4dA5DY` |
| `cairo_destroy` | `K6aayzvQMlE` |
| `cairo_clip_extents` | `LJNqwiDWTm0` |
| `cairo_set_scaled_font` | `LUh-6rZhniA` |
| `cairo_region_create` | `MGMwq9DNMyE` |
| `cairo_fill` | `Ndvm8s28Gyc` |
| `cairo_transform` | `OHgp1uE8P2c` |
| `cairo_set_line_join` | `Ori3CK8Sx6g` |
| `cairo_pop_group_to_source` | `Oyzf6kDk6Ns` |
| `cairo_font_options_copy` | `P2IjaGD66NE` |
| `cairo_identity_matrix` | `PaSEtyxiC0o` |
| `cairo_save` | `Pj8asGd55jA` |
| `cairo_clip` | `PkGZZi1G2Gw` |
| `cairo_pattern_create_for_surface` | `Q6xyruheFCE` |
| `cairo_arc` | `R0TePTSHd2k` |
| `cairo_stroke_preserve` | `RzM4ZhlKzPg` |
| `cairo_path_extents` | `S6ieIssLpCA` |
| `cairo_ft_scaled_font_unlock_face` | `SJMLSxaYvWU` |
| `cairo_font_options_get_antialias` | `SNSLa0ggfpw` |
| `cairo_paint_with_alpha` | `TAIhaLjmAlg` |
| `cairo_set_fill_rule` | `THcHgZElshs` |
| `cairo_get_miter_limit` | `TWtGBP-grBM` |
| `cairo_get_antialias` | `U4Ajvh3L57k` |
| `cairo_get_target` | `Wr3nsor1aNc` |
| `cairo_append_path` | `XX9tocaUX+g` |
| `cairo_region_union_rectangle` | `Ya+vgrJjBQE` |
| `cairo_region_reference` | `Yarxt1LHaf0` |
| `cairo_scaled_font_status` | `a5WkqZXnG2w` |
| `cairo_in_fill` | `akimK5gpdho` |
| `cairo_get_line_join` | `azNGOQLubLc` |
| `cairo_set_source` | `bU0QXekwNes` |
| `cairo_font_face_get_user_data` | `bXupa3+NYIs` |
| `cairo_path_destroy` | `cOkxYTuZQyU` |
| `cairo_stroke` | `cdr2T4SoIyU` |
| `cairo_set_operator` | `dc4bb-f5YEE` |
| `cairo_fill_preserve` | `dewyjTlk+u8` |
| `cairo_fill_extents` | `dkoQNd4kBlQ` |
| `cairo_ft_font_face_create_for_ft_face` | `dqEhIWRTjuo` |
| `cairo_get_dash_count` | `eSwGXlKHIas` |
| `cairo_surface_set_user_data` | `fFoWwCW2rDA` |
| `cairo_set_dash` | `fx5nS1yOp-I` |
| `cairo_new_path` | `hhuvB3JXuMM` |
| `cairo_push_group` | `hlh15JjNtWg` |
| `cairo_font_options_destroy` | `huXPyr6A9to` |
| `cairo_ft_font_options_substitute` | `ii895dyBjMo` |
| `cairo_font_options_create` | `j2xKmRzIV84` |
| `cairo_device_to_user` | `jyf32aEUoMs` |
| `cairo_font_options_set_antialias` | `kNua19l9-0I` |
| `cairo_glyph_free` | `lAEbTbSUTTo` |
| `cairo_select_font_face` | `ldk9m5h+-no` |
| `cairo_line_to` | `mSda1B-vfkM` |
| `cairo_scaled_font_reference` | `mb4bdMeFRs8` |
| `cairo_set_matrix` | `niIR3TU71Bg` |
| `cairo_font_face_reference` | `nqmUW6Z8xLE` |
| `cairo_set_line_cap` | `oFltTuV0f7Y` |
| `cairo_scaled_font_text_to_glyphs` | `ojbXRC0CmUo` |
| `cairo_scaled_font_glyph_extents` | `oy7NcAkex1I` |
| `cairo_font_options_set_subpixel_order` | `pk6gDcQRKxs` |
| `cairo_show_text` | `psdqLsaPnkk` |
| `cairo_set_font_face` | `qzEwpfbLVxI` |
| `cairo_close_path` | `rENdf2qgOE4` |
| `cairo_create` | `sfmawT-nIUc` |
| `cairo_get_current_point` | `sinmW4Wm6OQ` |
| `cairo_move_to` | `smpeN8iWrcc` |
| `cairo_scale` | `suww6V0qqpU` |
| `cairo_get_operator` | `tYTJIi4s6t0` |
| `cairo_scaled_font_create` | `uKNPDudJTmo` |
| `cairo_set_source_surface` | `vcN1gg+gWRg` |
| `cairo_region_destroy` | `vnHq+jcDKBM` |
| `cairo_device_destroy` | `wO1D4B90k0k` |
| `cairo_region_num_rectangles` | `xoe4ss9GmYc` |
| `cairo_set_line_width` | `zwMGP3ASKMI` |
