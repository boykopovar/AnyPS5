# libSceVideodec2

## add Functional

- [sceVideodec2Decode](../../../core/libs/prx/libSceVideodec2/Export.cpp) (libSceVideodec2) - validating and copying `input`, `frame` and `output` structs via `CopyInGuest`, throwing `std::runtime_error` on unmapped or invalid pointers, is a host-safety fallback; console hardware fault behavior on invalid buffers has not been measured.
