#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string_view>

namespace {

constexpr auto ToInt = _ZNSt3__14stoiERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi_nid_postfix;
constexpr auto ToULong = _ZNSt3__15stoulERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi_nid_postfix;
constexpr auto ToULongLong = _ZNSt3__16stoullERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPmi_nid_postfix;
constexpr auto ToFloat = _ZNSt3__14stofERKNS_12basic_stringIcNS_11char_traitsIcEENS_9allocatorIcEEEEPm_nid_postfix;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

std::array<char, 24> ShortString(std::string_view text) {
    Require(text.size() <= 22, "fixture string exceeds short-string storage");
    std::array<char, 24> value{};
    value[0] = static_cast<char>(text.size() << 1);
    std::memcpy(value.data() + 1, text.data(), text.size());
    return value;
}

template <typename Error, typename Operation>
void RequireError(Operation operation, const char* message) {
    try {
        operation();
    } catch (const Error& error) {
        const auto* guest = reinterpret_cast<const LibcException::ExceptionObject*>(&error);
        Require(std::strcmp(guest->message, message) == 0, "conversion error message differs");
        return;
    }
    Require(false, "conversion did not throw the expected exception");
}

}

int main() {
    std::uint64_t read = 0;
    *__error_nid_postfix() = 7;
    const auto hex = ShortString(" \t-0x2a!");
    Require(ToInt(hex.data(), &read, 0) == -42 && read == 7, "stoi hex value or index differs");
    Require(*__error_nid_postfix() == 7, "stoi changed errno on success");

    const auto maximum = ShortString("18446744073709551615x");
    Require(ToULong(maximum.data(), &read, 10) == UINT64_MAX && read == 20, "stoul lost LP64 range");
    Require(ToULongLong(maximum.data(), &read, 10) == UINT64_MAX && read == 20, "stoull lost 64-bit range");
    const auto negative = ShortString("-1");
    Require(ToULong(negative.data(), nullptr, 10) == UINT64_MAX, "stoul sign differs");

    constexpr char LongText[] = "                          12.5tail";
    const std::array<std::uint64_t, 3> longString{
        65, sizeof(LongText) - 1, reinterpret_cast<std::uintptr_t>(LongText),
    };
    Require(ToFloat(longString.data(), &read) == 12.5f && read == sizeof(LongText) - 5,
            "stof long-string value or index differs");
    const auto embeddedNull = ShortString(std::string_view{"1\0.5", 4});
    Require(ToFloat(embeddedNull.data(), &read) == 1.0f && read == 1, "stof read past the embedded NUL");

    read = 91;
    const auto invalid = ShortString("words");
    RequireError<std::invalid_argument>([&] { ToInt(invalid.data(), &read, 10); }, "stoi: no conversion");
    Require(read == 91 && *__error_nid_postfix() == 7, "failed parsing changed the index or errno");

    const auto beyondInt = ShortString("2147483648!");
    RequireError<std::out_of_range>([&] { ToInt(beyondInt.data(), &read, 10); }, "stoi: out of range");
    Require(read == 10 && *__error_nid_postfix() == 7, "stoi did not keep the consumed index");

    read = 91;
    const auto beyondULong = ShortString("18446744073709551616");
    RequireError<std::out_of_range>([&] { ToULong(beyondULong.data(), &read, 10); }, "stoul: out of range");
    RequireError<std::out_of_range>([&] { ToULongLong(beyondULong.data(), &read, 10); }, "stoull: out of range");
    const auto hugeFloat = ShortString("1e100");
    RequireError<std::out_of_range>([&] { ToFloat(hugeFloat.data(), &read); }, "stof: out of range");
    Require(read == 91 && *__error_nid_postfix() == 7, "range failure changed the index or errno");
    std::puts("libc++ string conversion ABI checks passed");
    return 0;
}
