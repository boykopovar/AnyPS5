# libSceHttp2

## add Functional

- [libSceHttp2 async input copies](../../../core/libs/prx/libSceHttp2/Export.cpp) - Console behavior for inaccessible async options or upload memory is unmeasured. Host async read/send copy the option through the checked guest reader; online send stages its nonempty upload through that reader before registration, returning `0x817B1225` on copy failure. The offline path still does not read upload bytes. CPU probes cover inaccessible and cross-page inputs, successful retries, binary uploads and ignored zero-length input. Deferred async reads stage at most 16 MiB per call in host memory and copy the received bytes to the guest buffer through the checked writer, completing with `0x817B1225` if that copy fails; console behavior for an output buffer unmapped before completion is unmeasured.
