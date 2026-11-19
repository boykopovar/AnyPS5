# libSceNKWebKit

## add Functional

- [WTF::MetaAllocatorHandle::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfMetaAllocatorHandleDump.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [MetaAllocator.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/MetaAllocator.cpp#L118-L121), which matches the module exporting that revision's `release(Locker<Lock> const&, MetaAllocatorHandle&)`: `print` calls the stream's virtual `begin` (slot 4), sends `%p` start, `%s` `...` and `%p` end to the returned stream's `vprintf` (slot 2), then calls `end` (slot 5) on the original stream. Start and end are read at offsets 40 and 48 of the 56-byte handle, the layout #21979 measured, without being checked on a console. A null handle, stream, vtable or virtual throws; an exception from `vprintf` propagates without `end`, as in the source. Closes when: compared with a console module.

## remove `_ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE`
