# libSceNKWebKit

## add Functional

- [JSC::ClassInfo::dump](../../../core/libs/prx/libSceNKWebKit/src/JscClassInfoDump.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [ClassInfo.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/runtime/ClassInfo.cpp#L33-L36), which the [11.x fork](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/ClassInfo.cpp#L33-L36) keeps unchanged: `print` calls virtual `begin` (slot 4), sends `%p` this, then `%s` with `:[`, the class name and `]` to the returned stream's `vprintf` (slot 2), then calls `end` (slot 5) on the original stream. `className` is the pointer at offset 0 in both revisions. A null class info, class name, stream, vtable or virtual throws. Closes when: compared with a console module.

## remove `_ZNK3JSC9ClassInfo4dumpERN3WTF11PrintStreamE`
