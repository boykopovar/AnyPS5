#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#endif
#include "prx/libScePosixForWebKit/libScePosixForWebKit.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "resolver service check failed at line %u\n", location.line());
    std::fflush(stderr);
    std::_Exit(1);
}

GuestResolver::AddressInfo Hints(int family, int type, int protocol, int flags) {
    GuestResolver::AddressInfo hints{};
    hints.family = family;
    hints.socketType = type;
    hints.protocol = protocol;
    hints.flags = flags;
    return hints;
}

unsigned Port(const GuestResolver::AddressInfo& entry) {
    Require(entry.address && entry.addressLength >= 4);
    const auto* bytes = static_cast<const unsigned char*>(entry.address);
    return (static_cast<unsigned>(bytes[2]) << 8) | bytes[3];
}

void ExpectPort(const char* service, unsigned expected, int family, int type, int protocol, int flags, bool noHints = false) {
    const auto hints = Hints(family, type, protocol, flags);
    GuestResolver::AddressInfo* result = nullptr;
    const auto error = getaddrinfo_nid_postfix(family == 28 ? "::1" : "127.0.0.1", service, noHints ? nullptr : &hints, &result);
    if (error != 0 || !result) {
        std::fprintf(stderr, "service='%s' family=%d type=%d protocol=%d flags=%d returned %d\n",
            service ? service : "(null)", family, type, protocol, flags, error);
    }
    Require(error == 0 && result);
    bool stream = false;
    bool datagram = false;
    for (const auto* entry = result; entry; entry = entry->next) {
        if (Port(*entry) != expected)
            std::fprintf(stderr, "service='%s' returned port %u, expected %u\n", service ? service : "(null)", Port(*entry), expected);
        Require(entry->family == family && Port(*entry) == expected);
        Require(entry->socketType == 1 || entry->socketType == 2);
        Require(entry->protocol == (entry->socketType == 1 ? 6 : 17));
        if (type) Require(entry->socketType == type);
        if (protocol) Require(entry->protocol == protocol);
        stream |= entry->socketType == 1;
        datagram |= entry->socketType == 2;
    }
    if (!type && !protocol) Require(stream && datagram);
    freeaddrinfo_nid_postfix(result);
}

void ExpectError(const char* node, const char* service, const GuestResolver::AddressInfo& hints, int expected) {
    auto* result = reinterpret_cast<GuestResolver::AddressInfo*>(std::uintptr_t{1});
    const auto error = getaddrinfo_nid_postfix(node, service, &hints, &result);
    if (error != expected || result) {
        std::fprintf(stderr, "service='%s' family=%d type=%d protocol=%d flags=%d returned %d, expected %d\n",
            service ? service : "(null)", hints.family, hints.socketType, hints.protocol, hints.flags, error, expected);
    }
    Require(error == expected && result == nullptr);
}

void Numeric() {
    struct Case { const char* service; unsigned port; };
    const Case cases[] = {
        {nullptr, 0}, {"0", 0}, {"-0", 0}, {"+0", 0}, {"00000", 0},
        {"80", 80}, {"0080", 80}, {"010", 10}, {"+443", 443}, {" 443", 443},
        {"\t\n\v\f\r 65535", 65535}, {"65535", 65535},
        {"-18446744073709551615", 1}, {"-18446744073709551536", 80},
        {"-18446744073709486081", 65535}
    };
    const std::array<std::array<int, 2>, 7> transports{{{0, 0}, {1, 0}, {2, 0}, {0, 6}, {0, 17}, {1, 6}, {2, 17}}};
    for (const auto& item : cases) {
        for (const int family : {2, 28}) {
            for (const int numericFlag : {0, 8}) {
                for (const auto& transport : transports)
                    ExpectPort(item.service, item.port, family, transport[0], transport[1], 4 | numericFlag);
            }
        }
    }
    const std::string manyZeros = std::string(512, '0') + "65535";
    const std::string negativeZero = "-" + std::string(512, '0');
    ExpectPort(manyZeros.c_str(), 65535, 2, 1, 6, 12);
    ExpectPort(negativeZero.c_str(), 0, 28, 2, 17, 12);
    ExpectPort("-18446744073709551536", 80, 2, 0, 0, 0, true);
    ExpectPort(" \t+443", 443, 28, 0, 0, 0, true);
    for (const int family : {2, 28}) {
        for (const int passive : {0, 1}) {
            const auto hints = Hints(family, 1, 6, 8 | passive);
            GuestResolver::AddressInfo* result = nullptr;
            Require(getaddrinfo_nid_postfix(nullptr, " +80", &hints, &result) == 0 && result);
            for (const auto* entry = result; entry; entry = entry->next) {
                Require(entry->family == family && Port(*entry) == 80);
                const auto* address = static_cast<const unsigned char*>(entry->address) + (family == 2 ? 4 : 8);
                const std::size_t size = family == 2 ? 4 : 16;
                std::array<unsigned char, 16> expected{};
                if (!passive) {
                    if (family == 2) expected[0] = 127;
                    expected[size - 1] = 1;
                }
                Require(std::memcmp(address, expected.data(), size) == 0);
            }
            freeaddrinfo_nid_postfix(result);
        }
    }
}

