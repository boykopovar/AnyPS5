# libSceVideoStreamingEngine

## remove vLAERdvUknc

## remove RWCz4Gc+lW0

## remove MB+kfLoZFWE

## remove +gqhFOBhLj4

## remove l42jJq+1X9M

## remove tUuzW-ctqRY

## remove o9B+2IRDKow

## remove ptFhKGXird8

## remove x32O4iKHZyA

## remove Y-Bp-73pyIs

## change Additional libSceVideoStreamingEngine exports
- [Additional libSceVideoStreamingEngine exports](../../../core/libs/prx/libSceVideoStreamingEngine/Export.cpp) - only these two exports still throw `NotImplemented_nid_no_patch`.

## change sceVideoStreamingEngineDestroyMediaKeySystemAccess
- [13.60 queries and gates](../../../core/libs/prx/libSceVideoStreamingEngine/Export.cpp) (libSceVideoStreamingEngine) - `QueryInitEsvmMemory`, `QueryVideoMemory`, `IsTransparentVideoMode` and 19 `MediaKey*` exports follow 13.60 (`0x19150`, `0x19380`, `0x4cec0`, `0xf3f0`, `0x12510`): returning `0`/`0x81780001`, `0x8178000A`, or `0x81780002` before init; CDM stays unimplemented.
