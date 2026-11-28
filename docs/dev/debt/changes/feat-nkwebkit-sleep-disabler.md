# libSceNKWebKit

## add Functional

- [PAL::SleepDisabler and WebCore::sleepDisablerClient](../../../core/libs/prx/libSceNKWebKit/src/PalSleepDisabler.cpp) (libSceNKWebKit) - not compared with a console module. The constructors use a host vtable `[D1, D0]` instead of the module's. They are the 3.20 `const char*` exports; the later module takes a `String`, but the 16-byte vptr-and-type object of pinned [SleepDisabler.h](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/PAL/pal/system/SleepDisabler.h#L30-L44) is unchanged, so `D0`, which frees with the guest `operator delete`, serves both. `sleepDisablerClient` returns one zero-initialized slot as [SleepDisablerClient.cpp](https://github.com/WebKit/WebKit/blob/e12a212a531aeddc11259e8515671cf5188e038d/Source/WebCore/platform/SleepDisablerClient.cpp#L33-L37) does; the `WebCore::SleepDisabler` exports still throw. Closes when: compared with a console module.

## remove | `_ZN3PAL13SleepDisablerD0Ev` | `QfBibggI2M0` |
