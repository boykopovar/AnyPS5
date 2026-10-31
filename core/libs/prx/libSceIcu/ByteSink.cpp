// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
#include "prx/libSceIcu/libSceIcu.hpp"
#include <stdexcept>
#include <algorithm>
#include <cstring>

extern "C" {

char* APS5_VABI _ZN6icu_688ByteSink15GetAppendBufferEiiPciPi_nid_postfix(void*, std::int32_t minimum, std::int32_t, char* scratch, std::int32_t capacity, std::int32_t* resultCapacity) {
    if (!resultCapacity) throw std::invalid_argument("ByteSink: null result capacity");
    if (minimum < 1 || capacity < minimum) {
        *resultCapacity = 0;
        return nullptr;
    }
    *resultCapacity = capacity;
    return scratch;
}

void APS5_VABI _ZN6icu_688ByteSink5FlushEv_nid_postfix(void*) {
}

void APS5_VABI _ZN6icu_688ByteSinkD1Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::ByteSink);
}

void APS5_VABI _ZN6icu_688ByteSinkD2Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::ByteSink);
}

void APS5_VABI _ZN6icu_688ByteSinkD0Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::ByteSink);
    Icu::Abi::Free_nid_no_patch(object);
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD1Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::ByteSink);
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD2Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::ByteSink);
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkD0Ev_nid_postfix(void* object) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::ByteSink);
    Icu::Abi::Free_nid_no_patch(object);
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkC1EPci_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, char* buffer, std::int32_t capacity) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::CheckedArrayByteSink);
    object->buffer = buffer;
    object->capacity = std::max(capacity, 0);
    object->size = object->appended = 0;
    object->overflowed = 0;
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSinkC2EPci_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, char* buffer, std::int32_t capacity) {
    Icu::Abi::Initialize_nid_no_patch(object, Icu::Abi::Class::CheckedArrayByteSink);
    object->buffer = buffer;
    object->capacity = std::max(capacity, 0);
    object->size = object->appended = 0;
    object->overflowed = 0;
}

Icu::Abi::CheckedArrayByteSink* APS5_VABI _ZN6icu_6820CheckedArrayByteSink5ResetEv_nid_postfix(Icu::Abi::CheckedArrayByteSink* object) {
    if (!object) throw std::invalid_argument("CheckedArrayByteSink: null object");
    object->size = object->appended = 0;
    object->overflowed = 0;
    return object;
}

void APS5_VABI _ZN6icu_6820CheckedArrayByteSink6AppendEPKci_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, const char* source, std::int32_t count) {
    if (count <= 0) return;
    if (!object) throw std::invalid_argument("CheckedArrayByteSink: null object");
    if (count > INT32_MAX - object->appended) { object->appended = INT32_MAX; object->overflowed = 1; return; }
    object->appended += count;
    const auto available = object->capacity - object->size;
    if (count > available) { count = available; object->overflowed = 1; }
    if (count > 0) {
        if (!source || !object->buffer) throw std::invalid_argument("CheckedArrayByteSink: null buffer");
        auto* destination = object->buffer + object->size;
        if (source != destination) {
            const auto a = reinterpret_cast<std::uintptr_t>(source), b = reinterpret_cast<std::uintptr_t>(destination);
            if (a < b + count && b < a + count) throw std::invalid_argument("CheckedArrayByteSink: overlapping append");
            std::memcpy(destination, source, count);
        }
    }
    object->size += count;
}

char* APS5_VABI _ZN6icu_6820CheckedArrayByteSink15GetAppendBufferEiiPciPi_nid_postfix(Icu::Abi::CheckedArrayByteSink* object, std::int32_t minimum, std::int32_t, char* scratch, std::int32_t capacity, std::int32_t* result) {
    if (!result) throw std::invalid_argument("CheckedArrayByteSink: null result capacity");
    if (minimum < 1 || capacity < minimum) { *result = 0; return nullptr; }
    if (!object) throw std::invalid_argument("CheckedArrayByteSink: null object");
    const auto available = object->capacity - object->size;
    if (available >= minimum) {
        *result = available;
        if (!object->buffer) throw std::invalid_argument("CheckedArrayByteSink: null buffer");
        return object->buffer + object->size;
    }
    *result = capacity;
    return scratch;
}

}
