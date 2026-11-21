# libSceNKWebKit

## add Unknown function info

- [PAL clock](../../../core/libs/prx/libSceNKWebKit/src/PalClock.cpp) (libSceNKWebKit) - Console ABI and behavior are unmeasured; RTTI is not reconstructed. The 48-byte object, nine SysV callbacks and nontrivial unique_ptr result slot follow [ClockGeneric.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WebCore/PAL/pal/system/ClockGeneric.cpp) and its ClockGeneric.h/Clock.h headers. Timing uses the existing WTF monotonic clock. The deleting callback requires a live factory allocation; state-accessing callbacks reject unreadable or unwritable state. Closes when: ABI, RTTI, and playback behavior are measured on console.
