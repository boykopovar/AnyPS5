#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "Device name check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

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
    Check(result == buffer.data());
    return result;
}
}

int main() {
    Check(Name(NoDevice, Character) == "#NODEV");
    Check(Name(NoDevice, Block) == "#NODEV");
    Check(Name(0x1234, Regular) == "#NODEV");
    Check(Name(0x1234, Directory) == "#NODEV");
    Check(Name(0x1234, 0) == "#NODEV");
    Check(Name(0x1234, Character | 0644) == "#C:0x1234");
    Check(Name(0, Character) == "#C:0");
    Check(Name(1, Character) == "#C:0x1");
    Check(Name(0xF, Character) == "#C:0xf");
    Check(Name(0x10, Character) == "#C:0x10");
    Check(Name(0xDEADBEEFu, Character) == "#C:0xdeadbeef");
    Check(Name(0xFFFFFFFEu, Character) == "#C:0xfffffffe");
    Check(Name(0x61006001u, Block) == "#B:0x61006001");
    Check(Name(0x6001u, Block) == "#B:0x6001");
    Check(Name(0x1234, Block) == "#NODEV");
    Check(Name(0x1234, Block | 0644) == "#NODEV");

    for (std::size_t length = 0; length <= 16; ++length) {
        std::array<char, 20> buffer;
        buffer.fill('#');
        char* result = devname_r_nid_postfix(0xABCDu, Character, buffer.data() + 1, static_cast<int>(length));
        Check(result == buffer.data() + 1);
        const std::string expected = "#C:0xabcd";
        Check(buffer.front() == '#');
        for (std::size_t index = length + 1; index < buffer.size(); ++index) Check(buffer[index] == '#');
        if (length != 0) Check(std::string(buffer.data() + 1, std::min(length - 1, expected.size())) == expected.substr(0, length - 1));
        if (length != 0 && length <= expected.size()) Check(buffer[length] == '\0');
        if (length > expected.size()) Check(std::strcmp(buffer.data() + 1, expected.c_str()) == 0);
    }

    for (std::size_t length = 0; length <= 8; ++length) {
        std::array<char, 12> buffer;
        buffer.fill('#');
        devname_r_nid_postfix(NoDevice, Character, buffer.data() + 1, static_cast<int>(length));
        const std::string expected = "#NODEV";
        if (length == 0) Check(buffer[1] == '#');
        else {
            Check(std::strncmp(buffer.data() + 1, expected.c_str(), std::min(length - 1, expected.size())) == 0);
            Check(buffer[1 + std::min(length - 1, expected.size())] == '\0');
        }
    }

    std::array<char, 8> negative;
    negative.fill('#');
    *__error_nid_postfix() = 0;
    Check(devname_r_nid_postfix(0x12, Character, negative.data(), -1) == negative.data());
    Check(*__error_nid_postfix() == 84 && negative[0] == '\0' && negative[1] == '#');
    *__error_nid_postfix() = 0;
    Check(devname_r_nid_postfix(0x12, Character, negative.data(), std::numeric_limits<int>::min()) == negative.data());
    Check(*__error_nid_postfix() == 84 && negative[0] == '\0');

    char* first = devname_nid_postfix(0x77, Character);
    Check(first != nullptr && std::strcmp(first, "#C:0x77") == 0);
    char* second = devname_nid_postfix(NoDevice, Character);
    Check(second == first && std::strcmp(second, "#NODEV") == 0);
    char* third = devname_nid_postfix(0x6001u, Block);
    Check(third == first && std::strcmp(third, "#B:0x6001") == 0);
    Check(std::strlen(devname_nid_postfix(0xFFFFFFFEu, Character)) == 13);
}
