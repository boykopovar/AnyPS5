# libSceNgs2 technical debt

### Unknown function info

- [sceNgs2FftInit, sceNgs2FftProcess, sceNgs2FftQuerySize](../../../core/libs/prx/libSceNgs2/PendingExports.cpp) (libSceNgs2) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNgs2.c), but no credible prototypes were found in the cited sources. They are declared without parameters solely to resolve known imports and make no ABI claim.
- [sceNgs2ParseWaveformUser, sceNgs2SystemEnumHandles, sceNgs2SystemEnumRackHandles](../../../core/libs/prx/libSceNgs2/PendingExports.cpp) (libSceNgs2) - names and NIDs are in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceNgs2.c); signatures follow [SharpProspero's Ngs2 bindings](https://github.com/SvenGDK/SharpProspero/blob/5dfdb4b0b166e83dc9ae275001afb695f12c7174/src/SharpProspero/Interop/Audio/Ngs2.cs), not measured on a console. Opaque structure pointers use `void*`; these stubs throw for import resolution.
