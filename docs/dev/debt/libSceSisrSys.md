# libSceSisrSys technical debt

### Callable exports

- [sceSisrSysSetup](../../../core/libs/prx/libSceSisrSys/Export.cpp) (libSceSisrSys) - firmware 13.60, device state unverified. The module's null-field stubs and internal init call are not modelled. AnyPS5 takes a pointer to four 8-byte fields at +0x00, +0x08, +0x10 and +0x18, accepts null and otherwise stores the fields, and returns 0.
- [sceSisrSysProcessUmdEvent](../../../core/libs/prx/libSceSisrSys/Export.cpp) (libSceSisrSys) - firmware 13.60, device state unverified. The module's state-dependent handling of types 0, 3 and 5 and its undefined return for a null or wrong magic are not reproduced: AnyPS5 throws for all of them, and the measured undefined value was caller register residue. Type 3 is also inconsistent between the dump and the payload log, which recorded 1 where the dump ends in `xor eax,eax`. The 11.60 run was not captured and the /dev/fen path was not verified. The event is `u32 magic` at +0x00, compared with the module's real global at base+0x134898 in the 32 KiB dump, and `u32 type` at +0x04. AnyPS5 returns the type for 1, 2, 4, 6 and 7.
