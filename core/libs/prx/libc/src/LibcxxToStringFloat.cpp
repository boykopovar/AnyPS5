#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstring>

namespace {

struct ShortString {
    unsigned char flags;
    std::array<char, 23> data;
};

struct LongString {
    std::uint64_t capacityWithFlag;
    std::uint64_t size;
    char* data;
};

static_assert(sizeof(ShortString) == 24 && sizeof(LongString) == 24);

constexpr std::size_t LongStringAllocationSize = 48;

}

extern "C" void* APS5_VABI _ZNSt3__19to_stringEf_nid_postfix(void* result, float value) {
    std::array<char, LongStringAllocationSize> text{};
    const int length = snprintf_nid_postfix(text.data(), text.size(), "%f", static_cast<double>(value));
    if (length < 0 || static_cast<std::size_t>(length) >= text.size()) {
        _ZSt15_Xruntime_errorPKc_nid_postfix("to_string: float formatting failed");
    }

    const auto size = static_cast<std::size_t>(length);
    ShortString small{};
    if (size < small.data.size()) {
        small.flags = static_cast<unsigned char>(size << 1u);
        std::memcpy(small.data.data(), text.data(), size + 1);
        if (!GuestCopyOut_nid_no_patch(result, &small, sizeof(small))) {
            _ZSt18_Xinvalid_argumentPKc_nid_postfix("to_string: unwritable short string");
        }
        return result;
    }

    auto* data = static_cast<char*>(_Znwm_nid_postfix(LongStringAllocationSize));
    std::memcpy(data, text.data(), size + 1);
    const LongString large{LongStringAllocationSize | 1u, size, data};
    if (!GuestCopyOut_nid_no_patch(result, &large, sizeof(large))) {
        _ZdlPv_nid_postfix(data);
        _ZSt18_Xinvalid_argumentPKc_nid_postfix("to_string: unwritable long string");
    }
    return result;
}
