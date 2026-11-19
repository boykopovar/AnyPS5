# libSceNKWebKit

## add Functional

- [JSC::CodeOrigin::dump](../../../core/libs/prx/libSceNKWebKit/src/JscCodeOriginDump.cpp) (libSceNKWebKit) - not compared with a console module. The body follows [CodeOrigin.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/bytecode/CodeOrigin.cpp#L135-L155) and [BytecodeIndex.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/JavaScriptCore/bytecode/BytecodeIndex.cpp#L33-L38), which the 11.x fork keeps unchanged: an unset origin prints `<none>`, otherwise `bc#` and the offset, then `cp#` and any checkpoint, through nested virtual `begin`/`end`. The 64-bit composite value uses the 48-bit effective address width. Gap: an inlined origin throws, because it prints the InlineCallFrame's function information and baseline CodeBlock, which have no layout. Closes when: InlineCallFrame is modelled and compared with a console module.

## remove `_ZNK3JSC10CodeOrigin4dumpERN3WTF11PrintStreamE`
