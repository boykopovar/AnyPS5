#include "prx/libc/include/Api.hpp"

#include <array>
#include <charconv>
#include <limits>

namespace {

template<class Integer>
void* FormatInteger(void* string, Integer value) {
    using Limits = std::numeric_limits<Integer>;
    std::array<char, 24> representation{};
    static_assert(Limits::digits10 + (Limits::is_signed ? 2u : 1u) <= representation.size() - 2u);
    auto* const first = representation.data() + 1;
    const auto result = std::to_chars(first, representation.data() + representation.size() - 1, value);
    representation[0] = static_cast<char>(2 * (result.ptr - first));
    if (!GuestCopyOut_nid_no_patch(string, representation.data(), representation.size())) {
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("to_string: unwritable result");
    }
    return string;
}

}

extern "C" void* APS5_VABI _ZNSt3__19to_stringEi_nid_postfix(void* string, std::int32_t value) {
    return FormatInteger(string, value);
}

extern "C" void* APS5_VABI _ZNSt3__19to_stringEj_nid_postfix(void* string, std::uint32_t value) {
    return FormatInteger(string, value);
}

extern "C" void* APS5_VABI _ZNSt3__19to_stringEm_nid_postfix(void* string, std::uint64_t value) {
    return FormatInteger(string, value);
}

extern "C" void* APS5_VABI _ZNSt3__19to_stringEx_nid_postfix(void* string, std::int64_t value) {
    return FormatInteger(string, value);
}
