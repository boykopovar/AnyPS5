# libSceAgc

## add Functional

- [sceAgcDcbResetQueue](../../../core/libs/prx/libSceAgc/DcbFlow/src/Control.cpp) lacks retail instance bounds beyond two compatibility descriptors, flag-0x400 synchronization, caller-code classification and hardware-installed clear-state defaults. Unsupported non-active instances and flag 0x400 refuse; active selection before initialization throws. Growth and partial output derive from PPSA06652 compatibility code, not fresh console measurements. Concurrent or growth-reentrant initialization timing is unmeasured on console; a host reset retains one initial state snapshot. Issue #1581 supplies 31 firmware-13.60 single-bit packet goldens; 0x3ff is a reference-derived composition, not a console transcript. Invalid host pointers, concurrent buffer writes and allocation failure are unmeasured. Packet backing is retained for late queue consumption.
