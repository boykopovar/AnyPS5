# libSceVideodec2

## add Functional

- [sceVideodec2Decode and sceVideodec2Flush ClearOutput](../../../core/libs/prx/libSceVideodec2/Export.cpp) (libSceVideodec2) - validates `frame` and `output` writability upfront via `GuestArena::HostWrite` and `GuestWritable_nid_no_patch` before clearing fields, throwing `Videodec2: invalid argument pointer` without mutating `output` or opening `output` to host writes when `frame` is read-only. Console behavior for bad pointers (throw vs. error code) and `EmitDecoded` picture metadata writes remain unmeasured on hardware. Closes when: measured on hardware. Labels: console, tested.
