# libSceJsc

## add Functional

- [WTF::StringView::GraphemeClusters::Iterator::operator!=](../../../core/libs/prx/libSceJsc/src/WtfGraphemeClusters.cpp) (libSceJsc) - not compared with a console module; the console WTF version and the 32-byte `Iterator::Impl` layout (`StringView` reference, `Optional<NonSharedCharacterBreakIterator>` with its flag at 8 and pointer at 16, `m_index` at 24, `m_indexEnd` at 28) are unverified. It follows pinned [StringView.cpp](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WTF/wtf/text/StringView.cpp#L163-L220): the iterators differ when their `m_index` differs. A null iterator or `Impl` pointer throws where WebKit dereferences it. Closes when: compared with a console module.

## remove | `_ZNK3WTF10StringView16GraphemeClusters8IteratorneERKS2_` | `vFWzXVdtsvk` |
