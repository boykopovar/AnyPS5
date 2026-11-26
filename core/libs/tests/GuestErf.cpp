#include "prx/libc/include/Api.hpp"
#include <bit>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <string_view>

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "%s:%u: requirement failed\n", location.file_name(), location.line());
    std::abort();
}

static void CheckErffBits() {
    struct Case {
        std::uint32_t input;
        std::uint32_t expected;
        int error;
    };
    constexpr Case erfCases[] = {
        {0x00000000u, 0x00000000u, 0}, {0x80000000u, 0x80000000u, 0}, {0x00000001u, 0x00000001u, 0}, {0x03ffffffu, 0x04106ebau, 0},
        {0x04000000u, 0x04106ebau, 0}, {0x317fffffu, 0x31906ebau, 0}, {0x31800000u, 0x31906ebau, 0}, {0x391c2317u, 0x39302e8cu, 0},
        {0x3e000327u, 0x3e0fb28eu, 0}, {0x3e800000u, 0x3e8d7aa7u, 0}, {0x3f000555u, 0x3f05442au, 0}, {0x3f57ffffu, 0x3f4468e6u, 0},
        {0x3f580000u, 0x3f4468e7u, 0}, {0x3f800000u, 0x3f57bb3du, 0}, {0xbf800000u, 0xbf57bb3du, 0}, {0x3f9fffffu, 0x3f6c432eu, 0},
        {0x3fa00000u, 0x3f6c432fu, 0}, {0x40000000u, 0x3f7ecd71u, 0}, {0xc0000000u, 0xbf7ecd71u, 0}, {0x4036db6cu, 0x3f7ffc82u, 0},
        {0x4036db6du, 0x3f7ffc82u, 0}, {0x4036db6eu, 0x3f7ffc82u, 0}, {0x40800000u, 0x3f800000u, 0}, {0xc0800000u, 0xbf800000u, 0},
        {0x40bfffffu, 0x3f800000u, 0}, {0x40c00000u, 0x3f800000u, 0}, {0xc0c00000u, 0xbf800000u, 0}, {0x41200000u, 0x3f800000u, 0},
        {0x41dfffffu, 0x3f800000u, 0}, {0x41e00000u, 0x3f800000u, 0}, {0xc1e00000u, 0xbf800000u, 0}, {0x23800000u, 0x23906ebau, 0},
        {0x237fffffu, 0x23906ebau, 0}, {0x7f7fffffu, 0x3f800000u, 0}, {0x7f800000u, 0x3f800000u, 0}, {0xff800000u, 0xbf800000u, 0},
        {0x7fc00001u, 0x7fc00001u, 0}, {0xffc00002u, 0xffc00002u, 0},
    };
    for (const auto& item : erfCases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(erff_nid_postfix(std::bit_cast<float>(item.input))) == item.expected);
        Require(errno == (item.error != 0 ? item.error : 1234));
    }
}

static void CheckErfcfBits() {
    struct Case {
        std::uint32_t input;
        std::uint32_t expected;
        int error;
    };
    constexpr Case erfcCases[] = {
        {0x00000000u, 0x3f800000u, 0}, {0x80000000u, 0x3f800000u, 0}, {0x00000001u, 0x3f800000u, 0}, {0x03ffffffu, 0x3f800000u, 0},
        {0x04000000u, 0x3f800000u, 0}, {0x317fffffu, 0x3f800000u, 0}, {0x31800000u, 0x3f800000u, 0}, {0x391c2317u, 0x3f7ff4fdu, 0},
        {0x3e000327u, 0x3f5c135cu, 0}, {0x3e800000u, 0x3f3942acu, 0}, {0x3f000555u, 0x3ef577abu, 0}, {0x3f57ffffu, 0x3e6e5c68u, 0},
        {0x3f580000u, 0x3e6e5c66u, 0}, {0x3f800000u, 0x3e21130bu, 0}, {0xbf800000u, 0x3febdd9eu, 0}, {0x3f9fffffu, 0x3d9de68du, 0},
        {0x3fa00000u, 0x3d9de68au, 0}, {0x40000000u, 0x3b9947b0u, 0}, {0xc0000000u, 0x3fff66b8u, 0}, {0x4036db6cu, 0x385f9bccu, 0},
        {0x4036db6du, 0x385f9bb8u, 0}, {0x4036db6eu, 0x385f9ba3u, 0}, {0x40800000u, 0x32846ee9u, 0}, {0xc0800000u, 0x40000000u, 0},
        {0x40bfffffu, 0x23c67c56u, 0}, {0x40c00000u, 0x23c67c0bu, 0}, {0xc0c00000u, 0x40000000u, 0}, {0x41200000u, 0x00000002u, 0},
        {0x41dfffffu, 0x00000000u, ERANGE}, {0x41e00000u, 0x00000000u, 0}, {0xc1e00000u, 0x40000000u, 0}, {0x23800000u, 0x3f800000u, 0},
        {0x237fffffu, 0x3f800000u, 0}, {0x7f7fffffu, 0x00000000u, 0}, {0x7f800000u, 0x00000000u, 0}, {0xff800000u, 0x40000000u, 0},
        {0x7fc00001u, 0x7fc00001u, 0}, {0xffc00002u, 0xffc00002u, 0},
    };
    for (const auto& item : erfcCases) {
        errno = 1234;
        Require(std::bit_cast<std::uint32_t>(erfcf_nid_postfix(std::bit_cast<float>(item.input))) == item.expected);
        Require(errno == (item.error != 0 ? item.error : 1234));
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) return 2;
    const std::string_view mode = argv[1];
    if (mode == "erff") CheckErffBits();
    else if (mode == "erfcf") CheckErfcfBits();
    else return 2;
}
