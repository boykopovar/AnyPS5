#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/GuestStringStorage.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <limits>

namespace {

using GuestLocale::String;

constexpr std::size_t NoPosition = std::numeric_limits<std::size_t>::max();
constexpr std::size_t MaxSize = NoPosition - 1;
constexpr std::size_t WideInlineCapacity = 7;
constexpr std::size_t MaxWideSize = NoPosition / 2 - 1;

char* Data(String* string) {
    return string->capacity > GuestLocale::StringInlineCapacity ? string->pointer : string->buffer;
}

const char* Data(const String* string) {
    return string->capacity > GuestLocale::StringInlineCapacity ? string->pointer : string->buffer;
}

void SetSize(String* string, std::size_t size) {
    string->size = size;
    Data(string)[size] = 0;
}

void Reserve(String* string, std::size_t size) {
    if (size > MaxSize) _ZNKSs5_XlenEv_nid_postfix(string);
    if (size <= string->capacity) return;
    const std::size_t capacity = std::min(size | GuestLocale::StringCapacityMask, MaxSize);
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    char* old = Data(string);
    std::memcpy(data, old, string->size + 1);
    if (string->capacity > GuestLocale::StringInlineCapacity) GuestStringStorage::Deallocate(old, string->capacity + 1);
    string->pointer = data;
    string->capacity = capacity;
}

std::size_t Clamp(const String* source, std::size_t position, std::size_t count) {
    if (position > source->size) _ZNKSs5_XranEv_nid_postfix(source);
    return std::min<std::size_t>(count, source->size - position);
}

void EraseRange(String* string, std::size_t position, std::size_t count) {
    char* data = Data(string);
    const std::size_t tail = string->size - position - count;
    if (count != 0 && tail != 0) std::memmove(data + position, data + position + count, tail);
    SetSize(string, string->size - count);
}

void Assign(String* string, const char* source, std::size_t count) {
    if (count <= string->capacity) {
        if (count != 0) std::memmove(Data(string), source, count);
        SetSize(string, count);
        return;
    }
    if (count > MaxSize) _ZNKSs5_XlenEv_nid_postfix(string);
    const std::size_t capacity = std::min(count | GuestLocale::StringCapacityMask, MaxSize);
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(capacity + 1));
    std::memcpy(data, source, count);
    data[count] = 0;
    char* old = Data(string);
    const std::size_t oldCapacity = string->capacity;
    if (oldCapacity > GuestLocale::StringInlineCapacity) GuestStringStorage::Deallocate(old, oldCapacity + 1);
    string->pointer = data;
    string->capacity = capacity;
    string->size = count;
}

char* WideData(String* string) {
    return string->capacity > WideInlineCapacity ? string->pointer : string->buffer;
}

const char* WideData(const String* string) {
    return string->capacity > WideInlineCapacity ? string->pointer : string->buffer;
}

void SetWideSize(String* string, std::size_t size) {
    string->size = size;
    const std::uint16_t terminator = 0;
    std::memcpy(WideData(string) + size * sizeof(terminator), &terminator, sizeof(terminator));
}

std::size_t WideCapacityFor(const String* string, std::size_t size) {
    if (size > MaxWideSize) _ZNKSs5_XlenEv_nid_postfix(string);
    const std::size_t rounded = size | std::size_t{7};
    if (rounded < size || rounded > MaxWideSize) return size;
    std::size_t capacity = rounded;
    if (string->capacity > WideInlineCapacity && string->capacity / 2 > rounded / 3) {
        if (string->capacity > MaxWideSize - string->capacity / 2) _ZSt11_Xbad_allocv_nid_postfix();
        capacity = string->capacity + string->capacity / 2;
    }
    return capacity;
}

std::size_t WideAllocationBytes(const String* string, std::size_t capacity) {
    if (capacity > MaxWideSize) _ZNKSs5_XlenEv_nid_postfix(string);
    const std::size_t bytes = (capacity + 1) * sizeof(std::uint16_t);
    if (bytes >= GuestStringStorage::LargeAllocation && bytes > NoPosition - GuestStringStorage::AllocationPadding) _ZSt11_Xbad_allocv_nid_postfix();
    return bytes;
}

void ReserveWide(String* string, std::size_t size) {
    if (size > MaxWideSize) _ZNKSs5_XlenEv_nid_postfix(string);
    if (size <= string->capacity) return;
    const std::size_t capacity = WideCapacityFor(string, size);
    const std::size_t bytes = WideAllocationBytes(string, capacity);
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(bytes));
    const std::size_t used = (string->size + 1) * sizeof(std::uint16_t);
    std::memcpy(data, WideData(string), used);
    char* old = WideData(string);
    const std::size_t oldCapacity = string->capacity;
    if (oldCapacity > WideInlineCapacity) GuestStringStorage::Deallocate(old, (oldCapacity + 1) * sizeof(std::uint16_t));
    string->pointer = data;
    string->capacity = capacity;
}

