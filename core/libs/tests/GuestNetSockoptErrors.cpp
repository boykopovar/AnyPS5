#include <algorithm>
#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include "prx/libc/include/General.hpp"
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

static int GetOption(int socket, int level, int option, void* value, std::uint32_t* length) {
    const int result = sceNetGetsockopt(socket, level, option, value, length);
    std::fprintf(stderr, "get socket %d level 0x%x option 0x%x result 0x%x errno %d\n", socket, level, option,
        static_cast<unsigned>(result), *sceNetErrnoLoc());
    return result;
}

static int SetOption(int socket, int level, int option, const void* value, std::uint32_t length) {
    const int result = sceNetSetsockopt(socket, level, option, value, length);
    std::fprintf(stderr, "set socket %d level 0x%x option 0x%x result 0x%x errno %d\n", socket, level, option,
        static_cast<unsigned>(result), *sceNetErrnoLoc());
    return result;
}

int main() {
    constexpr int socketLevel = 0xffff;
    constexpr int fault = 14;
    constexpr int badDescriptor = 9;
    constexpr int invalid = 22;
    constexpr int noOption = 42;
    constexpr int unsupported = 45;
    void* const unmapped = reinterpret_cast<void*>(0x10);
    int writable = 0;
    errno = 1234;
    Require(GuestWritable_nid_no_patch(&writable, sizeof(writable)) && errno == 1234);
    Require(!GuestWritable_nid_no_patch(unmapped, sizeof(writable)) && errno == 1234);
    Require(sceNetInit_nid_postfix() == 0);
    const int stream = sceNetSocket("sockopt-stream", 2, 1, 0);
    const int datagram = sceNetSocket("sockopt-datagram", 2, 2, 0);
    const int stream6 = sceNetSocket("sockopt-stream6", 28, 1, 0);
    Require(stream >= 0 && datagram >= 0);
    if (stream6 < 0) {
        Require(*sceNetErrnoLoc() == 43 || *sceNetErrnoLoc() == 47);
        std::fprintf(stderr, "IPv6 unavailable; continuing IPv4 sockopt checks\n");
    }
    int one = 1;
    std::uint32_t size = sizeof(one);

    for (const int socket : {stream, datagram}) {
        for (const int option : {0x4000, 0x8000, 0x100a, 0x100f, 0x1014, 0x1018, 0x1020, 0x1100, 0x1104, 0x110a, 0x1110, 0x1202, 0x1204,
                 0x1210, 0x7777}) {
            int value = 0;
            size = sizeof(value);
            RequireError(GetOption(socket, socketLevel, option, &value, &size), noOption);
            size = 0;
            RequireError(GetOption(socket, socketLevel, option, nullptr, &size), noOption);
        }
        for (const int option : {0x2, 0x100a, 0x100f, 0x1010, 0x1011, 0x1012, 0x1013, 0x1018, 0x1020, 0x1100, 0x1104, 0x110a, 0x1110,
                 0x1204, 0x1210, 0x7777, 0x1007, 0x1008, 0x1016, 0x1107}) {
            RequireError(SetOption(socket, socketLevel, option, &one, sizeof(one)), noOption);
            RequireError(SetOption(socket, socketLevel, option, &one, 0), noOption);
            RequireError(SetOption(socket, socketLevel, option, nullptr, 0), noOption);
        }
        for (const int option : {0x1000, 0x1014}) {
            RequireError(SetOption(socket, socketLevel, option, &one, sizeof(one)), invalid);
            RequireError(SetOption(socket, socketLevel, option, &one, 0), invalid);
            RequireError(SetOption(socket, socketLevel, option, nullptr, 0), invalid);
        }
        RequireError(SetOption(socket, socketLevel, 0x1009, &one, sizeof(one)), unsupported);
        RequireError(SetOption(socket, socketLevel, 0x1009, &one, 0), unsupported);
        RequireError(SetOption(socket, socketLevel, 0x1009, nullptr, 0), unsupported);
        size = sizeof(one);
        RequireError(GetOption(socket, socketLevel, 0x1000, &one, &size), invalid);
        RequireError(GetOption(socket, socketLevel, 0x1009, &one, &size), unsupported);
        RequireError(GetOption(socket, socketLevel, 0x1010, &one, &size), unsupported);
    }
    one = 1;

    for (const int level : {1, 17, 0x7fff}) {
        size = sizeof(one);
        RequireError(GetOption(stream, level, 4, &one, &size), invalid);
        RequireError(SetOption(stream, level, 4, &one, sizeof(one)), invalid);
        RequireError(SetOption(stream, level, 0x7777, &one, sizeof(one)), invalid);
        RequireError(SetOption(stream, level, 4, unmapped, sizeof(one)), invalid);
        size = sizeof(one);
        RequireError(GetOption(stream, level, 4, nullptr, &size), invalid);
    }
    size = sizeof(one);
    RequireError(GetOption(datagram, 1, 1, &one, &size), invalid);
    RequireError(GetOption(datagram, 41, 27, &one, &size), invalid);
    RequireError(SetOption(datagram, 41, 27, &one, sizeof(one)), invalid);
    RequireError(GetOption(datagram, 6, 1, &one, &size), invalid);
    RequireError(SetOption(datagram, 6, 1, &one, sizeof(one)), invalid);
    for (const int option : {1, 0x7777}) {
        size = sizeof(one);
        RequireError(GetOption(datagram, 17, option, &one, &size), noOption);
        RequireError(SetOption(datagram, 17, option, &one, sizeof(one)), noOption);
    }
    size = sizeof(one);
    RequireError(GetOption(stream, 6, 0x7777, &one, &size), noOption);
    RequireError(SetOption(stream, 6, 0x7777, &one, sizeof(one)), noOption);
    RequireError(SetOption(stream, 6, 0x7777, &one, 0), noOption);
    if (stream6 >= 0) {
        for (const int option : {3, 4}) {
            size = sizeof(one);
            RequireError(GetOption(stream6, 0, option, &one, &size), invalid);
        }
        RequireError(SetOption(stream6, 0, 4, &one, sizeof(one)), invalid);
    }

    Require(SetOption(stream, socketLevel, 0x1200, &one, sizeof(one)) == 0);
    const std::int64_t timeout[2] = {2, 0};
    Require(SetOption(stream, socketLevel, 0x1005, timeout, sizeof(timeout)) == 0);
    for (const int option : {0x4, 0x8, 0x10, 0x20, 0x80, 0x100, 0x200, 0x1001, 0x1002, 0x1005, 0x1006, 0x1007, 0x1008, 0x1016, 0x1105,
             0x1106, 0x1109, 0x1200, 0x10000}) {
        const std::uint32_t full = option == 0x80 ? 8 : option == 0x1005 || option == 0x1006 ? 16 : 4;
        std::array<std::uint8_t, 24> reference{};
        size = static_cast<std::uint32_t>(reference.size());
        if (const int result = GetOption(stream, socketLevel, option, reference.data(), &size); result != 0) {
            std::fprintf(stderr, "GetOption(stream, SOL_SOCKET, 0x%x) returned %d errno %d\n", option, result,
                *sceNetErrnoLoc());
            Require(false);
        }
        Require(size == full);
        for (const std::uint32_t capacity : {0u, 1u, 2u, 3u, 4u, 8u, 12u, 16u, 20u}) {
            std::array<std::uint8_t, 24> bytes;
            bytes.fill(0xa5);
            size = capacity;
            Require(GetOption(stream, socketLevel, option, bytes.data(), &size) == 0);
            const auto expected = std::min(capacity, full);
            Require(size == expected);
            Require(std::memcmp(bytes.data(), reference.data(), expected) == 0);
            Require(std::all_of(bytes.begin() + expected, bytes.end(), [](auto byte) { return byte == 0xa5; }));
        }
        size = 4;
        Require(GetOption(stream, socketLevel, option, nullptr, &size) == 0 && size == 0);
        size = 0;
        Require(GetOption(stream, socketLevel, option, nullptr, &size) == 0 && size == 0);
        size = 0;
        Require(GetOption(stream, socketLevel, option, unmapped, &size) == 0 && size == 0);
        size = 24;
        RequireError(GetOption(stream, socketLevel, option, unmapped, &size), fault);
        Require(size == 24);
    }
    size = 4;
    Require(GetOption(stream, 6, 1, nullptr, &size) == 0 && size == 0);
    std::uint16_t narrow = 0xa5a5;
    size = sizeof(narrow);
    Require(GetOption(stream, 6, 1, &narrow, &size) == 0 && size == sizeof(narrow) && narrow == 0);

    RequireError(GetOption(stream, socketLevel, 4, &one, nullptr), fault);
    RequireError(GetOption(stream, socketLevel, 4, nullptr, nullptr), fault);
    RequireError(GetOption(stream, socketLevel, 0x7777, &one, nullptr), fault);
    RequireError(GetOption(stream, 0x7fff, 4, &one, nullptr), fault);
    RequireError(GetOption(-1, socketLevel, 4, &one, nullptr), fault);
    RequireError(GetOption(stream, socketLevel, 4, &one, static_cast<std::uint32_t*>(unmapped)), fault);
    size = 4;
    RequireError(GetOption(-1, socketLevel, 4, nullptr, &size), badDescriptor);
    RequireError(GetOption(stream, socketLevel, 0x7777, nullptr, &size), noOption);
    for (const std::uint32_t huge : {0x80000000u, 0xffffffffu}) {
        size = huge;
        RequireError(GetOption(stream, socketLevel, 4, &one, &size), invalid);
        Require(size == huge);
        RequireError(GetOption(stream, socketLevel, 4, nullptr, &size), invalid);
        Require(size == huge);
        RequireError(SetOption(stream, socketLevel, 4, nullptr, huge), invalid);
        for (const int level : {socketLevel, 6, 41}) {
            const int descriptor = level == 41 ? stream6 : stream;
            if (descriptor < 0) continue;
            for (void* value : {static_cast<void*>(&one), static_cast<void*>(nullptr)}) {
                size = huge;
                RequireError(GetOption(descriptor, level, 0x7777, value, &size), noOption);
                Require(size == huge);
                RequireError(SetOption(descriptor, level, 0x7777, value, huge), noOption);
            }
        }
    }

    one = 1;
    for (const int option : {0x4, 0x8, 0x10, 0x20, 0x100, 0x200, 0x1200, 0x10000}) {
        for (const std::uint32_t length : {0u, 1u, 2u, 3u}) RequireError(SetOption(stream, socketLevel, option, &one, length), invalid);
        RequireError(SetOption(stream, socketLevel, option, nullptr, 0), invalid);
        RequireError(SetOption(stream, socketLevel, option, nullptr, sizeof(one)), fault);
        const std::array<int, 2> value{one, 0};
        Require(SetOption(stream, socketLevel, option, value.data(), sizeof(value)) == 0);
    }
    RequireError(SetOption(stream, socketLevel, 4, unmapped, sizeof(one)), fault);
    RequireError(SetOption(stream, socketLevel, 4, unmapped, 0), invalid);
    RequireError(SetOption(stream, socketLevel, 0x7777, unmapped, sizeof(one)), noOption);
    RequireError(SetOption(stream, socketLevel, 0x7777, unmapped, 0), noOption);
    RequireError(SetOption(stream, socketLevel, 0x7777, nullptr, sizeof(one)), fault);
    RequireError(SetOption(stream, 0x7fff, 4, nullptr, sizeof(one)), fault);
    RequireError(SetOption(9999, socketLevel, 4, nullptr, sizeof(one)), fault);
    RequireError(SetOption(-1, socketLevel, 4, nullptr, sizeof(one)), fault);
    RequireError(SetOption(9999, socketLevel, 4, nullptr, 0), badDescriptor);
    RequireError(SetOption(-1, socketLevel, 4, &one, sizeof(one)), badDescriptor);
    RequireError(SetOption(stream, socketLevel, 4, &one, 0x80000000u), invalid);
    Require(SetOption(stream, socketLevel, 4, &one, 0x10000) == 0);
    RequireError(SetOption(stream, 6, 1, nullptr, sizeof(one)), fault);
    RequireError(SetOption(stream, 6, 1, &one, 3), invalid);
    for (const int option : {1, 2, 256, 512, 1024}) {
        RequireError(SetOption(stream, 6, option, unmapped, sizeof(one)), fault);
    }
    for (const int socket : {stream, stream6, datagram}) {
        if (socket < 0) continue;
        for (const int enabled : {1, 0}) {
            Require(SetOption(socket, socketLevel, 0x20, &enabled, sizeof(enabled)) == 0);
            int value = -1;
            size = sizeof(value);
            Require(GetOption(socket, socketLevel, 0x20, &value, &size) == 0);
            Require(value == (enabled ? 0x20 : 0));
        }
    }
    if (stream6 >= 0) RequireError(SetOption(stream6, 41, 27, nullptr, sizeof(one)), fault);

    Require(sceNetSocketClose(stream) == 0 && sceNetSocketClose(datagram) == 0);
    if (stream6 >= 0) Require(sceNetSocketClose(stream6) == 0);
}
