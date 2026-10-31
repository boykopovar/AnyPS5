# libSceRazorCpu_debug technical debt

### Unknown function info

The 3 exports in [Export.cpp](../../../core/libs/prx/libSceRazorCpu_debug/Export.cpp) come from the `libSceRazorCpu_debug` list in [claimore22/ps5rs](https://github.com/claimore22/ps5rs/blob/5f252a658ff470bcb2b6cd15205fd8ca64ae4477/data/stubs/by_library/libSceRazorCpu_debug_nosubmission.txt) (commit 5f252a658ff470bcb2b6cd15205fd8ca64ae4477), a catalogue of PS5 symbols per library whose provenance the project does not document, and no PS5 export inventory of this module is available to confirm the list is complete or exact. Every name was kept only because its hash equals the listed NID. Prototypes and behaviour are unverified: every function takes no parameters in the import-resolution convention and throws through `NotImplemented_nid_no_patch`.

- [sceRazorCpuStartCapture](../../../core/libs/prx/libSceRazorCpu_debug/Export.cpp) (libSceRazorCpu_debug) - unknown signature (NID 1zHP7BQXxr4)
- [sceRazorCpuStopCapture](../../../core/libs/prx/libSceRazorCpu_debug/Export.cpp) (libSceRazorCpu_debug) - unknown signature (NID mdUPwgHRqss)
- [sceRazorCpuSetLiveServerThreadSettings](../../../core/libs/prx/libSceRazorCpu_debug/Export.cpp) (libSceRazorCpu_debug) - unknown signature (NID q4EyfhLHc0Q)
