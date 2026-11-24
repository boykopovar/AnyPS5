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

USet* NativeSet(USet* set) {
    auto& guest = Check(set);
    return guest.borrowed ? const_cast<USet*>(guest.borrowed) : guest.owned.toUSet();
}

const char16_t* Text(const char16_t* text, std::int32_t length) {
    if (text) return text;
    if (length != 0) APS5_INVALID_ARG_EX;
    static constexpr char16_t empty{};
    return &empty;
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

USet* APS5_VABI uset_clone_nid_postfix(const USet* set) {
    const auto* source = Icu::NativeSet(set);
    auto* clone = new (std::nothrow) GuestSet();
    if (!clone) return nullptr;
    clone->owned = *icu::UnicodeSet::fromUSet(source);
    if (uset_isFrozen(source)) clone->owned.freeze();
    return reinterpret_cast<USet*>(clone);
}

USet* APS5_VABI uset_cloneAsThawed_nid_postfix(const USet* set) {
    auto* thawed = icu::UnicodeSet::fromUSet(Icu::NativeSet(set))->cloneAsThawed();
    if (!thawed) return nullptr;
    auto* clone = new (std::nothrow) GuestSet();
    if (!clone) {
        delete thawed;
        return nullptr;
    }
    clone->owned = *thawed;
    delete thawed;
    return reinterpret_cast<USet*>(clone);
}

void APS5_VABI uset_close_nid_postfix(USet* set) {
    if (!set) return;
    auto& target = Check(set);
    if (target.borrowed) NotImplemented_nid_no_patch(__func__);
    target.magic = 0;
    delete &target;
}

void APS5_VABI uset_freeze_nid_postfix(USet* set) {
    auto& target = Check(set);
    if (!target.borrowed) target.owned.freeze();
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

void APS5_VABI uset_addString_nid_postfix(USet* set, const char16_t* string, std::int32_t length) {
    uset_addString(NativeSet(set), Text(string, length), length);
}

void APS5_VABI uset_addAllCodePoints_nid_postfix(USet* set, const char16_t* string, std::int32_t length) {
    uset_addAllCodePoints(NativeSet(set), Text(string, length), length);
}

void APS5_VABI uset_clear_nid_postfix(USet* set) {
    uset_clear(NativeSet(set));
}

void APS5_VABI uset_compact_nid_postfix(USet* set) {
    uset_compact(NativeSet(set));
}

void APS5_VABI uset_complement_nid_postfix(USet* set) {
    uset_complement(NativeSet(set));
}

void APS5_VABI uset_complementRange_nid_postfix(USet* set, std::int32_t start, std::int32_t end) {
    uset_complementRange(NativeSet(set), start, end);
}

std::int32_t APS5_VABI uset_indexOf_nid_postfix(const USet* set, std::int32_t codePoint) {
    return uset_indexOf(Icu::NativeSet(set), codePoint);
}

std::int32_t APS5_VABI uset_charAt_nid_postfix(const USet* set, std::int32_t index) {
    return uset_charAt(Icu::NativeSet(set), index);
}

std::int32_t APS5_VABI uset_getStringCount_nid_postfix(const USet* set) {
    return uset_getStringCount(Icu::NativeSet(set));
}

std::int8_t APS5_VABI uset_contains_nid_postfix(const USet* set, std::int32_t codePoint) {
    const auto* native = Icu::NativeSet(set);
    return codePoint >= 0 && codePoint <= MaximumCodePoint && uset_contains(native, codePoint);
}

std::int8_t APS5_VABI uset_containsRange_nid_postfix(const USet* set, std::int32_t start, std::int32_t end) {
    return uset_containsRange(Icu::NativeSet(set), start, end);
}

std::int8_t APS5_VABI uset_equals_nid_postfix(const USet* first, const USet* second) {
    return uset_equals(Icu::NativeSet(first), Icu::NativeSet(second));
}

std::int8_t APS5_VABI uset_containsAll_nid_postfix(const USet* set, const USet* other) {
    return uset_containsAll(Icu::NativeSet(set), Icu::NativeSet(other));
}

std::int8_t APS5_VABI uset_containsNone_nid_postfix(const USet* set, const USet* other) {
    return uset_containsNone(Icu::NativeSet(set), Icu::NativeSet(other));
}

std::int8_t APS5_VABI uset_containsSome_nid_postfix(const USet* set, const USet* other) {
    return uset_containsSome(Icu::NativeSet(set), Icu::NativeSet(other));
}

std::int8_t APS5_VABI uset_containsString_nid_postfix(const USet* set, const char16_t* string, std::int32_t length) {
    return uset_containsString(Icu::NativeSet(set), Text(string, length), length);
}

std::int8_t APS5_VABI uset_hasStrings_nid_postfix(const USet* set) {
    return uset_hasStrings(Icu::NativeSet(set));
}

std::int8_t APS5_VABI uset_isFrozen_nid_postfix(const USet* set) {
    return uset_isFrozen(Icu::NativeSet(set));
}

std::int32_t APS5_VABI uset_span_nid_postfix(const USet* set, const char16_t* text, std::int32_t length, std::int32_t condition) {
    const auto* native = Icu::NativeSet(set);
    if (!text) {
        if (length != 0) APS5_INVALID_ARG_EX;
        return 0;
    }
    return uset_span(native, text, length, static_cast<USetSpanCondition>(condition));
}

std::int32_t APS5_VABI uset_spanBack_nid_postfix(const USet* set, const char16_t* text, std::int32_t length, std::int32_t condition) {
    const auto* native = Icu::NativeSet(set);
    if (!text) {
        if (length != 0) APS5_INVALID_ARG_EX;
        return 0;
    }
    return uset_spanBack(native, text, length, static_cast<USetSpanCondition>(condition));
}

std::int32_t APS5_VABI uset_spanUTF8_nid_postfix(const USet* set, const char* text, std::int32_t length, std::int32_t condition) {
    const auto* native = Icu::NativeSet(set);
    if (!text) {
        if (length != 0) APS5_INVALID_ARG_EX;
        return 0;
    }
    return uset_spanUTF8(native, text, length, static_cast<USetSpanCondition>(condition));
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

std::int8_t APS5_VABI uset_isEmpty_nid_postfix(const USet* set) {
    return uset_isEmpty(Icu::NativeSet(set));
}

void APS5_VABI uset_remove_nid_postfix(USet* set, std::int32_t codePoint) {
    uset_remove(NativeSet(set), codePoint);
}

void APS5_VABI uset_removeRange_nid_postfix(USet* set, std::int32_t start, std::int32_t end) {
    uset_removeRange(NativeSet(set), start, end);
}

void APS5_VABI uset_removeString_nid_postfix(USet* set, const char16_t* string, std::int32_t length) {
    uset_removeString(NativeSet(set), Text(string, length), length);
}

void APS5_VABI uset_removeAll_nid_postfix(USet* set, const USet* remove) {
    uset_removeAll(NativeSet(set), Icu::NativeSet(remove));
}

void APS5_VABI uset_removeAllStrings_nid_postfix(USet* set) {
    uset_removeAllStrings(NativeSet(set));
}

void APS5_VABI uset_retain_nid_postfix(USet* set, std::int32_t start, std::int32_t end) {
    uset_retain(NativeSet(set), start, end);
}

void APS5_VABI uset_retainAll_nid_postfix(USet* set, const USet* retain) {
    uset_retainAll(NativeSet(set), Icu::NativeSet(retain));
}

void APS5_VABI uset_complementAll_nid_postfix(USet* set, const USet* complement) {
    uset_complementAll(NativeSet(set), Icu::NativeSet(complement));
}

std::int32_t APS5_VABI uset_size_nid_postfix(const USet* set) {
    return uset_size(Icu::NativeSet(set));
}

}
