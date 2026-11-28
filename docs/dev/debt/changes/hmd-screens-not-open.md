# libSceHmd

## add Unknown function info

- [libSceHmd screen and capture calls before their open](../../../core/libs/prx/libSceHmd/Export.cpp) (libSceHmd) - only the state before the open call is modelled: the three openers (`sceHmdInternalSeparateModeOpen`, `sceHmdInternalAnotherScreenOpen`, `sceHmdReprojectionInitializeCapture`) throw, and the 17 calls they gate return `0x81110002`, as firmware 13.42 does while each open flag is clear (`0x90284`, `0x90264`, `0x92d70`; read and run under an emulator, not measured on a console). These answers must follow the flags once an opener is implemented. What the calls do when open, and their argument checks after the flag test, are not read
