# libc

## add Functional

- [Exceptions raised inside libstdc++-6.dll](../../../core/libs/prx/libc/src/specifics/windows/RuntimeThrowHandoff.cpp) (libc) - on Windows, an exception larger than 512 bytes, one from libstdc++'s emergency pool, a dependent exception from `std::rethrow_exception` inside the DLL, or one crossing a runtime frame whose handler is not `__gxx_personality_seh0` still ends in the `cannot be delivered` report. The handoff that delivers the rest assumes the GCC 15 libstdc++ exception header (160 bytes before the object, type 80 bytes before the unwind header), checked through its reference count and type, and copies the object bytewise, which holds for the standard library types the DLL throws. Handlers in `KERNELBASE.dll` frames are passed over, as they return `ExceptionContinueSearch` for GCC exceptions.
