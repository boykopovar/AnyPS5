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
    return std::min(count, source->size - position);
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

}
