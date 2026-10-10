#pragma once
#include <cstddef>
#include <cstdint>

namespace GuestResolver {
struct HostEntry {
    char* name;
    char** aliases;
    std::int32_t family;
    std::int32_t length;
    char** addresses;
};
static_assert(sizeof(HostEntry) == 32);
static_assert(offsetof(HostEntry, family) == 16);
static_assert(offsetof(HostEntry, length) == 20);
static_assert(offsetof(HostEntry, addresses) == 24);

struct AddressInfo {
    std::int32_t flags;
    std::int32_t family;
    std::int32_t socketType;
    std::int32_t protocol;
    std::uint32_t addressLength;
    char* canonicalName;
    void* address;
    AddressInfo* next;
};
static_assert(sizeof(AddressInfo) == 48);
static_assert(offsetof(AddressInfo, canonicalName) == 24);
static_assert(offsetof(AddressInfo, address) == 32);
}
