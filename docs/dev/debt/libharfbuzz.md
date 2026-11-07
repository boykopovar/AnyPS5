# libharfbuzz technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libharfbuzz/Export.cpp) - The remaining 24 throwing callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libharfbuzz.c) and the [selected export metadata](https://gist.github.com/kyon-eth/510aa3fcaf09cbe8b52048b44e4e67c0/b909f17c99ecd6323c085f70064548789062d57e) from an older `libharfbuzz` image and a later `libSceVshHarfBuzz` image (SHA-256 `25af97e02c1fa8d318ff3697eb8a79057ee9b8c5e5e4ec321f25114aceb05628`). The later image has a different module and library identity; matching NIDs do not establish forwarding or equivalent behavior. The version labels are not authenticated. Their prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch`.

| Name | NID |
| --- | --- |
| `hb_blob_create` | `hj1KIIN5D54` |
| `hb_face_create_for_tables` | `l7GEYs5PMVU` |
| `hb_face_destroy` | `ULNHQXciJ3s` |
| `hb_font_create` | `FVhYusaWqA4` |
| `hb_font_destroy` | `-8t2aEtFws4` |
| `hb_font_funcs_create` | `8ZddTpSAKIg` |
| `hb_font_funcs_make_immutable` | `LyOypNLMoz0` |
| `hb_font_funcs_set_glyph_extents_func` | `ReQKurupflQ` |
| `hb_font_funcs_set_glyph_func` | `Mh4YoQ4Y57s` |
| `hb_font_funcs_set_glyph_h_advance_func` | `SfmbF51JL1Y` |
| `hb_font_funcs_set_glyph_h_origin_func` | `qv9cZX3xWQo` |
| `hb_font_funcs_set_nominal_glyph_func` | `88WaPmXp6x8` |
| `hb_font_funcs_set_variation_glyph_func` | `ik7nXsVUdMw` |
| `hb_font_make_immutable` | `IThWGmihQUM` |
| `hb_font_set_funcs` | `WH6phgWx6TI` |
| `hb_font_set_ppem` | `ztxZdb1IY+M` |
| `hb_font_set_scale` | `eG5EI3x6twg` |
| `hb_ft_face_create_cached` | `RTxFQhlbF+U` |
| `hb_icu_script_to_script` | `u0E8Xi0easM` |
| `hb_ot_layout_language_find_feature` | `qos3JIrKrDg` |
| `hb_ot_layout_script_get_language_tags` | `HH-sRSNiSaU` |
| `hb_ot_layout_table_get_script_tags` | `wf6CfVJj9PQ` |
| `hb_ot_tag_to_script` | `uKjh3hI1npw` |
| `hb_shape` | `dM3hP4y99rM` |

### Functional

- [The eleven buffer exports](../../../core/libs/prx/libharfbuzz/Buffer.cpp) have no console or title verification. Their signatures, 20-byte glyph records, Unicode properties, UTF-16 decoding and buffer behavior follow the pinned [HarfBuzz 10.0.1](https://github.com/harfbuzz/harfbuzz/tree/a1d9bfe62818ef0fa9cf63b6e6d51436b1c93cbc) public API and backend; the console revision and its Unicode data are unknown. Buffers are opaque host allocations, not console object layouts. Returned arrays belong to that backend and remain valid only until the buffer contents change. Segment-property guessing uses the host process's default language, not a console language setting. Handles from another HarfBuzz instance cannot be mixed with these buffers. Null buffer handles throw except for destruction, which accepts null as the public API does; console null-handle behavior is unverified. Shaping, fonts, faces and guest callbacks remain throwing exports.
