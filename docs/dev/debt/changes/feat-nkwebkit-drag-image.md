# libSceNKWebKit

## add Functional

- [WebCore::DragImage](../../../core/libs/prx/libSceNKWebKit/src/DragImage.cpp) - console ABI is unmeasured. Layout is the 200-byte Cairo DragImage.h variant: engagement after payload, index after union. Special members follow [DragImage.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/DragImage.cpp#L247-L279); `deleteDragImage` is a no-op taking its `DragImageRef` by value, so destruction derefs the surface through `libcairo`'s `cairo_surface_destroy`. Color moves steal the packed bits, empty the source and keep both on packed equality ([Color.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/platform/graphics/Color.cpp#L52-L78)). Destructor and assignment throw for a non-null `Image`, a held `PathImpl` or an out-of-line color (bit 51), whose deref is unmodeled; null objects throw where WebKit faults. Closes when: compared with a console module.

## remove | `_ZN7WebCore9DragImageC1EOS0_` | `Gblk9Tyt0B0` |

## remove | `_ZN7WebCore9DragImageC1Ev` | `qAqfKFQJNFk` |

## remove | `_ZN7WebCore9DragImageC2EOS0_` | `kl03klwNnE0` |

## remove | `_ZN7WebCore9DragImageC2Ev` | `mWHQR9TI4wQ` |

## remove | `_ZN7WebCore9DragImageD1Ev` | `0jsZGfvAN3g` |

## remove | `_ZN7WebCore9DragImageD2Ev` | `g2fw8kXx2IU` |

## remove | `_ZN7WebCore9DragImageaSEOS0_` | `jRX3E9YPAyk` |
