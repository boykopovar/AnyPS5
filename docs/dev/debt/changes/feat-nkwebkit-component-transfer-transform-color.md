# libSceNKWebKit

## add Functional

- [BasicComponentTransferFilterOperation::transformColor](../../../core/libs/prx/libSceNKWebKit/src/ComponentTransferFilter.cpp) (libSceNKWebKit) - not compared with a console module: the device's WebKit revision is unverified. It follows [FilterOperation.cpp](https://github.com/WebKit/WebKit/blob/73a35b43be7ae44c084885bd3fa4b403e59723c8/Source/WebCore/platform/graphics/filters/FilterOperation.cpp#L153-L183) of August 2020, whose `OperationType` names match the export: opacity scales alpha unclamped, brightness clamps only below 0, other types return false. It reads a 32-bit type at offset 12 and the `double` amount at 16. Closes when: compared with a console module.
