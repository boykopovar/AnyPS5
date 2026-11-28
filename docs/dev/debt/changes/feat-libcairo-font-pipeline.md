# libcairo

## add Functional

- [FontPipeline.cpp](../../../core/libs/prx/libcairo/FontPipeline.cpp) and [FontOptions.cpp](../../../core/libs/prx/libcairo/FontOptions.cpp) - Faces from `cairo_ft_font_face_create_for_ft_face` and `cairo_ft_font_face_create_for_pattern` have no native Cairo 1.18.0 face, so `cairo_scaled_font_create` and `cairo_set_font_face` throw for them. Native Cairo uses its built-in twin font until a compatible FreeType backend and face producer are connected, so glyph shapes and font resolution differ from the console. `cairo_scaled_font_get_font_face` returns one [FontFace.cpp](../../../core/libs/prx/libcairo/FontFace.cpp) record per native face, freed with it; reference and destroy act on the native count. Only the four font options exposed by guest setters cross the options bridge. PS5 behavior, including signatures and record layouts inferred from the pinned public Cairo header, is unmeasured.

- [FontPipeline.cpp](../../../core/libs/prx/libcairo/FontPipeline.cpp) - Null scaled-font handles, unreadable inputs, unwritable outputs and glyph indices above the native `unsigned long` range throw. `cairo_scaled_font_create` returns a null-pointer error font for a null face, font matrix or CTM after earlier checks. Cairo 1.18.0 dereferences those null arguments. The console response to these errors is unmeasured.

- [FontPipeline.cpp](../../../core/libs/prx/libcairo/FontPipeline.cpp) - NUL-terminated UTF-8 has a 1 MiB limit. Explicit lengths are copied in full. Conversion rejects negative capacities for provided arrays and sizes staging by the conversion result. Staging allocations can throw. Output writes can be partial for overlapping buffers or concurrent protection changes. Text cluster arrays use host allocation. The export catalogue has no `cairo_text_cluster_free`, and console allocator interoperability is unverified.

- [Device.cpp](../../../core/libs/prx/libcairo/Device.cpp) - `cairo_device_destroy` forwards null and valid native device handles to Cairo 1.18.0. No current guest export creates a device handle; native recording-observer device reference counts and finalization are tested. Cairo 1.18.0 normal-observer teardown forms a member pointer through a null recording surface in `cairo-surface-observer.c:170`; UBSan reports it, and these tests use recording observers. Console device backends and destruction behavior are unmeasured.

## change [Glyph.cpp]

- [Glyph.cpp](../../../core/libs/prx/libcairo/Glyph.cpp) - `cairo_glyph_free` releases the host allocations returned by `cairo_scaled_font_text_to_glyphs`, including null. Foreign allocations must use the same host allocator, as required by Cairo's matching allocation/free API. Console allocator interoperability is unverified.

## remove | `cairo_scaled_font_get_font_face` |

## remove | `cairo_scaled_font_get_font_options` |

## remove | `cairo_scaled_font_create` |

## remove | `cairo_scaled_font_text_to_glyphs` |

## remove | `cairo_scaled_font_glyph_extents` |

## remove | `cairo_set_scaled_font` |

## remove | `cairo_set_font_face` |

## remove | `cairo_device_destroy` |
