#include "prx/libSceAgc/Command/include/RegisterDefaults.hpp"
#include "prx/libc/include/Shutdown.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <stdexcept>
#include <vector>

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void testReferenceRanges() {
    const std::array<ShaderRegister, 3> first{{{2, 0x22222222u}, {5, 0x55555555u}, {9, 0x99999999u}}};
    const std::array<ShaderRegister, 3> second{{{0, 0x12345678u}, {1, 0xabcdef01u}, {4, 0x11223344u}}};
    struct Case {
        unsigned instance;
        std::uint32_t first;
        std::uint32_t last;
        std::vector<std::uint32_t> prefix;
    };
    const std::array<Case, 36> cases{{
        {0u, 0u, 0u, {}},
        {0u, 4u, 4u, {}},
        {0u, 8u, 4u, {}},
        {0u, 0u, 1u, {}},
        {0u, 0u, 5u, {}},
        {0u, 0u, 16u, {}},
        {0u, 2u, 10u, {}},
        {0u, 3u, 8u, {}},
        {0u, 9u, 10u, {}},
        {0u, 10u, 16u, {}},
        {0u, 20u, 24u, {}},
        {0u, 4294967294u, 4294967295u, {}},
        {1u, 0u, 0u, {}},
        {1u, 4u, 4u, {}},
        {1u, 8u, 4u, {}},
        {1u, 0u, 1u, {0x00000000u}},
        {1u, 0u, 5u, {0x00000000u, 0x00000000u, 0x22222222u, 0x00000000u, 0x00000000u}},
        {1u, 0u, 16u, {0x00000000u, 0x00000000u, 0x22222222u, 0x00000000u, 0x00000000u, 0x55555555u, 0x00000000u, 0x00000000u, 0x00000000u, 0x99999999u, 0x00000000u}},
        {1u, 2u, 10u, {0x22222222u, 0x00000000u, 0x00000000u, 0x55555555u, 0x00000000u, 0x00000000u, 0x00000000u, 0x99999999u}},
        {1u, 3u, 8u, {0x00000000u, 0x00000000u, 0x55555555u, 0x00000000u, 0x00000000u}},
        {1u, 9u, 10u, {0x99999999u}},
        {1u, 10u, 16u, {0x00000000u}},
        {1u, 20u, 24u, {0x00000000u}},
        {1u, 4294967294u, 4294967295u, {0x00000000u}},
        {2u, 0u, 0u, {}},
        {2u, 4u, 4u, {}},
        {2u, 8u, 4u, {}},
        {2u, 0u, 1u, {0x12345678u}},
        {2u, 0u, 5u, {0x12345678u, 0xabcdef01u, 0x00000000u, 0x00000000u, 0x11223344u}},
        {2u, 0u, 16u, {0x12345678u, 0xabcdef01u, 0x00000000u, 0x00000000u, 0x11223344u, 0x00000000u}},
        {2u, 2u, 10u, {0x00000000u, 0x00000000u, 0x11223344u, 0x00000000u}},
        {2u, 3u, 8u, {0x00000000u, 0x11223344u, 0x00000000u}},
        {2u, 9u, 10u, {0x00000000u}},
        {2u, 10u, 16u, {0x00000000u}},
        {2u, 20u, 24u, {0x00000000u}},
        {2u, 4294967294u, 4294967295u, {0x00000000u}},
    }};
    for (const auto& value : cases) {
        const std::span<const ShaderRegister> source = value.instance == 0 ? std::span<const ShaderRegister>{} : value.instance == 1 ? std::span<const ShaderRegister>{first} : std::span<const ShaderRegister>{second};
        std::array<std::uint32_t, 32> words;
        words.fill(0xdeadbeefu);
        check(Agc::Command::CopyDefaultCxStateRange(words.data(), value.first, value.last, source) == 0, "reference range return differs");
        check(std::equal(value.prefix.begin(), value.prefix.end(), words.begin()), "reference range prefix differs");
        check(std::all_of(words.begin() + value.prefix.size(), words.end(), [](auto word) { return word == 0xdeadbeefu; }), "reference range tail differs");
    }
    for (const auto firstOffset : {0u, 4u, 8u}) {
        check(Agc::Command::CopyDefaultCxStateRange(nullptr, firstOffset, firstOffset == 8 ? 4u : firstOffset, first) == 0, "empty null range differs");
    }
    check(Agc::Command::CopyDefaultCxStateRange(nullptr, 0, 1, {}) == 0, "empty descriptor inspected its output pointer");
    check(Agc::Command::CopyDefaultCxStateRange(nullptr, 0, 1, first) == static_cast<int>(0x80890001u), "invalid populated output was accepted");
}

}

int main() {
    try {
        testReferenceRanges();
        LibcRunShutdown_nid_postfix();
        std::puts("AGC default context ranges: 39 reference-derived cases and 2 host validation cases passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        LibcRunShutdown_nid_postfix();
        return 1;
    }
}
