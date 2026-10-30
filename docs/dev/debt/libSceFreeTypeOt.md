# libSceFreeTypeOt technical debt

- The `FT_Outline_Copy`, `FT_Outline_Decompose`, `FT_Outline_Get_Orientation`, `FT_Sin`, `FT_Tan` and `FT_Vector_Unit` NIDs in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) have named, throwing placeholders with unverified provider-specific prototypes and behavior. The existing placeholders remain until their provider changes land.
- The 73 `sceFreeTypeOtUnknown_*_nid_no_patch` exports in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) are present in the PS5 3.20 export inventory from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs). Their prototypes and behavior are unverified. Each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.
### Unknown function info

- [libSceFreeTypeOt additional exports](../../../core/libs/prx/libSceFreeTypeOt/AdditionalExports.cpp) - these name/NID pairs are present in the 13.60 export inventory. Parameter lists, return types and behavior are unverified. Parameterless `int` declarations are import-resolution placeholders, not verified ABI prototypes; every entry throws through `NotImplemented_nid_no_patch`. No runtime functionality is implemented. Provider-specific helpers use explicit NID aliases so they do not introduce canonical names that conflict with unidentified exports in other providers.

| Name | NID |
| --- | --- |
| `FTA_Add_Module` | `mCIXittfvrw` |
| `FTA_Add_Module_autofitter` | `r2C1PcTDYxk` |
| `FTA_Add_Module_bdf` | `1gAJb1uKyGo` |
| `FTA_Add_Module_cff` | `IjquEAbfizc` |
| `FTA_Add_Module_gxvalid` | `L-qInx9Dldk` |
| `FTA_Add_Module_otvalid` | `mhK+E1i3VuE` |
| `FTA_Add_Module_pcf` | `bAbUJvsiVdI` |
| `FTA_Add_Module_pfr` | `1GYsjUC-HLM` |
| `FTA_Add_Module_psaux` | `KLaWCpRxYWo` |
| `FTA_Add_Module_pshinter` | `qodr9TJuFds` |
| `FTA_Add_Module_psnames` | `Ic8JMhUFe88` |
| `FTA_Add_Module_raster1` | `CIrV8Sq2Uic` |
| `FTA_Add_Module_raster5` | `wbyY8kfNfaI` |
| `FTA_Add_Module_sfnt` | `eS+4GhL28gs` |
| `FTA_Add_Module_smooth` | `XpA3dnvjl8w` |
| `FTA_Add_Module_smooth_lcd` | `NLA7bmDS1jc` |
| `FTA_Add_Module_smooth_lcdv` | `xcJCXHqBrjE` |
| `FTA_Add_Module_t1cid` | `oKG-LjaBzzs` |
| `FTA_Add_Module_truetype` | `zxsyxyiLthM` |
| `FTA_Add_Module_type1` | `+v66TBg7tzA` |
| `FTA_Add_Module_type42` | `ZYpIYZg3Jf4` |
| `FTA_Add_Module_winfonts` | `JSi+hWkA9pI` |
| `FTA_Export_Module_autofitter` | `bZYqFlUVPCY` |
| `FTA_Export_Module_bdf` | `ZhVdSRyrpUI` |
| `FTA_Export_Module_cff` | `phi4au-2t04` |
| `FTA_Export_Module_gxvalid` | `qC+tiGwcW7E` |
| `FTA_Export_Module_otvalid` | `rl7aW17qQb8` |
| `FTA_Export_Module_pcf` | `-qwk1RxUuDA` |
| `FTA_Export_Module_pfr` | `mQYQAi8B0Yo` |
| `FTA_Export_Module_psaux` | `4TJ1qJGXAV8` |
| `FTA_Export_Module_pshinter` | `IewcaRR3F8I` |
| `FTA_Export_Module_psnames` | `vuuFugcw5ss` |
| `FTA_Export_Module_raster1` | `xaswa+ndZS0` |
| `FTA_Export_Module_raster5` | `6zDeyzjWz-8` |
| `FTA_Export_Module_sfnt` | `5-z1quqcDYo` |
| `FTA_Export_Module_smooth` | `VVfxa65sWF0` |
| `FTA_Export_Module_smooth_lcd` | `+bRRhPeBqZI` |
| `FTA_Export_Module_smooth_lcdv` | `q7kTOFaQrW4` |
| `FTA_Export_Module_t1cid` | `aX8PI-h3+dE` |
| `FTA_Export_Module_truetype` | `KIKL0rz6o-A` |
| `FTA_Export_Module_type1` | `QPh+yb2Y8AQ` |
| `FTA_Export_Module_type42` | `u36SHyuWQCA` |
| `FTA_Export_Module_winfonts` | `ZHsmiChQLTk` |
| `FTA_Remove_Module_autofitter` | `q2ESpjCIk7w` |
| `FTA_Remove_Module_bdf` | `C0nVvG6+x-s` |
| `FTA_Remove_Module_cff` | `DIIRFBxRfMQ` |
| `FTA_Remove_Module_gxvalid` | `s3qCnsS3ATE` |
| `FTA_Remove_Module_otvalid` | `OutzOzZp6-w` |
| `FTA_Remove_Module_pcf` | `E9y7-AmuziU` |
| `FTA_Remove_Module_pfr` | `-whG27+m+C0` |
| `FTA_Remove_Module_psaux` | `wGGhfabJAbs` |
| `FTA_Remove_Module_pshinter` | `KuJkc0qDvOQ` |
| `FTA_Remove_Module_psnames` | `jqMurvieLBk` |
| `FTA_Remove_Module_raster1` | `z9YQIyuTMV4` |
| `FTA_Remove_Module_raster5` | `SKUldaN3e2A` |
| `FTA_Remove_Module_sfnt` | `50HTRXvbW-k` |
| `FTA_Remove_Module_smooth` | `M5dtVv0ln5Q` |
| `FTA_Remove_Module_smooth_lcd` | `xtb9fOR6ilA` |
| `FTA_Remove_Module_smooth_lcdv` | `6z2P2V2XCiI` |
| `FTA_Remove_Module_t1cid` | `MJVrwM6aTTk` |
| `FTA_Remove_Module_truetype` | `EveUtaCYQgk` |
| `FTA_Remove_Module_type1` | `B7Sw-iUs7e4` |
| `FTA_Remove_Module_type42` | `RM21Fgm0ZvQ` |
| `FTA_Remove_Module_winfonts` | `OmxJF0FvwWc` |
| `FTA_Support_Format_Bdf` | `4to3tThESn0` |
| `FTA_Support_Format_Cid` | `JuzrPZClLew` |
| `FTA_Support_Format_OpenType` | `sylevnx-fqk` |
| `FTA_Support_Format_OpenType_Otf` | `0RSuGozMl9o` |
| `FTA_Support_Format_OpenType_Ttf` | `J3HnvW8FJV8` |
| `FTA_Support_Format_Pcf` | `2D3H+1nhwqQ` |
| `FTA_Support_Format_Pfr` | `upeKra1FaQk` |
| `FTA_Support_Format_TrueType` | `YpxR8CqGMIM` |
| `FTA_Support_Format_TrueTypeGx` | `83qtpn8+XnA` |
| `FTA_Support_Format_Type1` | `g2p7fkJpuC0` |
| `FTA_Support_Format_Type42` | `xqjAbn8VNGo` |
| `FTA_Support_Format_WinFonts` | `Yu7mmGCjSYM` |
| `FTA_Support_Hinter_AutoFitter` | `SDPzjFN7y4E` |
| `FTA_Support_Modules` | `oM0zMNCfXKM` |
| `FTA_Support_Renderer_Raster1` | `3dTNNBoujn0` |
| `FTA_Support_Renderer_Raster5` | `Tzm-z36UYKg` |
| `FTA_Support_Renderer_Smooth` | `b7S96aD8dkE` |
| `FTA_Support_Renderer_Smooth_Lcd` | `M94SjXo-UVA` |
| `FTA_Support_Renderer_Smooth_Lcdv` | `Ns9a-LjOcDw` |
| `FT_Add_Module` | `LonHHN98tMI` |
| `FT_Attach_Stream` | `ZqvTX8u1bEc` |
| `FT_CMap_Done` | `h0n4Fb9j2R8` |
| `FT_CMap_New` | `wLqQQWyOdpc` |
| `FT_Done_GlyphSlot` | `oJm1lThZ3iQ` |
| `FT_Get_Module_Interface` | `5NYsP564wwY` |
| `ft_service_list_lookup` | `00MQVzW2OPY` |
| `ft_stub_set_char_sizes` | `dGrmvw2Y1qA` |
| `ft_stub_set_pixel_sizes` | `-gdRfP6ZTys` |
| `ft_synthesize_vertical_metrics` | `y4Dg0njlwIU` |
| `ft_validator_error` | `CfkNVHnT78w` |
| `ft_validator_init` | `2WLueaP8Qyc` |
| `ft_validator_run` | `5DQUuOYK190` |

