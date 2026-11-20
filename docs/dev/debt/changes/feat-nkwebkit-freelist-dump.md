# libSceNKWebKit

## add Functional

- [JSC::FreeList::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfObjectDump.cpp) (libSceNKWebKit) - not compared with a console module. Follows [FreeList.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/FreeList.cpp#L79-L82); the 40-byte interval layout (`m_intervalStart`, `m_intervalEnd`, `m_nextInterval`, `m_secret`, `m_originalSize`, `m_cellSize`) comes from [FreeList.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/heap/FreeList.h) and is not measured. `m_secret` prints with `%lu`, as `uint64_t` is `unsigned long` on the console. A null list or stream throws.

## remove `_ZNK3JSC8FreeList4dumpERN3WTF11PrintStreamE`
