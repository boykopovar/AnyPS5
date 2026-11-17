# libSceNKWebKit

## remove `WKContextSetDomainRelaxationForbiddenForURLScheme`

## add Functional

- [Domain relaxation forbidden URL schemes](../../../core/libs/prx/libSceNKWebKit/src/ContextURLSchemes.cpp) (libSceNKWebKit) - `WKContextSetDomainRelaxationForbiddenForURLScheme` records domain-relaxation forbidden URL schemes in host memory; WebProcess IPC and security origin checks are unmodeled.
