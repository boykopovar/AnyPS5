# libSceUserService

## change [sceUserServiceGetHomeDirectory](../../../core/libs/prx/libSceUserService/Export.cpp)

- [sceUserServiceGetHomeDirectory](../../../core/libs/prx/libSceUserService/Export.cpp) (libSceUserService) - the chosen ABI is `(uint32_t userId, char* path, size_t size)`. Firmware 13.60 `libSceUserService.sprx` at `0x3b20` forwards a 32-bit user value and the destination/capacity to the formatting import with `/user/home/%08x`. The host rejects null and truncated buffers, and returns `USER_SERVICE_ERROR_INVALID_ARGUMENT` with caller `errno` preserved on an unwritable `path` via `GuestCopyOut_nid_no_patch` as a host-safety fallback; native error behavior for these inputs was not established.
