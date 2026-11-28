# libSceNKWebKit

## add Functional

- [PerspectiveTransformOperation lifecycle](../../../core/libs/prx/libSceNKWebKit/src/PerspectiveTransformOperation.cpp) (libSceNKWebKit) - not compared with a console module. `create`, the constructors, the three destructors and `clone` follow [PerspectiveTransformOperation.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/transforms/PerspectiveTransformOperation.h). Objects carry a library vtable with the ten slots of that revision's [TransformOperation.h](https://github.com/WebKit/WebKit/blob/c55364d53f971f63a709976f168d932d558b8909/Source/WebCore/platform/graphics/transforms/TransformOperation.h) (unchanged from 2019 to 2021-04-16); its `blend` and `dump` slots are the still-throwing exports. `create` and `clone` return their `Ref` through a hidden pointer and allocate with `fastMalloc`. A calculated length throws in all of them. Closes when: compared with a console module.
