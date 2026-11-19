# libSceNKWebKit

## add Functional

- [WTF printInternal and URL::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfPrintStream.cpp) (libSceNKWebKit) - unverified on console. TypedArrayMode printing follows [JSArrayBufferView.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/JSArrayBufferView.cpp#L389-L430); invalid values throw where the console asserts. StringImpl printing follows [PrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp#L113-L120). `URL::dump` calls `out.print(m_string)` ([URL.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/URL.cpp#L847)); `out.print` wraps both in its begin/end atomic section, OOM branch included, the StringImpl export prints without one via `printInternal(const char*)`. Lenient conversion never fails; a host allocation failure throws instead.

## change [Media and playback engine stubs]

- [Media and playback engine stubs](../../../core/libs/prx/libSceNKWebKit/src/Media.cpp) (libSceNKWebKit) - WebCore::MediaPlayer, HTMLMediaElement::fastSeek, TextTrackCueGeneric::create, MediaTime::toJSONObject, MediaTime::toJSONString, MediaTimeRange::toJSONString, and dump(PrintStream&) exports remain throwing stubs in Export.cpp because they require the WebCore playback pipeline, DOM/engine objects, JSON object model, or PrintStream runtime.

## remove | `_ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE` | `DwwpqTcKG0Q` |

## remove | `_ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE` | `iZmKEESJ3mw` |

## remove | `_ZNK3WTF3URL4dumpERNS_11PrintStreamE` | `ITwB6pPwmRc` |
