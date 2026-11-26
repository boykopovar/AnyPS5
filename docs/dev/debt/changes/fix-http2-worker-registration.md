# libSceHttp2

## add Functional

- [libSceHttp2 async worker registration](../../../core/libs/prx/libSceHttp2/Export.cpp) - Console behavior when host worker creation fails is unmeasured. Host read and online-send submissions register in-flight work only after their worker is constructed, under the async-state lock; a construction exception leaves no producerless operation. CPU tests inject a resource-unavailable exception immediately before worker construction and verify idle waits and a successful later submission; actual host thread exhaustion is not induced.
