# libcairo technical debt

### Functional

- [Matrix.cpp](../../../core/libs/prx/libcairo/Matrix.cpp) and [Region.cpp](../../../core/libs/prx/libcairo/Region.cpp) - console behaviour is unmeasured, and the backend differs from the console's in two known ways. PS5 system software 9.00 and later carries Cairo 1.18.0 with three Sony patches ([source](https://www.playstation.com/en-us/oss/ps5/cairo/)) that are not applied here; the one that changes behaviour adds the public status `CAIRO_STATUS_NULL_SURFACE_IMAGE`, which no switched export reaches yet. Sony publishes no pixman source, so the console's pixman version is unknown. The ten matrix and six region exports run [Cairo 1.18.0](https://gitlab.freedesktop.org/cairo/cairo/-/tree/1.18.0) and [pixman 0.42.2](https://gitlab.freedesktop.org/pixman/pixman/-/tree/pixman-0.42.2) from `3rdparty/cairo` and `3rdparty/pixman`, built with the image backend only; 0.42.2 was the newest pixman when Cairo 1.18.0 came out.

- [Region.cpp](../../../core/libs/prx/libcairo/Region.cpp) - a null region or rectangle, which Cairo dereferences, returns `CAIRO_STATUS_NULL_POINTER` from `cairo_region_union_rectangle`, 0 from `cairo_region_num_rectangles` and leaves `cairo_region_get_rectangle` without effect; an `nth` outside the region, which Cairo reads past the array for, returns an empty rectangle. A rectangle with a negative size or a far edge past 32 bits is ignored, and pixman prints an error to stderr. A box wider or taller than `INT32_MAX` overflows a signed subtraction in `cairo_region_get_rectangle`; the result is undefined.

### Functional image and surface exports

- [Surface.cpp](../../../core/libs/prx/libcairo/Surface.cpp) implements 20 Cairo image/surface/stride exports with caller-owned and internally-owned pixel buffers, reference-counted surfaces, format and stride handling, device scales, subsurfaces and error surfaces. CPU image flush/mark-dirty require no deferred device synchronization. The original 19 exports passed Windows and Linux sanitizer tests and native Cairo 1.18.0 differential metadata checks. Surface user-data tests pass on Linux; the callback has no Windows run. No PS5 console comparison or complete rasterizer is claimed.

- The PS5 library revision and [surface user-data](../../../core/libs/prx/libcairo/Surface.cpp) callback behavior remain unmeasured. A null surface pointer still throws because its contract is unknown. Surface user-data ownership follows [Cairo 1.18.0](https://cairographics.org/releases/cairo-1.18.0.tar.xz), `src/cairo-surface.c` and `src/cairo-array.c`. Arbitrary recursive slot replacement or growth from callbacks is not established.

- [PNG stream writing](../../../core/libs/prx/libcairo/Png.cpp) follows Cairo 1.18.0 image formats, subsurface clipping and callback status propagation. The shared encoder buffers the PNG before delivering chunks. Compression and callback chunk sizes can differ from native Cairo, and the encoder's size limit is reported as `CAIRO_STATUS_NO_MEMORY`. Null pointers, unreadable pixels and nonfinite subsurface coordinates have unverified behavior and throw. PS5 behavior remains unmeasured.

- [PNG stream reading](../../../core/libs/prx/libcairo/Png.cpp) uses the shared PNG decoder to create Cairo image surfaces with premultiplied alpha and floating-point formats for 16-bit PNGs. The adapter buffers input through IEND, checks CRCs and reports read errors from the callback. The decoder's malformed-image behavior and callback read sizes can differ from libpng. Original PNG MIME data is unavailable because MIME-data access is not implemented. A null callback throws, and the console version and behavior remain unmeasured.

### Functional font options

- [FontOptions.cpp](../../../core/libs/prx/libcairo/FontOptions.cpp) implements eight font-option lifecycle, antialias and hint-setting helpers with Cairo-compatible owned state and a protected nil object. Tested on Windows and Linux sanitizers.

### Functional context and coordinate transforms

- [Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implements seventeen Cairo context lifecycle, nested matrix save/restore, and user/device coordinate transform exports, with an owned surface reference, a sticky invalid-matrix/invalid-restore state and System V guest ABI. Drawing, clipping and rasterization remain pending. The context code depends on the image/surface lifecycles above and shares the affine matrix primitives.

- [Path data in Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) implements eleven path lifecycle and current-point exports with Cairo 24.8 device-coordinate quantization, target device scale, forward-collinear line canonicalization, move/line/cubic/close elements, deep-copied public cairo_path_t arrays, and guest ABI ownership. Append converts user paths through the current CTM and target device scale, while save/restore preserves the independent path. Rectangles use quantized device displacements and close-path canonicalization. Path extents bound line segments and quantized cubic extrema in device space, then convert to user coordinates. Arc construction and rendering remain unsupported, and console behavior is unmeasured.

- [Operator state in Context.cpp](../../../core/libs/prx/libcairo/Context.cpp) has no rendering consumer. The default is OVER, and save/restore preserves the stored operator. Native Cairo 1.18.0 is the comparison target; console behavior is unmeasured.

- [`cairo_in_fill`](../../../core/libs/prx/libcairo/InFill.cpp) throws through `NotImplemented_nid_no_patch` for a query whose device coordinates are not finite and, off the edges of a non-empty fill, for a fill rule other than winding and even-odd: Cairo 1.18.0 skips the query's transform while it tracks its matrices as identity, a flag the context does not keep, and reaches `ASSERT_NOT_REACHED` on such a rule. The rest is ported from `cairo-path-in-fill.c`, `cairo-spline.c` and `_cairo_fixed_from_double` of Cairo 1.18.0 and not compared with a native Cairo build: the query becomes 24.8 fixed point unclamped, so it wraps every 16777216 device units, curves are flattened at the default tolerance 0.1, as the catalogue has no `cairo_set_tolerance`, and a curve whose control points coincide with its ends is dropped without moving the current point, unless the query is above, below or left of its bounding box. The PS5 library revision is unmeasured.

### Functional glyphs

- [Glyph.cpp](../../../core/libs/prx/libcairo/Glyph.cpp) implements `cairo_glyph_free` for null, which Cairo ignores. No export here allocates glyph arrays yet, and `cairo_glyph_allocate` is not in the catalogue, so a non-null array comes from outside this library and still throws through `NotImplemented_nid_no_patch` instead of reaching `free`. Freeing must follow the allocator once `cairo_scaled_font_text_to_glyphs` returns arrays. Unverified on a console.

### Callable exports

- [Callable exports](../../../core/libs/prx/libcairo/Export.cpp) - The remaining callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libcairo.c) and [selected export metadata](https://gist.github.com/kyon-eth/a0bb48f2b3070a5eac8a638810f195f2/a3838ac54ebcc0bbfeb3cc46002a9a2dcce4dbdc) from two version-labelled images (SHA-256 `ec2158e0c5875316d200037d7f4bc48861626451752523f6930d687307c75ef8`). The labels are not authenticated. Prototypes and behavior of these remaining declarations are unverified; they still throw through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `cairo_scaled_font_get_font_face` | `+BL3-h8zGDo` |
| `cairo_paint` | `+PEo5Z7DOlY` |
| `cairo_set_font_options` | `-aqFIBn0h78` |
| `cairo_font_face_set_user_data` | `1GGpdjyZslI` |
| `cairo_stroke_extents` | `1K5ly63m6fo` |
| `cairo_arc_negative` | `2w5H3b3yPqw` |
| `cairo_ft_scaled_font_lock_face` | `4gDTaBZH8lo` |
| `cairo_glyph_path` | `54HrgUIQHOk` |
| `cairo_set_source_rgb` | `6DvK+OdIPW4` |
| `cairo_scaled_font_destroy` | `6Y3HHedGzlw` |
| `cairo_pattern_reference` | `6qRL52tLOII` |
| `cairo_scaled_font_get_font_options` | `8SOxQrQ7AfQ` |
| `cairo_in_stroke` | `8zMm0QOWZAA` |
| `cairo_scaled_font_get_ctm` | `A++pFuyxZPg` |
| `cairo_set_font_size` | `A-0StSi2AwA` |
| `cairo_pattern_set_extend` | `Ax867+0HUgA` |
| `cairo_set_source_rgba` | `BZFvjfb9xDk` |
| `cairo_ft_font_face_create_for_pattern` | `By6b1vjSVok` |
| `cairo_mask` | `D1KYCZ80Sac` |
| `cairo_scaled_font_extents` | `FbWFbLUQOys` |
| `cairo_show_glyphs` | `FnhwGKKZOjU` |
| `cairo_mesh_pattern_curve_to` | `H9bwCsWw7LA` |
| `cairo_scaled_font_get_font_matrix` | `HKjYBSCx3mY` |
| `cairo_mesh_pattern_line_to` | `HTB1uUwgSVQ` |
| `cairo_font_face_destroy` | `IgBFcd4cF28` |
| `cairo_scaled_font_text_extents` | `Jk9paxbMQsk` |
| `cairo_pattern_create_linear` | `K62jNcRGQdY` |
| `cairo_clip_extents` | `LJNqwiDWTm0` |
| `cairo_set_scaled_font` | `LUh-6rZhniA` |
| `cairo_fill` | `Ndvm8s28Gyc` |
| `cairo_pop_group_to_source` | `Oyzf6kDk6Ns` |
| `cairo_mesh_pattern_set_corner_color_rgba` | `P+Hm5enx01Y` |
| `cairo_clip` | `PkGZZi1G2Gw` |
| `cairo_pattern_create_for_surface` | `Q6xyruheFCE` |
| `cairo_arc` | `R0TePTSHd2k` |
| `cairo_stroke_preserve` | `RzM4ZhlKzPg` |
| `cairo_ft_scaled_font_unlock_face` | `SJMLSxaYvWU` |
| `cairo_mesh_pattern_move_to` | `T0JOV6EajrA` |
| `cairo_paint_with_alpha` | `TAIhaLjmAlg` |
| `cairo_mesh_pattern_begin_patch` | `Ux6XxakVN+8` |
| `cairo_mesh_pattern_end_patch` | `VBA3H7mYG70` |
| `cairo_scaled_font_status` | `a5WkqZXnG2w` |
| `cairo_set_source` | `bU0QXekwNes` |
| `cairo_font_face_get_user_data` | `bXupa3+NYIs` |
| `cairo_pattern_create_mesh` | `bz5MfOeWZw4` |
| `cairo_stroke` | `cdr2T4SoIyU` |
| `cairo_fill_preserve` | `dewyjTlk+u8` |
| `cairo_fill_extents` | `dkoQNd4kBlQ` |
| `cairo_ft_font_face_create_for_ft_face` | `dqEhIWRTjuo` |
| `cairo_pattern_destroy` | `eZpt6dzI9oo` |
| `cairo_pattern_set_matrix` | `fDNhBmmwSNs` |
| `cairo_pattern_set_filter` | `gsqESe70WD0` |
| `cairo_push_group` | `hlh15JjNtWg` |
| `cairo_ft_font_options_substitute` | `ii895dyBjMo` |
| `cairo_select_font_face` | `ldk9m5h+-no` |
| `cairo_scaled_font_reference` | `mb4bdMeFRs8` |
| `cairo_font_face_reference` | `nqmUW6Z8xLE` |
| `cairo_scaled_font_text_to_glyphs` | `ojbXRC0CmUo` |
| `cairo_scaled_font_glyph_extents` | `oy7NcAkex1I` |
| `cairo_show_text` | `psdqLsaPnkk` |
| `cairo_set_font_face` | `qzEwpfbLVxI` |
| `cairo_scaled_font_create` | `uKNPDudJTmo` |
| `cairo_pattern_create_radial` | `uYFjM3ReDRQ` |
| `cairo_set_source_surface` | `vcN1gg+gWRg` |
| `cairo_device_destroy` | `wO1D4B90k0k` |
| `cairo_pattern_add_color_stop_rgba` | `xXWrbaaPNNg` |
