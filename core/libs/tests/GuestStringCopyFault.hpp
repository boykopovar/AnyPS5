#pragma once

#include "prx/libc/include/GuestLocale.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <new>

namespace GuestStringCopyFault {
inline bool recording = false;
inline bool failCopy = false;
inline bool throwOnRelease = false;
inline bool resultReady = false;
inline void* raw = nullptr;
inline void* storage = nullptr;
inline std::size_t size = 0;
inline std::size_t capacity = 0;
inline std::size_t releases = 0;
inline std::atomic<bool> armed{false};
inline std::atomic<std::size_t> failedHostBytes{0};
struct ReleaseMarker {};

inline void Begin(bool fail, bool releaseThrows = false) {
    recording = true;
    failCopy = fail;
    throwOnRelease = releaseThrows;
    resultReady = false;
    raw = nullptr;
    storage = nullptr;
    size = capacity = releases = 0;
    armed = false;
    failedHostBytes = 0;
}

inline void Result(const GuestLocale::String& value) {
    if (!recording) return;
    size = value.size;
    capacity = value.capacity;
    storage = value.capacity > 15 ? value.pointer : nullptr;
    raw = value.capacity + 1 >= 0x1000 ? static_cast<void**>(storage)[-1] : storage;
    resultReady = true;
#if defined(__linux__)
    armed = failCopy;
#endif
}

inline bool Released(void* pointer) {
    if (!recording || raw == nullptr || pointer != raw) return false;
    ++releases;
    return throwOnRelease;
}

inline void Disarm() {
    armed = false;
    recording = false;
}
}

#if defined(__linux__)
void* operator new(std::size_t bytes) {
    if (bytes != 0 && GuestStringCopyFault::armed.exchange(false)) {
        GuestStringCopyFault::failedHostBytes = bytes;
        throw std::bad_alloc();
    }
    if (void* pointer = std::malloc(bytes ? bytes : 1)) return pointer;
    throw std::bad_alloc();
}
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
#endif
