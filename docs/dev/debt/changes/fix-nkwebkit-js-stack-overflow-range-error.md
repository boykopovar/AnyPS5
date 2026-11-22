# libSceNKWebKit

## add Functional

- [JS stack limit](../../../core/libs/prx/libSceNKWebKit/src/JscContext.cpp) (libSceNKWebKit) - not checked on a console. Script that runs out of stack raises a `RangeError`. The limit is set at each outermost entry into script as [`VM::updateStackLimits`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/runtime/VM.cpp#L973-L992) and [`StackBounds::recursionLimit`](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/StackBounds.h#L99-L113) do, with the default 5 MiB of use and 128 KiB reserved zone. A QuickJS frame is not the size of a JavaScriptCore frame, so the depth reached differs, and the message is the one of QuickJS. A thread whose stack the guest pthread layer cannot describe gets no limit. Closes when: compared with a console.
