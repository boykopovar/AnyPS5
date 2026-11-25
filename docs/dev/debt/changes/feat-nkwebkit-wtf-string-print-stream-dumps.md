# libSceNKWebKit

## add Functional

- [WTF::printInternal(PrintStream&, const StringImpl*) and WTF::URL::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfStringDump.cpp) (libSceNKWebKit) - not compared with a console module; the `(Out of memory while converting …)` and `(failed to convert …)` texts of the pinned [PrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp#L76-L120) are never printed, as lenient conversion does not fail and allocation failure is not reported. A null `StringImpl*` prints `(null StringImpl*)`; a string prints its lenient UTF-8 from this library's `StringImpl::utf8` through the stream's `vprintf` slot with `%s`; [URL::dump](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/URL.cpp#L847-L850) does the same for its string between the `begin` and `end` slots, a null string printing nothing. Closes when: compared with a console module.

## remove | `_ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE` | `iZmKEESJ3mw` |

## remove | `_ZNK3WTF3URL4dumpERNS_11PrintStreamE` | `ITwB6pPwmRc` |
