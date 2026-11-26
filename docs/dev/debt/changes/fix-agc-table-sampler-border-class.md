# libSceAgcDriver

## change - [Table S#s]
- [Table S#s](../../../core/libs/prx/libSceAgcDriver/Graphics/src/ShaderResources.cpp) that clamp to transparent black use an extra Vulkan sampler per distinct entry for integer image views. A selected opaque-white entry faults because packed formats and integer depth views need format-specific border conversion. Entries using Vulkan clamp-to-border also fault when a guest axis needs half-border or mirrored-border coordinate handling, or their sampler variants exceed device capacity.
