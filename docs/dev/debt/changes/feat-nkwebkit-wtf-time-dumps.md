# libSceNKWebKit

## add Functional

- [WTF time and ASCIILiteral dump](../../../core/libs/prx/libSceNKWebKit/src/WtfTimeDump.cpp) (libSceNKWebKit) - the guest `PrintStream` vtable slots (`vprintf` 2, `begin` 4, `end` 5, from the order in PS5 11.x [PrintStream.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.h#L53-L101)), the 32-bit `ClockType` and the `Seconds`, `WallTime`, `MonotonicTime` and `TimeWithDynamicClockType` layouts are not measured on a console. The dumps call those slots directly instead of the `printInternal` exports. An unknown `ClockType` throws before `begin`, where WebKit's `RELEASE_ASSERT_NOT_REACHED` fires after it; null pointers throw where WebKit faults.

## change [Media and playback engine stubs]

- [Media and playback engine stubs](../../../core/libs/prx/libSceNKWebKit/src/Media.cpp) (libSceNKWebKit) - WebCore::MediaPlayer, HTMLMediaElement::fastSeek, TextTrackCueGeneric::create, MediaTime::toJSONObject, MediaTime::toJSONString, MediaTimeRange::toJSONString, and PlatformTimeRanges::dump(PrintStream&) exports remain throwing stubs in Export.cpp because they require the WebCore playback pipeline, DOM/engine objects, JSON object model, or PrintStream runtime.

## remove | `_ZNK3WTF12ASCIILiteral4dumpERNS_11PrintStreamE` | `IdIO1h6EW3I` |

## remove | `_ZNK3WTF13MonotonicTime4dumpERNS_11PrintStreamE` | `K-UfI+QGiEU` |

## remove | `_ZNK3WTF24TimeWithDynamicClockType4dumpERNS_11PrintStreamE` | `Ea+86Wd8S-g` |

## remove | `_ZNK3WTF7Seconds4dumpERNS_11PrintStreamE` | `hrTPOtnC7Mw` |

## remove | `_ZNK3WTF8WallTime4dumpERNS_11PrintStreamE` | `BXpm0S4olVo` |

## remove | `_ZNK3WTF9MediaTime4dumpERNS_11PrintStreamE` | `tbWxzNR07RQ` |

## remove | `_ZNK3WTF9MediaTime8toStringEv` | `GraLckR1SPU` |
