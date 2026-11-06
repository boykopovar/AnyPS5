# libharfbuzz technical debt

### Callable exports

- [Callable exports](../../../core/libs/prx/libharfbuzz/Export.cpp) - 35 callable NIDs match the [public 3.20 catalogue](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libharfbuzz.c) and the [selected export metadata](https://gist.github.com/kyon-eth/510aa3fcaf09cbe8b52048b44e4e67c0/b909f17c99ecd6323c085f70064548789062d57e) from an older `libharfbuzz` image and a later `libSceVshHarfBuzz` image (SHA-256 `25af97e02c1fa8d318ff3697eb8a79057ee9b8c5e5e4ec321f25114aceb05628`). The later image has a different module and library identity; matching NIDs do not establish forwarding or equivalent behavior. The version labels are not authenticated. Prototypes and behavior are unverified; these declarations throw through `NotImplemented_nid_no_patch` and implement no runtime functionality.

| Name | NID |
| --- | --- |
| `hb_blob_create` | `hj1KIIN5D54` |
| `hb_buffer_add_utf16` | `P+hgu937lFs` |
| `hb_buffer_create` | `7f13XmbjTZo` |
| `hb_buffer_destroy` | `K-y1p7Z9THQ` |
| `hb_buffer_get_direction` | `0UafewjjWpM` |
| `hb_buffer_get_glyph_infos` | `ENsHmJCNG4Q` |
| `hb_buffer_get_glyph_positions` | `43mnfTQPCbw` |
| `hb_buffer_get_length` | `leHmVbjHyGw` |
| `hb_buffer_guess_segment_properties` | `DcrJr531A4w` |
| `hb_buffer_reset` | `PEggcTbDlfE` |
| `hb_buffer_set_direction` | `845S2XQdxhw` |
| `hb_buffer_set_script` | `eGbbvD8o7go` |
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
