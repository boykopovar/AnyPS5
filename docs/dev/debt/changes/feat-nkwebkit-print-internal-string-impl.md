# libSceNKWebKit

## add Functional

- [WTF::printInternal(PrintStream&, const StringImpl*)](../../../core/libs/prx/libSceNKWebKit/src/WtfPrintInternalStringImpl.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [PrintStream.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WTF/wtf/PrintStream.cpp#L70-L114) of August 2020: a null string prints `(null StringImpl*)`, a string longer than `MaxLength / 3` prints the out-of-memory message in three pieces, and any other string goes through the lenient `StringImpl::utf8` export, each piece reaching the stream's virtual `vprintf` (vtable slot 2) as `"%s"`. A null stream, vtable or `vprintf` throws; exceptions from `vprintf` propagate. Closes when: compared with a console module.

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEPKNS_10StringImplE`
