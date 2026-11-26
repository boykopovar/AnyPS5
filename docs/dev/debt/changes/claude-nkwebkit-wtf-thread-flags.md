# libSceNKWebKit

## add Unknown function info

- [WTF thread compilation and GC flags](../../../core/libs/prx/libSceNKWebKit/src/WtfThreadFlags.cpp) - not compared with a console module. `exchangeIsCompilationThread`, `isCompilationThread`, `registerGCThread` and `mayBeGCThread` follow [Threading.cpp:372-388](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/Threading.cpp#L372-L388) and [CompilationThread.cpp](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/CompilationThread.cpp). No `WTF::Thread` object exists here, so the one-bit compilation flag and two-bit GC type of [Threading.h:364-367](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/WTF/wtf/Threading.h#L364-L367) are host thread-local values, zero for every new thread; the GC type keeps the low two bits of the one-byte `GCThreadType`, as the bit field does.

## remove `_ZN3WTF19isCompilationThreadEv`

## remove `_ZN3WTF6Thread13mayBeGCThreadEv`

## remove `_ZN3WTF6Thread16registerGCThreadENS_12GCThreadTypeE`

## remove `_ZN3WTF6Thread27exchangeIsCompilationThreadEb`
