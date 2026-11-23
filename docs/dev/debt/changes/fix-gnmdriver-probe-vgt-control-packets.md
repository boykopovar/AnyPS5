# libSceGnmDriver

## add Unknown function info

- [sceGnmSetVgtControl and sceGnmResetVgtControl guest command buffer](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver) - the three-dword packet is built on the host and written with GuestCopyOut_nid_no_patch; returning -1 (the refusal for a null or wrongly sized buffer) for an unwritable buffer is a host-safety fallback, and console behaviour on unmapped memory is unmeasured.
