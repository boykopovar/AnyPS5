# libcairo

## add Functional

- [Pattern.cpp](../../../core/libs/prx/libcairo/Pattern.cpp) - console behaviour of `cairo_pattern_create_linear`, `cairo_pattern_create_radial`, `cairo_pattern_add_color_stop_rgba`, `cairo_pattern_set_extend`, `cairo_pattern_set_filter`, `cairo_pattern_set_matrix` and `cairo_pattern_destroy` is unmeasured. They forward to the bundled Cairo 1.18.0: stop offsets and colour components are clamped to 0..1, a stop goes after the stops with the same offset, extend and filter values outside Cairo's enums are stored unchecked, a singular matrix puts the pattern in `CAIRO_STATUS_INVALID_MATRIX`, calls on a pattern in error change nothing, and a null or foreign pattern pointer reaches Cairo unchanged. An unreadable matrix pointer throws through `NotImplemented_nid_no_patch` where Cairo would read it.

## change [SurfacePattern.cpp]

- [SurfacePattern.cpp](../../../core/libs/prx/libcairo/SurfacePattern.cpp) - console behaviour of `cairo_pattern_create_for_surface`, `cairo_set_source` and `cairo_mask` is unmeasured. They forward to the bundled Cairo 1.18.0: a null or failed surface gives a pattern in that error, and a null or failed pattern puts the context in that error and draws nothing; a null context reaches Cairo unchanged. Sony's `CAIRO_STATUS_NULL_SURFACE_IMAGE` patch changes only the span renderer for a solid mask, which `cairo_mask` can reach: Cairo turns an opaque mask (a gradient with opaque stops, or a surface pattern of opaque content whose extend is not `CAIRO_EXTEND_NONE`) into a paint and replaces a uniform gradient mask with a solid one. Results differ from the console only where a surface source's image cannot be allocated.

## remove | `cairo_pattern_create_linear` |

## remove | `cairo_pattern_create_radial` |

## remove | `cairo_pattern_add_color_stop_rgba` |

## remove | `cairo_pattern_set_extend` |

## remove | `cairo_pattern_set_filter` |

## remove | `cairo_pattern_set_matrix` |

## remove | `cairo_pattern_destroy` |
