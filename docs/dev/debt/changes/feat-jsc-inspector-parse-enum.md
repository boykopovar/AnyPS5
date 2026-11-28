# libSceJsc

## add Functional

- [Inspector protocol enum string parsing](../../../core/libs/prx/libSceJsc/src/InspectorEnumParsing.cpp) (libSceJsc) - not compared with a console module; the console JavaScriptCore version and global protocol enum encodings are unverified. The three `parseEnumValueFromString` specializations follow pinned [generate_cpp_protocol_types_implementation.py](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/JavaScriptCore/inspector/scripts/codegen/generate_cpp_protocol_types_implementation.py#L110-L132) and compare 8-bit and 16-bit strings, returning an 8-byte indirect [WTF::Optional](https://github.com/WebKit/WebKit/blob/c55364d53f97/Source/WTF/wtf/Optional.h#L306-L410) (flag at 0, 32-bit value at 4: `ExecutionContextType` 5/16/204, `CoordinateSystem` 175/176, `ConsoleMessage::Level` 70-74). Invalid guest references or non-empty null data throw where WebKit faults. Closes when: compared with a console module.
