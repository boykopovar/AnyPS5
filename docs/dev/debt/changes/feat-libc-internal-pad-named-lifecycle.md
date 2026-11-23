# libSceLibcInternal

## remove _ZNSt4_Pad7_LaunchEPKcPP12pthread_attrPP7pthread

## remove _ZNSt4_Pad7_LaunchEPKcPP7pthread

## remove _ZNSt4_PadC2EPKc

## change [_ZNSt4_PadD1Ev]

- [_ZNSt4_PadD1Ev, _ZNSt4_PadC2EPKc, _ZNSt4_Pad7_LaunchEPKcPP7pthread and _ZNSt4_Pad7_LaunchEPKcPP12pthread_attrPP7pthread](../../../core/libs/prx/libSceLibcInternal/Pad.cpp) (libSceLibcInternal, NIDs XyJPhPqpzMw, uhnb6dnXOnc, PBbZjsL6nfc, H7-7Z3ixv-w) follow Dinkumware 	hr/xthread and the SDK 10.00 libc.prx disassembly (at 0x17c80, 0x17e50, 0x17f50), otherwise without console verification. _ZNSt4_PadC2EPKc initializes condition and mutex with the thread name and sets started to false; the launch overloads pass the thread name as the fifth argument to pthread_create and wait for the started flag. Layout and synchronization match libc.
