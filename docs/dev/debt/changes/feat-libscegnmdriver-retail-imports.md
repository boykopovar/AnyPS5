# libSceGnmDriver

## add Silent stubs

- [Resource and owner registration calls](../../../core/libs/prx/libSceGnmDriver/Export.cpp) (libSceGnmDriver, PS4) - the 17 resource and owner registration calls, from `sceGnmRegisterOwner` to `sceGnmUnregisterOwnerAndResources`, return `0x8EEE00FF` without reading their arguments or registering anything. This is the value `libSceGnmDriverForNeoMode` already returns for them and that [shadPS4](https://github.com/shadps4-emu/shadPS4/blob/a808e76/src/core/libraries/gnmdriver/gnmdriver.cpp) returns for calls it marks as not available in retail firmware; it is not measured on a console. CUSA12982 imports them from `libSceGnmDriver`.
