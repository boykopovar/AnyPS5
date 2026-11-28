#include "prx/libc/include/Api.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, const char* message) {
    if (condition) {
        return;
    }
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

}

int main() {
    std::array<unsigned char, 24> shortString{8, 'a', 'b', 0, 'c'};
    LibcException::ExceptionObject complete{};
    _ZNSt13runtime_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &complete, shortString.data());
    shortString.fill(0);
    Require(std::strcmp(_ZNKSt13runtime_error4whatEv_nid_postfix(&complete), "ab") == 0,
            "complete constructor must copy the short string up to its embedded zero");
    _ZNSt13runtime_errorD1Ev_nid_postfix(&complete);

    constexpr char Original[] = "An exception diagnostic from a long guest string";
    std::array<char, 64> longText{};
    std::memcpy(longText.data(), Original, sizeof(Original));
    const std::array<std::uint64_t, 3> longString{
        longText.size() | 1u, sizeof(Original) - 1, reinterpret_cast<std::uintptr_t>(longText.data()),
    };
    LibcException::ExceptionObject base{};
    _ZNSt13runtime_errorC2ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
        &base, longString.data());
    longText.fill('x');
    Require(std::strcmp(_ZNKSt13runtime_error4whatEv_nid_postfix(&base), Original) == 0,
            "base constructor must own the long diagnostic independently of the guest string");
    _ZNSt13runtime_errorD2Ev_nid_postfix(&base);

    LibcException::ExceptionObject invalid{};
    const std::array<unsigned char, 24> invalidShort{46};
    bool rejected = false;
    try {
        _ZNSt13runtime_errorC1ERKNSt3__112basic_stringIcNS0_11char_traitsIcEENS0_9allocatorIcEEEE_nid_postfix(
            &invalid, invalidShort.data());
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    Require(rejected, "constructor must reject an invalid short-string length");
    std::puts("libc++ runtime_error string constructor ABI checks passed");
    return 0;
}