void Errors() {
    const char* outsidePort[] = {
        "65536", "99999", "2147483647", "2147483648", "4294967295", "+00065536", " \t65536",
        "-18446744073709486080", "-18446744069414584321"
    };
    const char* notNumeric[] = {
        "", " ", "+", "-", "1x", "0x50", "1 ", "80\n", "+ 1", "-1", "-80", "4294967296",
        "18446744073709551615", "18446744073709551616", "-18446744073709551616",
        "999999999999999999999999999999999999999999999999999999999999999"
    };
    for (const char* service : outsidePort) {
        auto* result = reinterpret_cast<GuestResolver::AddressInfo*>(std::uintptr_t{1});
        Require(getaddrinfo_nid_postfix("127.0.0.1", service, nullptr, &result) == 9 && result == nullptr);
    }
    for (const int family : {2, 28}) {
        const char* node = family == 28 ? "::1" : "127.0.0.1";
        for (const int type : {0, 1, 2}) {
            for (const int numericFlag : {0, 8}) {
                const auto hints = Hints(family, type, 0, 4 | numericFlag);
                for (const char* service : outsidePort) ExpectError(node, service, hints, 9);
            }
            const auto hints = Hints(family, type, 0, 12);
            for (const char* service : notNumeric) ExpectError(node, service, hints, 8);
        }
    }
    auto hints = Hints(2, 1, 6, 12);
    ExpectError("invalid numeric host", "65536", hints, 9);
    ExpectError(nullptr, "65536", hints, 9);
    hints.addressLength = 1;
    ExpectError("127.0.0.1", "65536", hints, 12);
    hints.addressLength = 0;
    hints.flags |= 0x40000000;
    ExpectError("127.0.0.1", "65536", hints, 3);
    hints.flags = 12;
    hints.family = 1234;
    ExpectError("127.0.0.1", "65536", hints, 5);
    hints.family = 2;
    hints.socketType = 1234;
    hints.protocol = 0;
    ExpectError("127.0.0.1", "65536", hints, 10);
    for (const auto& pair : std::array<std::array<int, 2>, 6>{{{1, 17}, {2, 6}, {1, 136}, {2, 132}, {1, -1}, {2, 999}}}) {
        hints = Hints(2, pair[0], pair[1], 12);
        ExpectError("127.0.0.1", "65536", hints, 12);
        ExpectError("127.0.0.1", nullptr, hints, 12);
        ExpectError(reinterpret_cast<const char*>(std::uintptr_t{1}), "80", hints, 12);
    }
    hints = Hints(2, 3, 1, 12);
    for (const char* service : {"", "80", "-1", "65536", "18446744073709551616"})
        ExpectError("127.0.0.1", service, hints, 9);
    ExpectError(nullptr, nullptr, hints, 8);
}

void Named() {
    const char* names[] = {"http", "domain", "www", "https", "", "80 ", "0x50", "-1", "4294967296"};
    for (const char* name : names) {
        std::array<bool, 2> present{};
        std::array<unsigned, 2> ports{};
        for (std::size_t index = 0; index < 2; ++index) {
            const auto* entry = ::getservbyname(name, index == 0 ? "tcp" : "udp");
            present[index] = entry != nullptr;
            if (entry) ports[index] = ntohs(static_cast<std::uint16_t>(entry->s_port));
        }
        for (const int family : {2, 28}) {
            const char* node = family == 28 ? "::1" : "127.0.0.1";
            for (int type = 0; type <= 2; ++type) {
                const auto hints = Hints(family, type, 0, 4);
                const bool expected = type == 0 ? present[0] || present[1] : present[type - 1];
                GuestResolver::AddressInfo* result = nullptr;
                const auto error = getaddrinfo_nid_postfix(node, name, &hints, &result);
                if (!expected) {
                    Require(error == 9 && result == nullptr);
                    continue;
                }
                Require(error == 0 && result != nullptr);
                std::array<bool, 2> found{};
                for (const auto* entry = result; entry; entry = entry->next) {
                    Require(entry->socketType == 1 || entry->socketType == 2);
                    const auto index = static_cast<std::size_t>(entry->socketType - 1);
                    Require(present[index] && Port(*entry) == ports[index]);
                    Require(!type || entry->socketType == type);
                    found[index] = true;
                }
                if (!type) Require(found == present);
                freeaddrinfo_nid_postfix(result);
            }
        }
    }
    struct Task { const char* name; unsigned port; bool stream; };
    std::vector<Task> tasks;
    for (int index = 0; index < 4; ++index) {
        const bool stream = index % 2 == 0;
        const char* name = stream ? "http" : "domain";
        const auto* entry = ::getservbyname(name, stream ? "tcp" : "udp");
        if (!entry) continue;
        const auto port = ntohs(static_cast<std::uint16_t>(entry->s_port));
        tasks.push_back({name, port, stream});
    }
    std::vector<std::thread> workers;
    for (const auto& task : tasks) {
        workers.emplace_back([task] {
            for (int iteration = 0; iteration < 16; ++iteration)
                ExpectPort(task.name, task.port, 2, task.stream ? 1 : 2, task.stream ? 6 : 17, 4);
        });
    }
    for (auto& worker : workers) worker.join();
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
#ifdef _WIN32
    WSADATA data{};
    Require(WSAStartup(MAKEWORD(2, 2), &data) == 0);
#endif
    const std::string mode = argv[1];
    if (mode == "numeric") Numeric();
    else if (mode == "errors") Errors();
    else if (mode == "named") Named();
    else Require(false);
#ifdef _WIN32
    WSACleanup();
#endif
    std::puts("FreeBSD resolver service checks passed");
}
