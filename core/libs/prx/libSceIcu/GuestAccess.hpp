#pragma once

#include "prx/libSceIcu/Status.hpp"
#include "prx/libc/include/GuestPtr.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <string>

namespace Icu {
namespace {

class SavedErrno {
public:
    SavedErrno() : value(errno) {}
    ~SavedErrno() { errno = value; }

private:
    int value;
};

template<typename TValue>
TValue* GuestElement(TValue* base, std::size_t index) {
    const auto address = reinterpret_cast<std::uintptr_t>(base);
    if (!base || index > (std::numeric_limits<std::uintptr_t>::max() - address) / sizeof(TValue))
        APS5_INVALID_ARG_EX;
    return reinterpret_cast<TValue*>(address + index * sizeof(TValue));
}

template<typename TValue>
TValue ReadGuest(const TValue* source, std::size_t index = 0) {
    const SavedErrno savedErrno;
    TValue value;
    if (!GuestPtr<const TValue>(GuestElement(source, index)).CopyIn(value)) APS5_INVALID_ARG_EX;
    return value;
}

template<typename TValue>
void WriteGuest(TValue* destination, TValue value, std::size_t index = 0) {
    const SavedErrno savedErrno;
    if (!GuestPtr<TValue>(GuestElement(destination, index)).CopyOut(value)) APS5_INVALID_ARG_EX;
}

inline void RequireWritable(void* destination, std::size_t bytes) {
    const SavedErrno savedErrno;
    if (bytes != 0 && !GuestWritable_nid_no_patch(destination, bytes)) APS5_INVALID_ARG_EX;
}

template<typename TValue>
std::int32_t GuestStringLength(const TValue* text) {
    for (std::int32_t length = 0;; ++length) {
        if (ReadGuest(text, length) == 0) return length;
        if (length == std::numeric_limits<std::int32_t>::max()) APS5_INVALID_ARG_EX;
    }
}

template<typename TValue>
std::basic_string<TValue> CopyText(const TValue* text, std::int32_t length, bool stopAtNull = false) {
    const SavedErrno savedErrno;
    std::basic_string<TValue> copy;
    if (length == 0) return copy;
    if (length < -1 || !text) APS5_INVALID_ARG_EX;
    if (length >= 0 && !stopAtNull) {
        copy.resize(static_cast<std::size_t>(length));
        if (!GuestPtr<const TValue>(text).CopyIn(copy.data(), copy.size())) APS5_INVALID_ARG_EX;
        return copy;
    }
    for (std::int32_t index = 0; length < 0 || index < length; ++index) {
        const auto value = ReadGuest(text, index);
        if (value == 0) break;
        if (index == std::numeric_limits<std::int32_t>::max()) APS5_INVALID_ARG_EX;
        copy.push_back(value);
    }
    return copy;
}

template<typename TValue>
void TerminateGuest(TValue* destination, std::int32_t capacity, std::int32_t length, std::int32_t* status) {
    const auto value = ReadGuest(status);
    if (value > 0 || length < 0) return;
    if (length < capacity) {
        WriteGuest(destination, TValue{}, length);
        if (value == NotTerminated) WriteGuest(status, std::int32_t{0});
    } else {
        WriteGuest(status, length == capacity ? NotTerminated : BufferOverflow);
    }
}

}
}
