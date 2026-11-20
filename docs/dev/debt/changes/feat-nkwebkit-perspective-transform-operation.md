# libSceNKWebKit

## add Functional

- [PerspectiveTransformOperation queries](../../../core/libs/prx/libSceNKWebKit/src/PerspectiveTransformOperation.cpp) (libSceNKWebKit) - not compared with a console module; a calculated length throws (needs the calculation-value map). Layout assumed: 16-byte `TransformOperation` header with the 4-byte type at offset 12, then `Length m_p` at 16. `perspective()` returns through a hidden result pointer. Follows the pinned [PerspectiveTransformOperation.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/transforms/PerspectiveTransformOperation.h#L47-L60) and [Length.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/Length.h#L221-L231), whose `create(const Length&)` matches the console export. Closes when: compared with a console module.
