# libScePatchCheckerClient

## remove [scePatchCheckerDisableAutoDownload, scePatchCheckerEnableAutoDownload, scePatchCheckerInitialize and scePatchCheckerTerminate]

## add Unknown function info

- [scePatchCheckerDisableAutoDownload and scePatchCheckerEnableAutoDownload](../../../core/libs/prx/libScePatchCheckerClient/Export.cpp) (libScePatchCheckerClient) - names and NIDs are present in the [PS5 3.20 export inventory](https://github.com/DNNDHH/PS5-3.20_Libs/blob/b2a77c09cd71c01f34c790cfbe98c751a13262ad/libScePatchCheckerClient.c), but no credible prototypes were found. The declarations have no parameters solely to resolve known imports; this is not an ABI claim and callers must not call them until signatures are verified. Firmware 13.42's `libScePatchCheckerClient.sprx` exports neither, and their NIDs are in none of the 640 modules of that firmware dump ([reading](https://github.com/boykopovar/AnyPS5/issues/21869#issuecomment-6938564992))

## add Unknown function info

- [scePatchCheckerInitialize](../../../core/libs/prx/libScePatchCheckerClient/Export.cpp) (libScePatchCheckerClient) - takes no arguments and throws. In firmware 13.42 ([reading](https://github.com/boykopovar/AnyPS5/issues/21869#issuecomment-6938564992), disassembly only, not run on a console) it returns `0x80182401` while a client exists, then creates an IPMI client for the `ScePatchChecker` system service and returns what its creation and connect return; nothing here provides that service. `scePatchCheckerTerminate` returns 0 there on every path, initialized or not, as it does here
