# libSceNKWebKit

## add Functional

- [JSC::DOMJIT::HeapRange::dump](../../../core/libs/prx/libSceNKWebKit/src/JscHeapRangeDump.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [DOMJITHeapRange.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/domjit/DOMJITHeapRange.cpp#L31-L34), which the [11.x fork](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/domjit/DOMJITHeapRange.cpp#L30-L33) keeps unchanged: the non-virtual `PrintStream::printf` sends `0x%x-0x%x` with the zero-extended begin and end to virtual `vprintf` (slot 2), without `begin`/`end`. The range is read as two `uint16_t` fields at offsets 0 and 2 from the union in [DOMJITHeapRange.h](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/domjit/DOMJITHeapRange.h#L120-L128). A null range, stream, vtable or `vprintf` throws. Closes when: compared with a console module.

## remove `_ZNK3JSC6DOMJIT9HeapRange4dumpERN3WTF11PrintStreamE`
