# libSceNKWebKit

## add Functional

- [WTF::StackBounds](../../../core/libs/prx/libSceNKWebKit/src/WtfSystem.cpp) (libSceNKWebKit) - `currentThreadStackBoundsInternal` follows the OS(UNIX) branch of the pinned [StackBounds.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/StackBounds.cpp#L89-L139): it reads the calling thread's attributes through libkernel `pthread_attr_get_np` and returns `{bound + size, bound}`, so its bounds are whatever stack record libkernel holds for that thread. Where WebKit ignores a failed attribute call, this throws. The two-pointer layout follows [StackBounds.h](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/StackBounds.h#L152-L153) and is not measured on a console.

- [WTF::TimingScope](../../../core/libs/prx/libSceNKWebKit/src/WtfTime.cpp) (libSceNKWebKit) - `scopeDidEnd` follows the pinned [TimingScope.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/TimingScope.cpp): process-wide totals keyed by the name pointer, logged through `WTFLogAlways` every `m_logIterationInterval` calls. A null scope, a null name or a zero interval throws, where WebKit dereferences or divides by zero.

- [WTF::PrintStream::end](../../../core/libs/prx/libSceNKWebKit/src/NoOpExports.cpp) (libSceNKWebKit) - empty and `void` by [PrintStream.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/PrintStream.cpp#L67-L69) only; its 11.00 body was not checked.

## remove `_ZN3WTF11PrintStream3endEv`

## remove `_ZN3WTF11StackBounds32currentThreadStackBoundsInternalEv`

## remove `_ZN3WTF11TimingScope11scopeDidEndEv`
