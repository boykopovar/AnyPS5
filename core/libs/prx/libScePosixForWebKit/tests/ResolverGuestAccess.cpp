#include "prx/libScePosixForWebKit/include/Api.hpp"
#include "prx/libScePosixForWebKit/GuestResolver.hpp"
#include "Testing.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

class Pages {
public:
    Pages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous));
#else
        size = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
        data = static_cast<char*>(mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(data != MAP_FAILED);
        Require(mprotect(data + size, size, PROT_NONE) == 0);
#endif
    }

    ~Pages() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }

    char* End(std::size_t bytes = 0) { return data + size - bytes; }

    void ReadOnly() {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(data, size, PAGE_READONLY, &previous));
#else
        Require(mprotect(data, size, PROT_READ) == 0);
#endif
    }

private:
    char* data = nullptr;
    std::size_t size = 0;
};

void AddressInfo() {
    Pages names;
    Pages services;
    Pages output;
    char* node = names.End(10);
    char* service = services.End(4);
    std::memcpy(node, "127.0.0.1", 10);
    std::memcpy(service, "443", 4);
    names.ReadOnly();
    services.ReadOnly();
    GuestResolver::AddressInfo hints{};
    hints.flags = 14;
    hints.family = 2;
    hints.socketType = 1;
    GuestResolver::AddressInfo* result = nullptr;
    Require(getaddrinfo_nid_postfix(node, service, &hints, &result) == 0 && result);
    Require(result->canonicalName && std::strcmp(result->canonicalName, node) == 0);
    const auto* address = static_cast<const unsigned char*>(result->address);
    const std::array<unsigned char, 8> expected{16, 2, 1, 187, 127, 0, 0, 1};
    Require(std::equal(expected.begin(), expected.end(), address));
    freeaddrinfo_nid_postfix(result);

    result = reinterpret_cast<GuestResolver::AddressInfo*>(1);
    Require(getaddrinfo_nid_postfix(names.End(), service, &hints, &result) == 4 && result == nullptr);
    Require(getaddrinfo_nid_postfix(node, services.End(), &hints, &result) == 4 && result == nullptr);
    hints.family = 1234;
    Require(getaddrinfo_nid_postfix(names.End(), services.End(), &hints, &result) == 5 && result == nullptr);
    hints.family = 2;
    hints.socketType = 3;
    Require(getaddrinfo_nid_postfix(names.End(), services.End(), &hints, &result) == 9 && result == nullptr);
    hints.socketType = 1;
    auto** readOnlyResult = reinterpret_cast<GuestResolver::AddressInfo**>(output.End(sizeof(result)));
    *readOnlyResult = reinterpret_cast<GuestResolver::AddressInfo*>(1);
    output.ReadOnly();
    Require(getaddrinfo_nid_postfix(node, service, &hints, readOnlyResult) == 4);
    Require(*readOnlyResult == reinterpret_cast<GuestResolver::AddressInfo*>(1));
}

void NumericCanonicalNames() {
    struct Case {
        const char* name;
        int family;
    };
    const Case cases[] = {
        {"127.0.0.1", 2},
        {"::1", 28},
        {"0:0:0:0:0:0:0:1", 28},
        {"2001:0DB8:0:0::1", 28},
        {"::FFFF:127.0.0.1", 28}
    };
    for (const auto& item : cases) {
        for (const int numeric : {0, 4}) {
            for (const int socketType : {1, 2}) {
                GuestResolver::AddressInfo hints{};
                hints.flags = 2 | 8 | numeric;
                hints.family = item.family;
                hints.socketType = socketType;
                GuestResolver::AddressInfo* result = nullptr;
                Require(getaddrinfo_nid_postfix(item.name, "443", &hints, &result) == 0 && result);
                for (auto* entry = result; entry; entry = entry->next) {
                    Require(entry->family == item.family && entry->socketType == socketType);
                    Require(entry->canonicalName && std::strcmp(entry->canonicalName, item.name) == 0);
                }
                freeaddrinfo_nid_postfix(result);
            }
        }
    }
}

void HostEntry() {
    Pages names;
    Pages output;
    Pages header;
    auto* node = names.End(10);
    std::memcpy(node, "127.0.0.1", 10);
    names.ReadOnly();
    auto* legacy = gethostbyname_nid_postfix(node);
    Require(legacy && std::strcmp(legacy->name, node) == 0);
    ExpectErr<std::invalid_argument>([&] { gethostbyname_nid_postfix(names.End()); });

    GuestResolver::HostEntry entry{};
    GuestResolver::HostEntry* result = nullptr;
    auto* buffer = output.End(48);
    std::fill_n(buffer, 48, static_cast<char>(0x5a));
    int hostError = 77;
    Require(scePosixForWebKit_gethostbyname_r(node, &entry, buffer, 38, &result, &hostError) == 0);
    Require(result == &entry && hostError == 77);
    Require(entry.addresses == reinterpret_cast<char**>(buffer));
    Require(entry.addresses[0] == buffer + 24 && entry.addresses[1] == nullptr);
    Require(entry.aliases == reinterpret_cast<char**>(buffer + 16) && entry.aliases[0] == nullptr);
    Require(entry.name == buffer + 28 && std::strcmp(entry.name, node) == 0);
    Require(std::all_of(buffer + 38, output.End(), [](char value) { return value == 0x5a; }));
    Require(scePosixForWebKit_gethostbyname_r(node, &entry, buffer + 1, 45, &result,
        reinterpret_cast<int*>(names.End())) == 0);
    Require(entry.addresses == reinterpret_cast<char**>(buffer + 8));

    const auto before = entry;
    std::array<char, sizeof(entry)> saved;
    std::memcpy(saved.data(), &before, sizeof(before));
    std::fill_n(output.End(16), 16, static_cast<char>(0x5a));
    ExpectErr<std::invalid_argument>([&] { scePosixForWebKit_gethostbyname_r(node, &entry, output.End(16), 128, &result, &hostError); });
    Require(result == nullptr && std::memcmp(&entry, saved.data(), sizeof(entry)) == 0);
    Require(std::all_of(output.End(16), output.End(), [](char value) { return value == 0x5a; }));

    auto* readOnlyEntry = reinterpret_cast<GuestResolver::HostEntry*>(header.End(sizeof(entry)));
    std::memcpy(readOnlyEntry, &entry, sizeof(entry));
    header.ReadOnly();
    std::fill_n(buffer, 48, static_cast<char>(0x5a));
    ExpectErr<std::invalid_argument>([&] { scePosixForWebKit_gethostbyname_r(node, readOnlyEntry, buffer, 48, &result, &hostError); });
    Require(std::all_of(buffer, output.End(), [](char value) { return value == 0x5a; }));
    Require(std::memcmp(readOnlyEntry, saved.data(), sizeof(entry)) == 0);
    ExpectErr<std::invalid_argument>([&] { scePosixForWebKit_gethostbyname_r(node, &entry, buffer, 48,
        reinterpret_cast<GuestResolver::HostEntry**>(output.End()), &hostError); });
    ExpectErr<std::invalid_argument>([&] { scePosixForWebKit_gethostbyname_r(node, &entry, buffer, 0, &result,
        reinterpret_cast<int*>(output.End())); });
    Require(result == nullptr && errno == 34);
}

}

int main() {
    AddressInfo();
    NumericCanonicalNames();
    HostEntry();
    std::puts("resolver checked guest access tests passed");
}