std::size_t ClampWide(const String* source, std::size_t position, std::size_t count) {
    if (position > source->size) _ZNKSs5_XranEv_nid_postfix(source);
    return std::min(count, source->size - position);
}

void EraseWideRange(String* string, std::size_t position, std::size_t count) {
    char* data = WideData(string);
    const std::size_t tail = string->size - position - count;
    if (count != 0 && tail != 0) {
        std::memmove(data + position * sizeof(std::uint16_t), data + (position + count) * sizeof(std::uint16_t), tail * sizeof(std::uint16_t));
    }
    SetWideSize(string, string->size - count);
}

void FillWide(char* data, std::size_t position, std::size_t count, std::uint16_t character) {
    for (std::size_t index = 0; index < count; ++index) {
        std::memcpy(data + (position + index) * sizeof(character), &character, sizeof(character));
    }
}

void AssignWide(String* string, const char* source, std::size_t count) {
    const std::uintptr_t sourceAddress = reinterpret_cast<std::uintptr_t>(source);
    const std::uintptr_t dataAddress = reinterpret_cast<std::uintptr_t>(WideData(string));
    const std::size_t sourceBytes = string->size * sizeof(std::uint16_t);
    const bool aliases = sourceAddress >= dataAddress && sourceAddress - dataAddress < sourceBytes;
    if (aliases) count = std::min(count, string->size - (sourceAddress - dataAddress) / sizeof(std::uint16_t));
    if (count > MaxWideSize) _ZNKSs5_XlenEv_nid_postfix(string);
    const std::size_t bytes = count * sizeof(std::uint16_t);
    if (count <= string->capacity) {
        if (count != 0) std::memmove(WideData(string), source, bytes);
        SetWideSize(string, count);
        return;
    }
    const std::size_t capacity = WideCapacityFor(string, count);
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(WideAllocationBytes(string, capacity)));
    std::memcpy(data, source, bytes);
    const std::uint16_t terminator = 0;
    std::memcpy(data + bytes, &terminator, sizeof(terminator));
    char* old = WideData(string);
    const std::size_t oldCapacity = string->capacity;
    if (oldCapacity > WideInlineCapacity) GuestStringStorage::Deallocate(old, (oldCapacity + 1) * sizeof(std::uint16_t));
    string->pointer = data;
    string->capacity = capacity;
    string->size = count;
}

void ReserveForInsert(String* string, std::size_t size) {
    if (size > MaxSize) _ZNKSs5_XlenEv_nid_postfix(string);
    if (size <= string->capacity) return;
    const std::size_t rounded = size | GuestLocale::StringCapacityMask;
    std::size_t capacity = rounded;
    if (rounded < size || rounded > MaxSize) {
        capacity = size;
    } else if (string->capacity > GuestLocale::StringInlineCapacity && string->capacity / 2 > rounded / 3) {
        if (string->capacity > MaxSize - string->capacity / 2) _ZSt11_Xbad_allocv_nid_postfix();
        capacity = string->capacity + string->capacity / 2;
    }
    const std::size_t bytes = capacity + 1;
    if (bytes >= GuestStringStorage::LargeAllocation && bytes > NoPosition - GuestStringStorage::AllocationPadding) _ZSt11_Xbad_allocv_nid_postfix();
    auto* data = static_cast<char*>(GuestStringStorage::Allocate(bytes));
    char* old = Data(string);
    std::memcpy(data, old, string->size + 1);
    const std::size_t oldCapacity = string->capacity;
    if (oldCapacity > GuestLocale::StringInlineCapacity) GuestStringStorage::Deallocate(old, oldCapacity + 1);
    string->pointer = data;
    string->capacity = capacity;
}

}

extern "C" {

[[noreturn]] void APS5_VABI _ZNKSs5_XranEv_nid_postfix(const String*) {
    _ZSt14_Xout_of_rangePKc_nid_postfix("invalid string position");
}

[[noreturn]] void APS5_VABI _ZNKSs5_XlenEv_nid_postfix(const String*) {
    _ZSt14_Xlength_errorPKc_nid_postfix("string too long");
}

String* APS5_VABI _ZNSs5eraseEmm_nid_postfix(String* self, std::size_t position, std::size_t count) {
    EraseRange(self, position, Clamp(self, position, count));
    return self;
}

String* APS5_VABI _ZNSs6appendERKSsmm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    count = Clamp(source, position, count);
    if (count >= NoPosition - self->size) _ZNKSs5_XlenEv_nid_postfix(self);
    if (count == 0) return self;
    const std::size_t size = self->size;
    Reserve(self, size + count);
    std::memmove(Data(self) + size, Data(source) + position, count);
    SetSize(self, size + count);
    return self;
}

