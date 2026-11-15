# libSceNKWebKit

## add Additional callable exports

- [WebCore property dumps](../../../core/libs/prx/libSceNKWebKit/src/WebCorePropertyDump.cpp) (libSceNKWebKit) - not compared with a console module. The `TextStream` dumps of `TouchAction`, `FloatRoundedRect`, `RectEdges<float>`, `Region` and `ScrollableAreaParameters` follow the pinned [ScrollingCoordinatorTypes.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/page/scrolling/ScrollingCoordinatorTypes.cpp#L129-L146) and the other files named in the source. The 16-byte `RectEdges<float>` and the 11-byte by-value `ScrollableAreaParameters` are source layouts pinned with `static_assert`, not console measurements. An out-of-range enumeration value prints nothing, as the release build does.

## remove | `_ZN7WebCorelsERN3WTF10TextStreamENS_11TouchActionE` | `D5Y1cX5ZyLU` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamENS_24ScrollableAreaParametersE` | `vrqGZr3Opv8` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_16FloatRoundedRectE` | `0kJTcu2ueOE` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_6RegionE` | `4W0wxLj52NI` |

## remove | `_ZN7WebCorelsERN3WTF10TextStreamERKNS_9RectEdgesIfEE` | `s8ZoG4mimcw` |
