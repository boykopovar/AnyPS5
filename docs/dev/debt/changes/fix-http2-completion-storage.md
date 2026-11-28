# libSceHttp2

## add Functional

- [libSceHttp2 async completion storage](../../../core/libs/prx/libSceHttp2/Export.cpp) - Console behavior under host allocation pressure is unmeasured. Host submissions reserve their operation record and queue node before acceptance; normal completion and request-deletion aborts transfer those nodes without growing a queue. CPU probes reject C++ allocations during queue publication for 32 accepted reads, offline sends and deletion aborts. These probes use no kernel completion queue; allocation failure during transport execution or kernel event delivery is not qualified.
