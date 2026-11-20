#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstdint>
#include "prx/libSceIcu/HostEnumeration.hpp"
#include "prx/libSceIcu/Configuration.hpp"

namespace {

constexpr std::int32_t UnsupportedError = 16;

bool Failed(const UEnumeration* enumeration, std::int32_t* status) {
    if (!enumeration) return true;
    if (!status) APS5_INVALID_ARG_EX;
    return *status > 0;
}

}

extern "C" {

void APS5_VABI uenum_close_nid_postfix(UEnumeration* enumeration) {
    if (!enumeration) return;
    if (!enumeration->close) {
        free_nid_postfix(enumeration);
        return;
    }
    if (enumeration->baseContext) free_nid_postfix(enumeration->baseContext);
    enumeration->close(enumeration);
}

std::int32_t APS5_VABI uenum_count_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (Failed(enumeration, status)) return -1;
    if (enumeration->count) return enumeration->count(enumeration, status);
    *status = UnsupportedError;
    return -1;
}

const char16_t* APS5_VABI uenum_unext_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (enumeration->uNext) return enumeration->uNext(enumeration, resultLength, status);
    *status = UnsupportedError;
    return nullptr;
}

const char* APS5_VABI uenum_next_nid_postfix(UEnumeration* enumeration, std::int32_t* resultLength, std::int32_t* status) {
    if (Failed(enumeration, status)) return nullptr;
    if (!enumeration->next) {
        *status = UnsupportedError;
        return nullptr;
    }
    std::int32_t ignoredLength = 0;
    return enumeration->next(enumeration, resultLength ? resultLength : &ignoredLength, status);
}

}

extern "C" UEnumeration* APS5_VABI uenum_openCharStringsEnumeration_nid_postfix(const char* const* strings,
    std::int32_t count, std::int32_t* status) {
    return Icu::OpenHostEnumeration(status, [strings, count](UErrorCode* local) {
        return uenum_openCharStringsEnumeration(strings, count, local);
    }, __func__);
}

extern "C" UEnumeration* APS5_VABI uenum_openUCharStringsEnumeration_nid_postfix(const char16_t* const* strings,
    std::int32_t count, std::int32_t* status) {
    return Icu::OpenHostEnumeration(status, [strings, count](UErrorCode* local) {
        return uenum_openUCharStringsEnumeration(strings, count, local);
    }, __func__);
}

extern "C" void APS5_VABI uenum_reset_nid_postfix(UEnumeration* enumeration, std::int32_t* status) {
    if (enumeration == nullptr || status == nullptr || *status > 0) return;
    if (enumeration->reset != nullptr) {
        enumeration->reset(enumeration, status);
        return;
    }
    *status = 16;
}

namespace Icu {
namespace {

struct EnumerationBuffer {
    std::int32_t length;
    char data[1];
};

constexpr std::int32_t EnumerationBufferPadding = 8;

void* EnumerationScratch(UEnumeration* enumeration, std::int32_t capacity) {
    if (enumeration->baseContext != nullptr) {
        if (static_cast<EnumerationBuffer*>(enumeration->baseContext)->length < capacity) {
            capacity += EnumerationBufferPadding;
            enumeration->baseContext = uprv_realloc_nid_postfix(enumeration->baseContext,
                static_cast<std::size_t>(sizeof(std::int32_t) + capacity));
            if (enumeration->baseContext == nullptr) return nullptr;
            static_cast<EnumerationBuffer*>(enumeration->baseContext)->length = capacity;
        }
    } else {
        capacity += EnumerationBufferPadding;
        enumeration->baseContext = uprv_malloc_nid_postfix(static_cast<std::size_t>(sizeof(std::int32_t) + capacity));
        if (enumeration->baseContext == nullptr) return nullptr;
        static_cast<EnumerationBuffer*>(enumeration->baseContext)->length = capacity;
    }
    return static_cast<EnumerationBuffer*>(enumeration->baseContext)->data;
}

}

}

extern "C" const char* APS5_VABI uenum_nextDefault_nid_postfix(UEnumeration* enumeration,
    std::int32_t* resultLength, std::int32_t* status) {
    if (enumeration == nullptr || status == nullptr || *status > 0) return nullptr;
    if (enumeration->uNext == nullptr) return nullptr;
    const char16_t* wide = enumeration->uNext(enumeration, resultLength, status);
    if (wide == nullptr || resultLength == nullptr) return nullptr;
    auto* target = static_cast<char*>(Icu::EnumerationScratch(enumeration,
        static_cast<std::int32_t>((*resultLength + 1) * static_cast<std::int32_t>(sizeof(char)))));
    if (target == nullptr) {
        *status = 7;
        return nullptr;
    }
    for (std::int32_t index = 0; index <= *resultLength; ++index) target[index] = static_cast<char>(wide[index]);
    return target;
}

extern "C" const char16_t* APS5_VABI uenum_unextDefault_nid_postfix(UEnumeration* enumeration,
    std::int32_t* resultLength, std::int32_t* status) {
    if (enumeration == nullptr || status == nullptr || *status > 0) return nullptr;
    if (enumeration->next == nullptr) {
        *status = 16;
        return nullptr;
    }
    std::int32_t length = 0;
    const char* narrow = enumeration->next(enumeration, &length, status);
    if (narrow == nullptr) return nullptr;
    auto* target = static_cast<char16_t*>(Icu::EnumerationScratch(enumeration,
        static_cast<std::int32_t>((length + 1) * static_cast<std::int32_t>(sizeof(char16_t)))));
    if (target == nullptr) {
        *status = 7;
        return nullptr;
    }
    for (std::int32_t index = 0; index <= length; ++index) target[index] = static_cast<char16_t>(static_cast<std::uint8_t>(narrow[index]));
    if (resultLength != nullptr) *resultLength = length;
    return target;
}
