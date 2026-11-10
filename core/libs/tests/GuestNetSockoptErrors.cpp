#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include "prx/libSceNet/libSceNet.hpp"

static void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static int Error(int code) {
    return static_cast<int>(0x80410100u | static_cast<unsigned>(code));
}

static void RequireError(int result, int code, std::source_location location = std::source_location::current()) {
    Require(result == Error(code), location);
    Require(*sceNetErrnoLoc() == code, location);
}

int main() {
    constexpr int socketLevel = 0xffff;
    constexpr int fault = 14;
    constexpr int badDescriptor = 9;
    constexpr int invalid = 22;
    constexpr int noOption = 42;
    constexpr int unsupported = 45;
    void* const unmapped = reinterpret_cast<void*>(0x10);
    Require(sceNetInit_nid_postfix() == 0);
    const int stream = sceNetSocket("sockopt-stream", 2, 1, 0);
    const int datagram = sceNetSocket("sockopt-datagram", 2, 2, 0);
    const int stream6 = sceNetSocket("sockopt-stream6", 28, 1, 0);
    Require(stream >= 0 && datagram >= 0 && stream6 >= 0);
    int one = 1;
    std::uint32_t size = sizeof(one);

    for (const int socket : {stream, datagram}) {
        for (const int option : {0x4000, 0x8000, 0x100a, 0x100f, 0x1014, 0x1018, 0x1020, 0x1100, 0x1104, 0x110a, 0x1110, 0x1202, 0x1204,
                 0x1210, 0x7777}) {
            int value = 0;
            size = sizeof(value);
            RequireError(sceNetGetsockopt(socket, socketLevel, option, &value, &size), noOption);
            size = 0;
            RequireError(sceNetGetsockopt(socket, socketLevel, option, nullptr, &size), noOption);
        }
        for (const int option : {0x2, 0x100a, 0x100f, 0x1010, 0x1011, 0x1012, 0x1013, 0x1018, 0x1020, 0x1100, 0x1104, 0x110a, 0x1110,
                 0x1204, 0x1210, 0x7777, 0x1007, 0x1008, 0x1016, 0x1107}) {
            RequireError(sceNetSetsockopt(socket, socketLevel, option, &one, sizeof(one)), noOption);
            RequireError(sceNetSetsockopt(socket, socketLevel, option, &one, 0), noOption);
            RequireError(sceNetSetsockopt(socket, socketLevel, option, nullptr, 0), noOption);
        }
        for (const int option : {0x1000, 0x1014}) {
            RequireError(sceNetSetsockopt(socket, socketLevel, option, &one, sizeof(one)), invalid);
            RequireError(sceNetSetsockopt(socket, socketLevel, option, &one, 0), invalid);
            RequireError(sceNetSetsockopt(socket, socketLevel, option, nullptr, 0), invalid);
        }
        RequireError(sceNetSetsockopt(socket, socketLevel, 0x1009, &one, sizeof(one)), unsupported);
        RequireError(sceNetSetsockopt(socket, socketLevel, 0x1009, &one, 0), unsupported);
        RequireError(sceNetSetsockopt(socket, socketLevel, 0x1009, nullptr, 0), unsupported);
        size = sizeof(one);
        RequireError(sceNetGetsockopt(socket, socketLevel, 0x1000, &one, &size), invalid);
        RequireError(sceNetGetsockopt(socket, socketLevel, 0x1009, &one, &size), unsupported);
        RequireError(sceNetGetsockopt(socket, socketLevel, 0x1010, &one, &size), unsupported);
    }
    one = 1;

    for (const int level : {1, 17, 0x7fff}) {
        size = sizeof(one);
        RequireError(sceNetGetsockopt(stream, level, 4, &one, &size), invalid);
        RequireError(sceNetSetsockopt(stream, level, 4, &one, sizeof(one)), invalid);
        RequireError(sceNetSetsockopt(stream, level, 0x7777, &one, sizeof(one)), invalid);
        RequireError(sceNetSetsockopt(stream, level, 4, unmapped, sizeof(one)), invalid);
        size = sizeof(one);
        RequireError(sceNetGetsockopt(stream, level, 4, nullptr, &size), invalid);
    }
    size = sizeof(one);
    RequireError(sceNetGetsockopt(datagram, 1, 1, &one, &size), invalid);
    RequireError(sceNetGetsockopt(datagram, 41, 27, &one, &size), invalid);
    RequireError(sceNetSetsockopt(datagram, 41, 27, &one, sizeof(one)), invalid);
    RequireError(sceNetGetsockopt(datagram, 6, 1, &one, &size), invalid);
    RequireError(sceNetSetsockopt(datagram, 6, 1, &one, sizeof(one)), invalid);
    for (const int option : {1, 0x7777}) {
        size = sizeof(one);
        RequireError(sceNetGetsockopt(datagram, 17, option, &one, &size), noOption);
        RequireError(sceNetSetsockopt(datagram, 17, option, &one, sizeof(one)), noOption);
    }
    size = sizeof(one);
    RequireError(sceNetGetsockopt(stream, 6, 0x7777, &one, &size), noOption);
    RequireError(sceNetSetsockopt(stream, 6, 0x7777, &one, sizeof(one)), noOption);
    RequireError(sceNetSetsockopt(stream, 6, 0x7777, &one, 0), noOption);
    for (const int option : {3, 4}) {
        size = sizeof(one);
        RequireError(sceNetGetsockopt(stream6, 0, option, &one, &size), invalid);
    }
    RequireError(sceNetSetsockopt(stream6, 0, 4, &one, sizeof(one)), invalid);

    Require(sceNetSetsockopt(stream, socketLevel, 0x1200, &one, sizeof(one)) == 0);
    const std::int64_t timeout[2] = {2, 0};
    Require(sceNetSetsockopt(stream, socketLevel, 0x1005, timeout, sizeof(timeout)) == 0);
    for (const int option : {0x4, 0x8, 0x10, 0x20, 0x80, 0x100, 0x200, 0x1001, 0x1002, 0x1005, 0x1006, 0x1007, 0x1008, 0x1016, 0x1105,
             0x1106, 0x1109, 0x1200, 0x10000}) {
        const std::uint32_t full = option == 0x80 ? 8 : option == 0x1005 || option == 0x1006 ? 16 : 4;
        std::array<std::uint8_t, 24> reference{};
        size = static_cast<std::uint32_t>(reference.size());
        Require(sceNetGetsockopt(stream, socketLevel, option, reference.data(), &size) == 0);
        Require(size == full);
        for (const std::uint32_t capacity : {0u, 1u, 2u, 3u, 4u, 8u, 12u, 16u, 20u}) {
            std::array<std::uint8_t, 24> bytes;
            bytes.fill(0xa5);
            size = capacity;
            Require(sceNetGetsockopt(stream, socketLevel, option, bytes.data(), &size) == 0);
            const auto expected = std::min(capacity, full);
            Require(size == expected);
            Require(std::memcmp(bytes.data(), reference.data(), expected) == 0);
            Require(std::all_of(bytes.begin() + expected, bytes.end(), [](auto byte) { return byte == 0xa5; }));
        }
        size = 4;
        Require(sceNetGetsockopt(stream, socketLevel, option, nullptr, &size) == 0 && size == 0);
        size = 0;
        Require(sceNetGetsockopt(stream, socketLevel, option, nullptr, &size) == 0 && size == 0);
        size = 0;
        Require(sceNetGetsockopt(stream, socketLevel, option, unmapped, &size) == 0 && size == 0);
        size = 4;
        RequireError(sceNetGetsockopt(stream, socketLevel, option, unmapped, &size), fault);
        Require(size == 4);
    }
    size = 4;
    Require(sceNetGetsockopt(stream, 6, 1, nullptr, &size) == 0 && size == 0);
    std::uint16_t narrow = 0xa5a5;
    size = sizeof(narrow);
    Require(sceNetGetsockopt(stream, 6, 1, &narrow, &size) == 0 && size == sizeof(narrow) && narrow == 0);

    RequireError(sceNetGetsockopt(stream, socketLevel, 4, &one, nullptr), fault);
    RequireError(sceNetGetsockopt(stream, socketLevel, 4, nullptr, nullptr), fault);
    RequireError(sceNetGetsockopt(stream, socketLevel, 0x7777, &one, nullptr), fault);
    RequireError(sceNetGetsockopt(stream, 0x7fff, 4, &one, nullptr), fault);
    RequireError(sceNetGetsockopt(-1, socketLevel, 4, &one, nullptr), fault);
    RequireError(sceNetGetsockopt(stream, socketLevel, 4, &one, static_cast<std::uint32_t*>(unmapped)), fault);
    size = 4;
    RequireError(sceNetGetsockopt(-1, socketLevel, 4, nullptr, &size), badDescriptor);
    RequireError(sceNetGetsockopt(stream, socketLevel, 0x7777, nullptr, &size), noOption);
    for (const std::uint32_t huge : {0x80000000u, 0xffffffffu}) {
        size = huge;
        RequireError(sceNetGetsockopt(stream, socketLevel, 4, &one, &size), invalid);
        Require(size == huge);
    }

    one = 1;
    for (const int option : {0x4, 0x8, 0x10, 0x20, 0x100, 0x200, 0x1200, 0x10000}) {
        for (const std::uint32_t length : {0u, 1u, 2u, 3u}) RequireError(sceNetSetsockopt(stream, socketLevel, option, &one, length), invalid);
        RequireError(sceNetSetsockopt(stream, socketLevel, option, nullptr, 0), invalid);
        RequireError(sceNetSetsockopt(stream, socketLevel, option, nullptr, sizeof(one)), fault);
        Require(sceNetSetsockopt(stream, socketLevel, option, &one, 8) == 0);
    }
    RequireError(sceNetSetsockopt(stream, socketLevel, 4, unmapped, sizeof(one)), fault);
    RequireError(sceNetSetsockopt(stream, socketLevel, 4, unmapped, 0), invalid);
    RequireError(sceNetSetsockopt(stream, socketLevel, 0x7777, unmapped, sizeof(one)), noOption);
    RequireError(sceNetSetsockopt(stream, socketLevel, 0x7777, unmapped, 0), noOption);
    RequireError(sceNetSetsockopt(stream, socketLevel, 0x7777, nullptr, sizeof(one)), fault);
    RequireError(sceNetSetsockopt(stream, 0x7fff, 4, nullptr, sizeof(one)), fault);
    RequireError(sceNetSetsockopt(9999, socketLevel, 4, nullptr, sizeof(one)), fault);
    RequireError(sceNetSetsockopt(-1, socketLevel, 4, nullptr, sizeof(one)), fault);
    RequireError(sceNetSetsockopt(9999, socketLevel, 4, nullptr, 0), badDescriptor);
    RequireError(sceNetSetsockopt(-1, socketLevel, 4, &one, sizeof(one)), badDescriptor);
    RequireError(sceNetSetsockopt(stream, socketLevel, 4, &one, 0x80000000u), invalid);
    Require(sceNetSetsockopt(stream, socketLevel, 4, &one, 0x10000) == 0);
    RequireError(sceNetSetsockopt(stream, 6, 1, nullptr, sizeof(one)), fault);
    RequireError(sceNetSetsockopt(stream, 6, 1, &one, 3), invalid);
    RequireError(sceNetSetsockopt(stream6, 41, 27, nullptr, sizeof(one)), fault);

    Require(sceNetSocketClose(stream) == 0 && sceNetSocketClose(datagram) == 0 && sceNetSocketClose(stream6) == 0);
}
