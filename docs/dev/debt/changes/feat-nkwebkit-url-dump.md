# libSceNKWebKit

## add Functional

- [WTF::printInternal(PrintStream&, const String&) and WTF::URL::dump](../../../core/libs/prx/libSceNKWebKit/src/WtfUrlDump.cpp) (libSceNKWebKit) - not compared with a console module. The bodies follow [PrintStream.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/PrintStream.cpp#L75-L105) and [URL.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/URL.cpp#L774-L777): a null string prints an empty `%s`, a string longer than `MaxLength / 3` prints the out-of-memory message in three pieces, and any other string goes through the lenient `StringImpl::utf8` export to the stream's `vprintf` (slot 2). `URL::dump` prints `m_string` between `begin` (slot 4) and `end` (slot 5, on the original stream). Null arguments throw; a `vprintf` exception skips `end`. Closes when: compared with a console module.

## remove `_ZN3WTF13printInternalERNS_11PrintStreamERKNS_6StringE`

## remove `_ZNK3WTF3URL4dumpERNS_11PrintStreamE`
