/* Derived from Microsoft STL stl/inc/streambuf (770da5aa457af11f0a796600e1aa6f7f6fce14a6) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceLibcInternal/include/StreambufTransfer.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

namespace {

constexpr std::size_t StreambufOverflowSlot = 4;
constexpr std::size_t StreambufUflowSlot = 8;
constexpr int StreamEof = -1;

char** GetNext(GuestLocale::Streambuf* buffer) {
    return reinterpret_cast<char**>(const_cast<void*>(buffer->reserved[6]));
}

char** GetEnd(GuestLocale::Streambuf* buffer) {
    return reinterpret_cast<char**>(const_cast<void*>(buffer->reservedCounts[2]));
}

std::int32_t Available(char** next, char** end, std::size_t element) {
    if (next == nullptr || *next == nullptr || end == nullptr || *end == nullptr || *end <= *next) return 0;
    return static_cast<std::int32_t>(static_cast<std::uint32_t>(static_cast<std::size_t>(*end - *next) / element));
}

int CallUflow(GuestLocale::Streambuf* buffer) {
    const auto* slots = reinterpret_cast<const std::uintptr_t*>(buffer->vtable);
    return reinterpret_cast<int (APS5_VABI*)(GuestLocale::Streambuf*)>(slots[StreambufUflowSlot])(buffer);
}

int CallOverflow(GuestLocale::Streambuf* buffer, int element) {
    const auto* slots = reinterpret_cast<const std::uintptr_t*>(buffer->vtable);
    return reinterpret_cast<int (APS5_VABI*)(GuestLocale::Streambuf*, int)>(slots[StreambufOverflowSlot])(buffer, element);
}

template<typename TElement>
int GetElements(GuestLocale::Streambuf* self, TElement* destination, int count, const char* caller) {
    if (self == nullptr || self->vtable == nullptr || (count > 0 && destination == nullptr))
        throw std::invalid_argument(std::string(caller) + ": null stream buffer or element range");
    const int start = count;
    while (count > 0) {
        char** next = GetNext(self);
        const std::int32_t available = Available(next, GetEnd(self), sizeof(TElement));
        if (available > 0) {
            const int taken = available < count ? available : count;
            std::memcpy(destination, *next, static_cast<std::size_t>(taken) * sizeof(TElement));
            *next += static_cast<std::size_t>(taken) * sizeof(TElement);
            destination += taken;
            count -= taken;
            continue;
        }
        const int element = CallUflow(self);
        if (element == StreamEof) break;
        *destination++ = static_cast<TElement>(element);
        --count;
    }
    return start - count;
}

template<typename TElement>
int PutElements(GuestLocale::Streambuf* self, const TElement* source, int count, const char* caller) {
    if (self == nullptr || self->vtable == nullptr || (count > 0 && source == nullptr))
        throw std::invalid_argument(std::string(caller) + ": null stream buffer or element range");
    const int start = count;
    while (count > 0) {
        const std::int32_t available = Available(self->putNext, self->putEnd, sizeof(TElement));
        if (available > 0) {
            const int stored = available < count ? available : count;
            std::memcpy(*self->putNext, source, static_cast<std::size_t>(stored) * sizeof(TElement));
            *self->putNext += static_cast<std::size_t>(stored) * sizeof(TElement);
            source += stored;
            count -= stored;
            continue;
        }
        if (CallOverflow(self, static_cast<int>(*source)) == StreamEof) break;
        ++source;
        --count;
    }
    return start - count;
}

}

extern "C" {

int APS5_VABI _ZNSt15basic_streambufIcSt11char_traitsIcEE6xsgetnEPci_nid_postfix(GuestLocale::Streambuf* self, char* destination, int count) {
    return GetElements(self, reinterpret_cast<unsigned char*>(destination), count, __func__);
}

int APS5_VABI _ZNSt15basic_streambufIcSt11char_traitsIcEE6xsputnEPKci_nid_postfix(GuestLocale::Streambuf* self, const char* source, int count) {
    return PutElements(self, reinterpret_cast<const unsigned char*>(source), count, __func__);
}

int APS5_VABI _ZNSt15basic_streambufIwSt11char_traitsIwEE6xsgetnEPwi_nid_postfix(GuestLocale::Streambuf* self, std::uint16_t* destination, int count) {
    return GetElements(self, destination, count, __func__);
}

int APS5_VABI _ZNSt15basic_streambufIwSt11char_traitsIwEE6xsputnEPKwi_nid_postfix(GuestLocale::Streambuf* self, const std::uint16_t* source, int count) {
    return PutElements(self, source, count, __func__);
}

}
