# libSceNKWebKit

## add Functional

- [WTF::Thread::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfThreadDump.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [Threading.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/Threading.cpp#L346-L349), which the 11.x fork keeps unchanged: `print` calls the stream's virtual `begin` (slot 4), sends `%s` `Thread:` and `%p` of the thread's own address to the returned stream's `vprintf` (slot 2), then calls `end` (slot 5) on the original stream. No thread field is read, so no `WTF::Thread` layout is assumed and a null thread prints the null pointer. A null stream, vtable or virtual throws; an exception from `vprintf` propagates without `end`, as in the source. Closes when: compared with a console module.

## remove `_ZNK3WTF6Thread4dumpERNS_11PrintStreamE`
