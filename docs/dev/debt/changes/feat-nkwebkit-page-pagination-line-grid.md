# libSceNKWebKit

## add Functional

- [WKPageGetPaginationLineGridEnabled](../../../core/libs/prx/libSceNKWebKit/src/PageLayout.cpp) (libSceNKWebKit) - implements the companion getter returning `page->paginationLineGridEnabled` for `WKPageRef`, casting and checking invalid null page references with `std::invalid_argument`. Replaces the parameterless constant 0 stub from `ConstantExports.cpp`. Unverified on console.
