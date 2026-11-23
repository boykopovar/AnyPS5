#include "prx/libc/include/Api.hpp"

#include <cstddef>
#include <limits>

namespace {

static_assert(sizeof(LibcxxErrorCondition) == 16 && offsetof(LibcxxErrorCondition, category) == 8);
constexpr std::uintptr_t DefaultConditionOffset = 0x18;
using DefaultCondition = decltype(&_ZNKSt3__114error_category23default_error_conditionEi_nid_postfix);

template<typename Value>
Value ReadGuestValue(const void* source, const char* error) {
    Value value{};
    if (!GuestCopyIn_nid_no_patch(&value, source, sizeof(value))) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix(error);
    }
    return value;
}

}

extern "C" {

LibcxxErrorCondition APS5_VABI _ZNKSt3__114error_category23default_error_conditionEi_nid_postfix(
    const void* category, std::int32_t value) {
    return {value, 0, category};
}

bool APS5_VABI _ZNKSt3__114error_category10equivalentERKNS_10error_codeEi_nid_postfix(
    const void* category, const LibcxxErrorCondition* code, std::int32_t condition) {
    const auto value = ReadGuestValue<LibcxxErrorCondition>(code, "error_category::equivalent: unreadable error code");
    return value.category == category && value.value == condition;
}

bool APS5_VABI _ZNKSt3__114error_category10equivalentEiRKNS_15error_conditionE_nid_postfix(
    const void* category, std::int32_t code, const LibcxxErrorCondition* condition) {
    constexpr auto invalidCategory = "error_category::equivalent: invalid category vtable";
    const auto vtable = ReadGuestValue<std::uintptr_t>(category, invalidCategory);
    if (vtable == 0 || vtable > std::numeric_limits<std::uintptr_t>::max() - DefaultConditionOffset) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix(invalidCategory);
    }
    const auto defaultCondition = ReadGuestValue<DefaultCondition>(
        reinterpret_cast<const void*>(vtable + DefaultConditionOffset), invalidCategory);
    if (defaultCondition == nullptr) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix(invalidCategory);
    }
    const auto mapped = defaultCondition(category, code);
    const auto expected = ReadGuestValue<LibcxxErrorCondition>(condition, "error_category::equivalent: unreadable error condition");
    return mapped.category == expected.category && mapped.value == expected.value;
}

}