- [FT_Outline_GetInsideBorder, FT_Outline_GetOutsideBorder](../../../core/libs/prx/libSceFreeTypeOt/CMakeLists.txt) (libSceFreeTypeOl, libSceFreeTypeOt) - console prototypes, enum/layout equivalence, FreeType configuration and module/library versions are unverified. The existing LP64 outline layout and unsigned 16-bit counts follow bundled FreeType 2.13.3; border selection follows [ftstroke.c](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftstroke.c), with the shifted polygon area from [ftoutln.c](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftoutln.c) accumulated in 64 bits. Null positive-count arrays and out-of-range or non-increasing contour endpoints throw when reached; their console behavior is unknown. Tags and flags are not read, and input arrays remain caller-owned.

- [FT_Stream calls](../../../core/libs/prx/libSceFreeTypeOt/Stream.cpp) (libSceFreeTypeOt) - console signatures and FreeType version are unverified. The [PS5 3.20 inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceFreeTypeOt.c) has both the signed readers FreeType removed in 2.4.5 and their unsigned replacements; the 36 calls follow 2.4.12 [ftstream.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftstream.c) and [ftobjs.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c#L205-L219), the signed readers 2.4.4, on the 80-byte LP64 record with guest callbacks. A null pointer FreeType would dereference throws before any allocation, read or frame is left behind, as does `FT_Stream_ReadFields` with a null stream, whose error changed in 2.5.4. `FT_Stream_New` and `FT_Stream_Open` still throw.

- The import-resolution helpers in [Export.cpp](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) retain the PS5 3.20 NIDs from [PS5-3.20_Libs](https://github.com/DNNDHH/PS5-3.20_Libs), including identified names and still-unidentified exports. Their provider-specific prototypes and behavior are unverified. Each uses the no-parameter import-resolution convention and throws through `NotImplemented_nid_no_patch`; these declarations make no ABI or behavior claim.

- [Forwarded FreeType exports](../../../core/libs/prx/libSceFreeTypeOt/ForwardExports.cpp) use the existing typed guest wrappers in a single libSceFreeType dependency. Windows exports explicit typed thunks; macOS and Linux retain the dependency and resolve its exports without duplicating wrappers or ownership state. This preserves the existing wrapper contracts; it does not establish equivalence with every firmware revision.

### Missing data exports

The following 13.60 data exports remain unavailable. Their guest structure layout, function-pointer ABI and lifetime are unverified. Host FreeType tables are not guest-compatible globals merely because their names match. No placeholder storage is provided.

| Name | NID | Bytes |
| --- | --- | --- |
| `sfnt_module_class` | `6wWrq5tb+yk` | 72 |
| `tt_cmap0_class_rec` | `yS-FJI-37oc` | 104 |
| `tt_cmap2_class_rec` | `ZDvHK6T-sh4` | 104 |
| `tt_cmap4_class_rec` | `GBl3n8k9jWY` | 104 |
| `tt_cmap6_class_rec` | `fIVxKQHldfk` | 104 |
| `tt_cmap8_class_rec` | `RIRv-1FA-yU` | 104 |
| `tt_cmap10_class_rec` | `XX2pm0bprEc` | 104 |
| `tt_cmap12_class_rec` | `BQs6Wk0OSiw` | 104 |
| `tt_cmap13_class_rec` | `q6EWScb8SvU` | 104 |
| `tt_cmap14_class_rec` | `UM6KCdqJmqs` | 104 |
| `cff_driver_class` | `QCHORyo4-Aw` | 208 |
| `cff_cmap_encoding_class_rec` | `TVapRR2giTo` | 80 |
| `cff_cmap_unicode_class_rec` | `HhAxSe0AxGc` | 80 |
| `psnames_module_class` | `yuOrMQkmgZg` | 72 |
| `otv_module_class` | `SwB8EH71bIU` | 72 |
| `gxv_module_class` | `1j+o52TClOU` | 72 |
| `tt_driver_class` | `KrftZ0w-4Ds` | 208 |
| `ft_smooth_renderer_class` | `s1e88GPYMYQ` | 120 |
| `ft_smooth_lcd_renderer_class` | `VVDUxERNatk` | 120 |
| `ft_smooth_lcdv_renderer_class` | `DRP2CWLtObo` | 120 |

### Pending dependency exports

Additional callable NIDs not supplied here remain reserved for open libSceFreeType implementation or export changes. They are not supplied by this batch. Once their base implementations land, Windows requires matching Ot forwarding entries; macOS and Linux must be checked through the retained dependency. Introducing child stubs for those entries would shadow future working providers.

Windows provider wiring for `FT_Cos`, `FT_Angle_Diff`, `FT_Sin`, `FT_Tan`, `FT_Vector_Unit`, `FT_Outline_Copy`, `FT_Outline_Decompose` and `FT_Outline_Get_Orientation` is reserved for [#11613](https://github.com/boykopovar/AnyPS5/pull/11613). This branch adds no competing Windows thunks for these eight NIDs and retains existing throwing placeholders until that provider change lands; it must be rebased after the provider change. Non-Windows dependency lookup is not a claim of Windows availability before that prerequisite.

- [ft_corner_is_flat, ft_corner_orientation](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - console prototypes and FreeType version equivalence are unverified. The LP64 signatures and 64-bit coordinate calculations follow bundled [FreeType 2.13.3](https://github.com/freetype/freetype/blob/42608f77f20749dd6ddc9e0536788eaad70ea4b5/src/base/ftcalc.c): orientation returns +1 for counter-clockwise turns, -1 for clockwise turns and 0 for collinear inputs using 64-bit coordinate cross-products; flatness compares hypotenuse approximations via the alpha-max-plus-beta-min metric and returns 1 when the corner angle deviation does not exceed the threshold, 0 otherwise.

- [FT_MulDiv_No_Round, FT_Matrix_Multiply_Scaled, FT_Vector_Transform_Scaled, ft_highpow2](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - console prototypes and the FT_LONG64 build option are unverified. FT_Property_Set and ft_highpow2 date this FreeType to 2.4.11-2.5.3, whose stock [ftconfig.h](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/include/freetype/config/ftconfig.h#L289-L306) drops FT_LONG64 under `__STDC__` and compiles the 32-bit [FT_MulDiv](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftcalc.c#L411-L479). Inputs where that build, the FT_LONG64 build and bundled 2.13.3 can differ throw without writing: an operand above 0x7FFFFFFF in magnitude, a zero divisor with a zero factor, or a quotient above 0xFFFFFFFF. The scaled calls divide each term by 0x10000 * scaling, so |scaling| > 32767 throws; a null matrix or vector is a no-op unless that product overflows.

- [FT_SqrtFixed, FT_Sqrt32](../../../core/libs/prx/libSceFreeTypeOt/Export.cpp) (libSceFreeTypeOt) - the exports carry the NIDs of these FreeType names; console prototypes and FreeType version equivalence are unverified. `FT_Property_Set` and `FT_Sqrt32` (`FT_CONFIG_OPTION_OLD_INTERNALS`, removed in 2.5.0) narrow this FreeType to 2.4.11-2.4.12 [ftcalc.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftcalc.c#L155-L186). Both take and return 32-bit `FT_Int32` regardless of `FT_LONG64`: [FT_SqrtFixed](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftcalc.c#L855-L885) returns 0 for non-positive inputs and floors `sqrt(x << 16)` for positive `x` (unlike the unsigned rounding `FT_SqrtFixed` in bundled 2.13.3), and `FT_Sqrt32` floors the square root of `x` cast to `FT_UInt32`.

- [ft_mem_alloc, ft_mem_qalloc, ft_mem_realloc, ft_mem_qrealloc, ft_mem_free, ft_mem_dup, ft_mem_strdup, ft_mem_strcpyn](../../../core/libs/prx/libSceFreeTypeOt/src/Memory.cpp) (libSceFreeTypeOt) - the exports carry the NIDs of these FreeType names; console prototypes and FreeType version equivalence are unverified. The LP64 signatures, the guest `FT_MemoryRec` callbacks and the error codes (Invalid_Argument 0x06, Array_Too_Large 0x0A, Out_Of_Memory 0x40) follow FreeType 2.4.12 [ftutil.c](https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftutil.c#L49-L221). Bundled FreeType 2.13.3 differs only in `ft_mem_qrealloc` from a count of 0, which it does not zero. As in a release build, `ft_mem_qrealloc` does not check the `FT_ASSERT` that a block with no current items is null. The current byte size passed to `realloc` wraps where the source's signed product would overflow.
