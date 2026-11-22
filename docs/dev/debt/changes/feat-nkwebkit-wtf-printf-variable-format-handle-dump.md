# libSceNKWebKit

## add Functional

- [WTF::PrintStream::printfVariableFormat and WTF::MetaAllocatorHandle::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfStreamPrint.cpp) (libSceNKWebKit) - not compared with a console module; `printfVariableFormat` passes its System V `va_list` to the stream's `vprintf` slot (2) and `MetaAllocatorHandle::dump` prints `%p` of `m_start`, `...` and `%p` of `m_end` in one `print` (`begin`/`end` slots 4 and 5). The 56-byte `MetaAllocatorHandle` layout (`ThreadSafeRefCounted` count at 0, `RedBlackTree` node at 8, allocator reference at 32, `m_start` at 40, `m_end` at 48) follows the pinned headers for a release non-ARM64E build and is unverified. A null stream, null format, null handle or null stream returned by `begin` throws instead of crashing. Closes when: compared with a console module.

## remove | `_ZN3WTF11PrintStream20printfVariableFormatEPKcz` | `wdbdMn2JHZc` |

## remove | `_ZNK3WTF19MetaAllocatorHandle4dumpERNS_11PrintStreamE` | `BPh+itev3WQ` |
