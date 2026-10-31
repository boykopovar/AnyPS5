# libSceGpuTrace technical debt

### Unknown function info

The 8 exports in [Export.cpp](../../../core/libs/prx/libSceGpuTrace/Export.cpp) come from the `libSceGpuTrace` list in [claimore22/ps5rs](https://github.com/claimore22/ps5rs/blob/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library/libSceGpuTrace_nosubmission.txt) (commit 5f252a658ff470bcb2b6cd15205fd8ca64ae4477), a catalogue of PS5 symbols per library whose provenance the project does not document, and no PS5 export inventory of this module is available to confirm the list is complete or exact. Every name was kept only because its hash equals the listed NID. Prototypes and behaviour are unverified: every function takes no parameters in the import-resolution convention and throws through `NotImplemented_nid_no_patch`.

- [sceGpuTraceParametersSetDuration](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID GLFNgsekcJU)
- [sceGpuTraceParametersInit](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID IQl1M8Gynsw)
- [sceGpuTraceParametersSetMemorySize](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID QDMpdglDPoo)
- [sceGpuTraceParametersSetGroup](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID TehfTHUiaag)
- [sceGpuTraceStart](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID WpNcYUoJe74)
- [sceGpuTraceStop](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID rxXvAxEbq8I)
- [sceGpuTraceMonitorTrigger](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID t+2V-Erb8Bg)
- [sceGpuTraceCancel](../../../core/libs/prx/libSceGpuTrace/Export.cpp) (libSceGpuTrace) - unknown signature (NID ubB+tVwd4-U)
