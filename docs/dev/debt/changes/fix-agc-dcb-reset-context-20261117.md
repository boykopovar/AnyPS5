# libSceAgc

## add Functional

- [sceAgcDcbResetQueue](../../../core/libs/prx/libSceAgc/DcbFlow/src/Control.cpp) lacks retail instance bounds beyond two bundled compatibility descriptors, flag-0x400 synchronization, registered caller-code classification and hardware-installed clear-state defaults. Unsupported non-active instances and flag 0x400 use the standard refusal; active selection before initialization throws. Indexed loads use versioned public/internal tables and restore the counter group only when requested. Growth and partial output follow static PPSA06652 compatibility-code analysis, not fresh console measurements. Issue #1581 supplies 31 firmware-13.60 single-bit packet goldens; the full 0x3ff fixture is a reference-derived composition, not a supplied console transcript. Invalid host pointers, concurrent buffer writes and allocation failure are unmeasured. Packet backing remains alive for late queue consumption.
