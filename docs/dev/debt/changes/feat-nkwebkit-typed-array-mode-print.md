# libSceNKWebKit

## add Functional

- [WTF::printInternal(PrintStream&, JSC::TypedArrayMode)](../../../core/libs/prx/libSceNKWebKit/src/WtfTypedArrayModePrint.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [JSArrayBufferView.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/runtime/JSArrayBufferView.cpp#L342-L359) with its four modes; the 11.x fork adds eight resizable/growable modes and renumbers `DataViewMode`, but the module exports the 73a35b43-only `ArrayBufferContents()`, `ArrayBufferContents::clear` and `errorMesasgeForTransfer`. `print` calls the stream's `begin` (slot 4), sends `%s` and the name to `vprintf` (slot 2), then `end` (slot 5). A mode above 3 throws instead of the source's crash; a null stream, vtable or virtual throws. Closes when: compared with a console module.

## remove `_ZN3WTF13printInternalERNS_11PrintStreamEN3JSC14TypedArrayModeE`
