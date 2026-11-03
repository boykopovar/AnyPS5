# libSceAvPlayer.native technical debt

### Functional

- [sceAvPlayerPrintf and sceAvPlayerVprintf](../../../core/libs/prx/libSceAvPlayer.native/Export.cpp) (libSceAvPlayer.native) - firmware 11.00 `libSceAvPlayer.sprx` (SHA-256 `3f1a2dbe47bb0d8d731fb670babcd9e72d4b55acf99e190e845093eb63dfbe7f`, not measured on a console) exports `sceAvPlayerPrintf` at `0x4060` (218 bytes) and `sceAvPlayerVprintf` at `0x31c0` (38 bytes). Both test a global callback pointer at `0x54068`; when it is set they call `callback(user_data at 0x54070, format, va_list)`, and otherwise forward `(format, va_list)` to the module's printf import at `0x278`, which `sceAvPlayerPrintf` reaches after building a `va_list`. The host has no AvPlayer log callback setter, so it always takes the fallback and writes to standard output; the callback path and the console's behaviour for a null format (here `-1`) are not implemented.
