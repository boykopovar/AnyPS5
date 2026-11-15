# libcairo

## add Functional

- [FontPipeline.cpp](../../../core/libs/prx/libcairo/FontPipeline.cpp) and [FontOptions.cpp](../../../core/libs/prx/libcairo/FontOptions.cpp) - Font-face handles must be native Cairo 1.18.0 objects. No implemented guest export produces the initial font face yet; the FreeType face constructor remains a stub. The hand-written faces proposed in PR #18406 cannot be passed to these wrappers. Native Cairo uses its built-in twin font until a compatible FreeType backend and face producer are connected, so glyph shapes and font resolution differ from the console. Only the four font options exposed by guest setters cross the options bridge. PS5 behavior, including signatures and record layouts inferred from the pinned public Cairo header, is unmeasured.

- [FontPipeline.cpp](../../../core/libs/prx/libcairo/FontPipeline.cpp) - Null scaled-font handles, unreadable matrices, strings or glyph arrays, unwritable output memory and glyph indices above the native `unsigned long` range throw. Cairo 1.18.0 dereferences a null font face, font matrix or CTM passed to `cairo_scaled_font_create`; the wrapper instead returns Cairo's null-pointer error font, after the native face-status and NaN font-matrix checks, and the console response is unmeasured. NUL-terminated UTF-8 has a 1 MiB limit; explicitly sized text is copied for its full declared length. Guest buffers are staged before native conversion, including reusable glyph and cluster arrays. Allocation failures in staging can throw C++ allocation exceptions. Output publication is not atomic against overlapping buffers or concurrent page-protection changes. Text cluster arrays use host allocation; the catalogue has no `cairo_text_cluster_free` export. Console responses to address faults and allocator interoperability for these cluster arrays are unverified.

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
