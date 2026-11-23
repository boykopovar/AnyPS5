# libSceLibcInternal

## add Additional PS5 3.20 imports (xdCloudy #9498)

- [_Getdst](../../../core/libs/prx/libSceLibcInternal/Export.cpp) (libSceLibcInternal) now reads its rule string byte by byte through a guarded guest copy and throws `std::invalid_argument` when the pointer is unreadable or no NUL terminator appears within 0x10000 bytes; the console's behavior for an unreadable pointer or an unterminated string was not measured
