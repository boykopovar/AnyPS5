#include "prx/libSceIcu/libSceIcu.hpp"
#include "prx/libc/include/General.hpp"
#include "unicode/uchar.h"
#include "unicode/uniset.h"
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <new>
#include <vector>

namespace {

constexpr std::int32_t SetMagic = 0x55534554;
constexpr std::int32_t MaximumCodePoint = 0x10ffff;
constexpr std::int32_t IllegalArgumentError = 1;
constexpr std::int32_t IndexOutOfBoundsError = 8;

struct GuestSet {
    std::int32_t magic = SetMagic;
    icu::UnicodeSet owned;
    const USet* borrowed{};
};

const GuestSet& Check(const USet* set) {
    if (!set) APS5_INVALID_ARG_EX;
    std::int32_t magic = 0;
    std::memcpy(&magic, set, sizeof(magic));
    if (magic != SetMagic) APS5_INVALID_ARG_EX;
    return *reinterpret_cast<const GuestSet*>(set);
}

GuestSet& Check(USet* set) {
    return const_cast<GuestSet&>(Check(static_cast<const USet*>(set)));
}

void WriteError(std::int32_t* error, std::int32_t value, const char* caller) {
    if (!GuestCopyOut_nid_no_patch(error, &value, sizeof(value))) NotImplemented_nid_no_patch(caller);
}

}

const USet* Icu::NativeSet(const USet* set) {
    const auto& guest = Check(set);
    return guest.borrowed ? guest.borrowed : guest.owned.toUSet();
}

extern "C" {

const USet* APS5_VABI u_getBinaryPropertySet_nid_postfix(std::int32_t property, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    std::int32_t incoming = 0;
    const bool readable = GuestCopyIn_nid_no_patch(&incoming, error, sizeof(incoming));
    if (!readable) NotImplemented_nid_no_patch(__func__);
    if (incoming > 0) return nullptr;
    if (property < UCHAR_BINARY_START || property >= UCHAR_BINARY_LIMIT) {
        WriteError(error, U_ILLEGAL_ARGUMENT_ERROR, __func__);
        return nullptr;
    }
    static std::array<GuestSet, UCHAR_BINARY_LIMIT> sets;
    static std::mutex mutex;
    const std::lock_guard lock(mutex);
    auto& set = sets[property];
    if (!set.borrowed) {
        auto status = U_ZERO_ERROR;
        const auto* source = u_getBinaryPropertySet(static_cast<UProperty>(property), &status);
        if (U_FAILURE(status)) {
            WriteError(error, status, __func__);
            return nullptr;
        }
        set.borrowed = source;
    }
    return reinterpret_cast<const USet*>(&set);
}

USet* APS5_VABI uset_openEmpty_nid_postfix() {
    return reinterpret_cast<USet*>(new (std::nothrow) GuestSet());
}

void APS5_VABI uset_close_nid_postfix(USet* set) {
    if (!set) return;
    auto& target = Check(set);
    if (target.borrowed) NotImplemented_nid_no_patch(__func__);
    target.magic = 0;
    delete &target;
}

void APS5_VABI uset_add_nid_postfix(USet* set, std::int32_t codePoint) {
    auto& target = Check(set);
    if (!target.borrowed) target.owned.add(codePoint);
}

void APS5_VABI uset_addRange_nid_postfix(USet* set, std::int32_t start, std::int32_t end) {
    auto& target = Check(set);
    if (!target.borrowed) target.owned.add(start, end);
}

void APS5_VABI uset_addAll_nid_postfix(USet* set, const USet* additionalSet) {
    auto& target = Check(set);
    if (!target.borrowed && set != additionalSet) target.owned.addAll(*icu::UnicodeSet::fromUSet(Icu::NativeSet(additionalSet)));
}

std::int8_t APS5_VABI uset_contains_nid_postfix(const USet* set, std::int32_t codePoint) {
    const auto* native = Icu::NativeSet(set);
    return codePoint >= 0 && codePoint <= MaximumCodePoint && uset_contains(native, codePoint);
}

std::int32_t APS5_VABI uset_getItemCount_nid_postfix(const USet* set) {
    return uset_getItemCount(Icu::NativeSet(set));
}

std::int32_t APS5_VABI uset_getItem_nid_postfix(const USet* set, std::int32_t itemIndex, std::int32_t* start, std::int32_t* end,
                                                char16_t* string, std::int32_t capacity, std::int32_t* error) {
    if (!error) APS5_INVALID_ARG_EX;
    std::int32_t incoming = 0;
    const bool readable = GuestCopyIn_nid_no_patch(&incoming, error, sizeof(incoming));
    if (!readable) NotImplemented_nid_no_patch(__func__);
    if (incoming > 0) return 0;
    if (itemIndex < 0) {
        WriteError(error, IllegalArgumentError, __func__);
        return -1;
    }
    const auto* native = Icu::NativeSet(set);
    const auto ranges = uset_getRangeCount(native);
    if (itemIndex < ranges) {
        if (!start || !end) APS5_INVALID_ARG_EX;
        auto status = static_cast<UErrorCode>(incoming);
        std::int32_t first = 0, last = 0;
        uset_getItem(native, itemIndex, &first, &last, nullptr, 0, &status);
        if (!GuestCopyOut_nid_no_patch(start, &first, sizeof(first)) || !GuestCopyOut_nid_no_patch(end, &last, sizeof(last))) APS5_INVALID_ARG_EX;
        return 0;
    }
    std::int32_t length = 0;
    const auto* units = uset_getString(native, itemIndex - ranges, &length);
    if (!units) {
        WriteError(error, IndexOutOfBoundsError, __func__);
        return -1;
    }
    std::vector<char16_t> output(string ? static_cast<std::size_t>(length) + 1u : 0u);
    auto status = static_cast<UErrorCode>(incoming);
    icu::UnicodeString(false, units, length).extract(string ? output.data() : nullptr, capacity, status);
    if (string && U_SUCCESS(status)) {
        const auto count = static_cast<std::size_t>(length) + (capacity > length);
        if (!GuestCopyOut_nid_no_patch(string, output.data(), count * sizeof(char16_t))) APS5_INVALID_ARG_EX;
    }
    if (status != incoming) WriteError(error, status, __func__);
    return length;
}

}
