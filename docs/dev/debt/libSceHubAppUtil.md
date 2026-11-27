# libSceHubAppUtil technical debt

### Immediate returns with a fixed message

- [sceHubAppUtilDeeplinkToLiveDetailView, sceHubAppUtilDeeplinkToPSSubscriptions and sceHubAppUtilGetInstallationStatus](../../../core/libs/prx/libSceHubAppUtil/Export.cpp) (libSceHubAppUtil) - the names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libSceHubAppUtil.c). In the 13.40 module each of the three bodies is 23 bytes long: it passes one constant string to the libc `puts` import and returns `0x818D0001` without reading an argument, so neither the message nor the return value depends on the parameters. Read from disassembly, not measured on a console. The prototypes are unverified and `0x818D0001` has no public description, so the declarations keep no parameters; this is not an ABI claim
