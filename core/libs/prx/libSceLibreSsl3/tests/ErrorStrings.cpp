#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "Testing.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

constexpr std::uint64_t Pack(std::uint64_t library, std::uint64_t function, std::uint64_t reason) {
    return (library & 0xFF) << 24 | (function & 0xFFF) << 12 | (reason & 0xFFF);
}

constexpr std::array<std::uint64_t, 9> Codes{
    0, Pack(2, 0, 0), Pack(13, 0, 0), Pack(13, 0xFFF, 0xA8), Pack(8, 0xFFF, 0x65), Pack(20, 0xFFF, 0x41),
    Pack(6, 0xFFF, 0x9F), 0xFFFFFFFFULL, 0x1234567890ABCDEFULL,
};

}

int main() {
    ERR_load_crypto_strings_nid_postfix();
    for (const auto code : Codes) Require(ERR_lib_error_string_nid_postfix(code) == nullptr);
    for (const auto code : Codes) Require(ERR_func_error_string_nid_postfix(code) == nullptr);
    for (const auto code : Codes) Require(ERR_reason_error_string_nid_postfix(code) == nullptr);
    for (std::uint64_t library = 0; library < 256; ++library) Require(ERR_lib_error_string_nid_postfix(Pack(library, 0, 0)) == nullptr);
    for (std::uint64_t function = 0; function < 0x1000; ++function) Require(ERR_func_error_string_nid_postfix(Pack(2, function, 0)) == nullptr);
    for (std::uint64_t reason = 0; reason < 0x1000; ++reason) Require(ERR_reason_error_string_nid_postfix(Pack(13, 0xFFF, reason)) == nullptr);
}
