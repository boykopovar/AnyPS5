#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "Testing.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

namespace {
constexpr std::uint16_t Character = 0020000;
constexpr std::uint16_t Block = 0060000;
constexpr std::uint16_t Regular = 0100000;
constexpr std::uint16_t Directory = 0040000;
constexpr std::uint32_t NoDevice = 0xFFFFFFFFu;

std::string Name(std::uint32_t device, std::uint16_t type) {
    std::array<char, 64> buffer;
    buffer.fill('#');
    char* result = devname_r_nid_postfix(device, type, buffer.data(), static_cast<int>(buffer.size()));
    Require(result == buffer.data());
    return result;
}
}

int main() {
    Require(Name(NoDevice, Character) == "#NODEV");
    Require(Name(NoDevice, Block) == "#NODEV");
    Require(Name(0x1234, Regular) == "#NODEV");
    Require(Name(0x1234, Directory) == "#NODEV");
    Require(Name(0x1234, 0) == "#NODEV");
    Require(Name(0x1234, Character | 0644) == "#C:0x1234");
    Require(Name(0, Character) == "#C:0");
    Require(Name(1, Character) == "#C:0x1");
    Require(Name(0xF, Character) == "#C:0xf");
    Require(Name(0x10, Character) == "#C:0x10");
    Require(Name(0xDEADBEEFu, Character) == "#C:0xdeadbeef");
    Require(Name(0xFFFFFFFEu, Character) == "#C:0xfffffffe");
    Require(Name(0x61006001u, Block) == "#B:0x61006001");
    Require(Name(0x6001u, Block) == "#B:0x6001");
    Require(Name(0x1234, Block) == "#NODEV");
    Require(Name(0x1234, Block | 0644) == "#NODEV");

    for (std::size_t length = 0; length <= 16; ++length) {
        std::array<char, 20> buffer;
        buffer.fill('#');
        char* result = devname_r_nid_postfix(0xABCDu, Character, buffer.data() + 1, static_cast<int>(length));
        Require(result == buffer.data() + 1);
        const std::string expected = "#C:0xabcd";
        Require(buffer.front() == '#');
        for (std::size_t index = length + 1; index < buffer.size(); ++index) Require(buffer[index] == '#');
        if (length != 0) Require(std::string(buffer.data() + 1, std::min(length - 1, expected.size())) == expected.substr(0, length - 1));
        if (length != 0 && length <= expected.size()) Require(buffer[length] == '\0');
        if (length > expected.size()) Require(std::strcmp(buffer.data() + 1, expected.c_str()) == 0);
    }

    for (std::size_t length = 0; length <= 8; ++length) {
        std::array<char, 12> buffer;
        buffer.fill('#');
        devname_r_nid_postfix(NoDevice, Character, buffer.data() + 1, static_cast<int>(length));
        const std::string expected = "#NODEV";
        if (length == 0) Require(buffer[1] == '#');
        else {
            Require(std::strncmp(buffer.data() + 1, expected.c_str(), std::min(length - 1, expected.size())) == 0);
            Require(buffer[1 + std::min(length - 1, expected.size())] == '\0');
        }
    }

    std::array<char, 8> negative;
    negative.fill('#');
    *__error_nid_postfix() = 0;
    Require(devname_r_nid_postfix(0x12, Character, negative.data(), -1) == negative.data());
    Require(*__error_nid_postfix() == 84 && negative[0] == '\0' && negative[1] == '#');
    *__error_nid_postfix() = 0;
    Require(devname_r_nid_postfix(0x12, Character, negative.data(), std::numeric_limits<int>::min()) == negative.data());
    Require(*__error_nid_postfix() == 84 && negative[0] == '\0');

    char* first = devname_nid_postfix(0x77, Character);
    Require(first != nullptr && std::strcmp(first, "#C:0x77") == 0);
    char* second = devname_nid_postfix(NoDevice, Character);
    Require(second == first && std::strcmp(second, "#NODEV") == 0);
    char* third = devname_nid_postfix(0x6001u, Block);
    Require(third == first && std::strcmp(third, "#B:0x6001") == 0);
    Require(std::strlen(devname_nid_postfix(0xFFFFFFFEu, Character)) == 13);

    char* unmapped = reinterpret_cast<char*>(std::uintptr_t{0x10});
    Require(devname_r_nid_postfix(0xABCDu, Character, unmapped, 16) == unmapped);
    Require(devname_r_nid_postfix(NoDevice, Character, unmapped, 16) == unmapped);
    *__error_nid_postfix() = 0;
    Require(devname_r_nid_postfix(0x12, Character, unmapped, -1) == unmapped);
    Require(*__error_nid_postfix() == 84);

    std::array<char, 24> full;
    full.fill('#');
    Require(devname_r_nid_postfix(0xFFFFFFFEu, Character, full.data(), static_cast<int>(full.size())) == full.data());
    Require(std::strcmp(full.data(), "#C:0xfffffffe") == 0 && full[14] == '#');
}
