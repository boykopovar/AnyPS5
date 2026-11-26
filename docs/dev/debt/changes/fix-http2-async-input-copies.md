# libSceHttp2

## add Functional

- [libSceHttp2 async input copies](../../../core/libs/prx/libSceHttp2/Export.cpp) - Console behavior for inaccessible async options or upload memory is unmeasured. Host async read/send copy the option through the checked guest reader; online send stages its nonempty upload through that reader before registration, returning `0x817B1225` on copy failure. The offline path still does not read upload bytes. CPU probes cover inaccessible and cross-page inputs, successful retries, binary uploads and ignored zero-length input. Deferred async-read output still uses the transport destination directly; its mapping lifetime and checked completion-time output are not qualified here.
