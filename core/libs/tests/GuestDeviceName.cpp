#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
    *__error_nid_postfix() = 0;
    Check(Name(NoDevice, Character) == "#NODEV" && *__error_nid_postfix() == 0);
    *__error_nid_postfix() = 0;
    Check(Name(0x1234, Character) == "#C:18:0x34" && *__error_nid_postfix() == 2);
    Check(Name(NoDevice, Block) == "#NODEV");
    Check(Name(0x1234, Regular) == "#NODEV");
    Check(Name(0x1234, Directory) == "#NODEV");
    Check(Name(0x1234, 0) == "#NODEV");
    Check(Name(0x1234, Character | 0644) == "#C:18:0x34");
    Check(Name(0x1234, Block) == "#NODEV");
    Check(Name(0x1234, Block | 0644) == "#NODEV");

    for (std::size_t length = 0; length <= 16; ++length) {
        std::array<char, 20> buffer;
        buffer.fill('#');
        char* result = devname_r_nid_postfix(0x1234, Character, buffer.data() + 1, static_cast<int>(length));
        Check(result == buffer.data() + 1);
        const std::string expected = "#C:18:0x34";
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

    std::array<char, 16> negative;
    negative.fill('#');
    *__error_nid_postfix() = 0;
    Check(devname_r_nid_postfix(0x1234, Character, negative.data(), -1) == negative.data());
    Check(*__error_nid_postfix() == 14 && std::strcmp(negative.data(), "#C:18:0x34") == 0);
    negative.fill('#');
    *__error_nid_postfix() = 0;
    Check(devname_r_nid_postfix(0x1234, Character, negative.data(), 0) == negative.data());
    Check(*__error_nid_postfix() == 2 && negative.front() == '#' && negative.back() == '#');

    char* first = devname_nid_postfix(0x1234, Character);
    Check(first != nullptr && std::strcmp(first, "#C:18:0x34") == 0);
    char* second = devname_nid_postfix(0x1235, Character);
    Check(second == first);
    char* third = devname_nid_postfix(NoDevice, Character);
    Check(third == first && std::strcmp(third, "#NODEV") == 0);

    char* unmapped = reinterpret_cast<char*>(std::uintptr_t{0x10});
    Check(devname_r_nid_postfix(0xABCDu, Character, unmapped, 16) == unmapped);
    Check(devname_r_nid_postfix(NoDevice, Character, unmapped, 16) == unmapped);
}
