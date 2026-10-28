# libSceFreeTypeOt technical debt

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

Windows provider wiring for `FT_Cos`, `FT_Angle_Diff`, `FT_Sin`, `FT_Tan`, `FT_Vector_Unit`, `FT_Outline_Copy`, `FT_Outline_Decompose` and `FT_Outline_Get_Orientation` is reserved for [#11613](https://github.com/boykopovar/AnyPS5/pull/11613). This branch adds no competing Windows thunks for these eight NIDs and must be rebased after that provider change lands. Non-Windows dependency lookup is not a claim of Windows availability before that prerequisite.
