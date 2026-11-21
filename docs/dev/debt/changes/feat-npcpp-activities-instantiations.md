# libSceNpCppWebApi

## add Silent stubs

- [Activities::V1 and Leaderboards::V1 instantiations](../../../core/libs/prx/libSceNpCppWebApi/Export.cpp) (libSceNpCppWebApi) - the exports titles import from these two families are bookkeeping only: the `Transaction` constructors and destructors, `TransactionBase::start`, `finish` and `abort`, `Transaction::hasResponse`, and the `IntrusivePtr`, `Vector` and `Iterator` members over `UserActivities`, `Activity` and `Leaderboards::V1::Entry`. They answer like their siblings offline (`start` returns `SCE_NP_WEBAPI_ERROR_NOT_SIGNED_IN`, `finish` and `abort` `TransactionNotStarted`, `hasResponse` zero, the pointer members `nullptr` over an empty handle) and store nothing a request would read. The vector and iterator members walk host-created bodies only and throw on a title-provided one, and no response object is produced, so nothing here is measured on a console.
