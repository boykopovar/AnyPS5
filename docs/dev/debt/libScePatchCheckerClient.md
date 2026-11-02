# libScePatchCheckerClient technical debt

### Functional

- [scePatchCheckerInitialize, scePatchCheckerTerminate](../../../core/libs/prx/libScePatchCheckerClient/Export.cpp): atomic lifecycle tracking. Duplicate initialization and uninitialized termination throw `std::logic_error`; uninitialized export calls throw before dispatching.

### Unknown function info

- [scePatchCheckerCancel, scePatchCheckerCheckPatch, scePatchCheckerCreateHandler, scePatchCheckerDestroyHandler, scePatchCheckerDisableAutoDownload, scePatchCheckerEnableAutoDownload, scePatchCheckerGetApplicableTick, scePatchCheckerGetPackageInfo, scePatchCheckerRequestCheckPatch and scePatchCheckerRequestCheckPatchByType](../../../core/libs/prx/libScePatchCheckerClient/Export.cpp) (libScePatchCheckerClient) - names and NIDs from [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePatchCheckerClient.c). Shims resolve imports without ABI claims.
