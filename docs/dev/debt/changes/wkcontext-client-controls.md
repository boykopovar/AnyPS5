# libSceNKWebKit

## add Silent stubs

- [WKContext client and invalid message controls](../../../core/libs/prx/libSceNKWebKit/src/ContextClients.cpp) (libSceNKWebKit) - `WKContextSetClient` and `WKContextSetHistoryClient` record only whether a client structure was passed without reading the structure or its version; no context or history callback is ever invoked, page `addsVisitedLinks` is not updated, and `WKContextSetInvalidMessageFunction` stores the callback without calling it.

## remove `s19dG0nY-dE`

## remove `voUp4sJ5mfU`

## remove `v+TCMUBUWog`