String* APS5_VABI _ZNSs6appendEmc_nid_postfix(String* self, std::size_t count, char character) {
    if (count >= NoPosition - self->size) _ZNKSs5_XlenEv_nid_postfix(self);
    if (count == 0) return self;
    const std::size_t size = self->size;
    Reserve(self, size + count);
    std::memset(Data(self) + size, character, count);
    SetSize(self, size + count);
    return self;
}

String* APS5_VABI _ZNSs6assignERKSsmm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    count = Clamp(source, position, count);
    if (source != self) {
        Assign(self, Data(source) + position, count);
        return self;
    }
    EraseRange(self, position + count, self->size - position - count);
    EraseRange(self, 0, position);
    return self;
}

String* APS5_VABI _ZNSs6assignEPKcm_nid_postfix(String* self, const char* source, std::size_t count) {
    Assign(self, source, count);
    return self;
}

String* APS5_VABI _ZNSs6assignEmc_nid_postfix(String* self, std::size_t count, char character) {
    Reserve(self, count);
    if (count != 0) std::memset(Data(self), character, count);
    SetSize(self, count);
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE5eraseEmm_nid_postfix(String* self, std::size_t position, std::size_t count) {
    EraseWideRange(self, position, ClampWide(self, position, count));
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6appendERKS2_mm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    count = ClampWide(source, position, count);
    if (count > MaxWideSize - self->size) _ZNKSs5_XlenEv_nid_postfix(self);
    if (count == 0) return self;
    const std::size_t size = self->size;
    ReserveWide(self, size + count);
    std::memmove(WideData(self) + size * sizeof(std::uint16_t), WideData(source) + position * sizeof(std::uint16_t), count * sizeof(std::uint16_t));
    SetWideSize(self, size + count);
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6appendEmw_nid_postfix(String* self, std::size_t count, std::uint16_t character) {
    if (count > MaxWideSize - self->size) _ZNKSs5_XlenEv_nid_postfix(self);
    if (count == 0) return self;
    const std::size_t size = self->size;
    ReserveWide(self, size + count);
    FillWide(WideData(self), size, count, character);
    SetWideSize(self, size + count);
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6assignEPKwm_nid_postfix(String* self, const std::uint16_t* source, std::size_t count) {
    AssignWide(self, reinterpret_cast<const char*>(source), count);
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6assignERKS2_mm_nid_postfix(String* self, const String* source, std::size_t position, std::size_t count) {
    count = ClampWide(source, position, count);
    if (source != self) {
        AssignWide(self, WideData(source) + position * sizeof(std::uint16_t), count);
        return self;
    }
    EraseWideRange(self, position + count, self->size - position - count);
    EraseWideRange(self, 0, position);
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6assignEmw_nid_postfix(String* self, std::size_t count, std::uint16_t character) {
    ReserveWide(self, count);
    FillWide(WideData(self), 0, count, character);
    SetWideSize(self, count);
    return self;
}

String* APS5_VABI _ZNSbIwSt11char_traitsIwESaIwEE6insertEmmw_nid_postfix(String* self, std::size_t position, std::size_t count, std::uint16_t character) {
    if (position > self->size) _ZNKSs5_XranEv_nid_postfix(self);
    if (count > MaxWideSize - self->size) _ZNKSs5_XlenEv_nid_postfix(self);
    if (count == 0) return self;
    const std::size_t size = self->size;
    ReserveWide(self, size + count);
    char* data = WideData(self);
    std::memmove(data + (position + count) * sizeof(std::uint16_t), data + position * sizeof(std::uint16_t), (size - position + 1) * sizeof(std::uint16_t));
    FillWide(data, position, count, character);
    SetWideSize(self, size + count);
    return self;
}

String* APS5_VABI _ZNSs6insertEmmc_nid_postfix(String* self, std::size_t position, std::size_t count, char character) {
    if (position > self->size) _ZNKSs5_XranEv_nid_postfix(self);
    if (count > MaxSize - self->size) _ZNKSs5_XlenEv_nid_postfix(self);
    if (count == 0) return self;
    const std::size_t size = self->size;
    ReserveForInsert(self, size + count);
    char* data = Data(self);
    std::memmove(data + position + count, data + position, size - position + 1);
    std::memset(data + position, character, count);
    SetSize(self, size + count);
    return self;
}

}
