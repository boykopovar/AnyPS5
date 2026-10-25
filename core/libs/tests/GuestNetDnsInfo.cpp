#include "prx/libSceNet/include/Net.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrorBase = static_cast<int>(0x80410100);
constexpr int kEfault = 14;
constexpr int kEinval = 22;
constexpr int kEnotinit = 200;

bool Failed(int result, int error) { return result == (kErrorBase | error) && *sceNetErrnoLoc() == error; }

void CheckUninitialized() {
    NetDnsInfo info{0x08080808u, 0x01010101u};
    const NetDnsInfo before = info;
    NetDns6Info info6{};
    std::memset(&info6, 0x5a, sizeof(info6));
    const NetDns6Info before6 = info6;
    for (const std::uint32_t flags : {0u, 1u, 0xffffffffu}) {
        Require(Failed(sceNetGetDnsInfo(&info, flags), kEnotinit));
        Require(Failed(sceNetSetDnsInfo(&info, flags), kEnotinit));
        Require(Failed(sceNetGetDns6Info(&info6, flags), kEnotinit));
        Require(Failed(sceNetSetDns6Info(&info6, flags), kEnotinit));
        Require(Failed(sceNetGetDnsInfo(nullptr, flags), kEnotinit));
        Require(Failed(sceNetSetDnsInfo(nullptr, flags), kEnotinit));
        Require(Failed(sceNetGetDns6Info(nullptr, flags), kEnotinit));
        Require(Failed(sceNetSetDns6Info(nullptr, flags), kEnotinit));
        Require(std::memcmp(&info, &before, sizeof(info)) == 0);
        Require(std::memcmp(&info6, &before6, sizeof(info6)) == 0);
    }
}

void CheckDns() {
    NetDnsInfo info{0x08080808u, 0};
    Require(Failed(sceNetSetDnsInfo(&info, 1), kEinval));
    NetDnsInfo empty{0, 0x01010101u};
    Require(Failed(sceNetSetDnsInfo(&empty, 0), kEinval));

    Require(sceNetSetDnsInfo(&info, 0) == 0);
    NetDnsInfo read{1, 1};
    Require(sceNetGetDnsInfo(&read, 0) == 1 && read.primaryAddress == 0x08080808u && read.secondaryAddress == 0);
    info.secondaryAddress = 0x04040808u;
    Require(sceNetSetDnsInfo(&info, 0) == 0);
    Require(sceNetGetDnsInfo(&read, 0) == 2 && read.primaryAddress == 0x08080808u && read.secondaryAddress == 0x04040808u);
    Require(sceNetSetDnsInfo(nullptr, 0) == 0);
    Require(sceNetGetDnsInfo(&read, 0) >= 0);

    NetDns6Info info6{};
    Require(Failed(sceNetGetDnsInfo(nullptr, 0), kEinval));
    Require(Failed(sceNetGetDns6Info(nullptr, 0), kEinval));
    for (const std::uint32_t flags : {1u, 2u, 0xffffffffu}) {
        NetDnsInfo unchanged{0x5a5a5a5au, 0x5a5a5a5au};
        const NetDnsInfo before = unchanged;
        std::memset(&info6, 0x5a, sizeof(info6));
        const NetDns6Info before6 = info6;
        Require(Failed(sceNetGetDnsInfo(&unchanged, flags), kEinval));
        Require(Failed(sceNetGetDns6Info(&info6, flags), kEinval));
        Require(Failed(sceNetGetDnsInfo(nullptr, flags), kEinval));
        Require(Failed(sceNetGetDns6Info(nullptr, flags), kEinval));
        Require(Failed(sceNetSetDnsInfo(nullptr, flags), kEinval));
        Require(Failed(sceNetSetDns6Info(nullptr, flags), kEinval));
        Require(std::memcmp(&unchanged, &before, sizeof(before)) == 0);
        Require(std::memcmp(&info6, &before6, sizeof(before6)) == 0);
    }
    std::memset(&info6, 0xff, sizeof(info6));
    Require(sceNetGetDns6Info(&info6, 0) == 0);
    for (const auto byte : info6.primaryAddress.bytes) Require(byte == 0);
    for (const auto byte : info6.secondaryAddress.bytes) Require(byte == 0);

    NetDns6Info set6{};
    Require(Failed(sceNetSetDns6Info(&set6, 0), kEinval));
    set6.primaryAddress.bytes[0] = 0x20;
    set6.primaryAddress.bytes[15] = 0x01;
    Require(Failed(sceNetSetDns6Info(&set6, 2), kEinval));
    Require(sceNetSetDns6Info(&set6, 0) == 0);
    Require(sceNetGetDns6Info(&info6, 0) == 1 && std::memcmp(&info6, &set6, sizeof(set6)) == 0);
    set6.secondaryAddress.bytes[0] = 0x26;
    Require(sceNetSetDns6Info(&set6, 0) == 0);
    Require(sceNetGetDns6Info(&info6, 0) == 2 && std::memcmp(&info6, &set6, sizeof(set6)) == 0);
    Require(sceNetSetDns6Info(nullptr, 0) == 0);
    Require(sceNetGetDns6Info(&info6, 0) == 0);
}

}

void* UnreadablePage() {
#ifdef _WIN32
    return VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS);
#else
    void* page = mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return page == MAP_FAILED ? nullptr : page;
#endif
}

void CheckUnreadableInfo() {
    void* page = UnreadablePage();
    Require(page != nullptr);
    NetDnsInfo info{0x08080808u, 0};
    Require(sceNetSetDnsInfo(&info, 0) == 0);
    Require(Failed(sceNetSetDnsInfo(static_cast<const NetDnsInfo*>(page), 1), kEinval));
    Require(Failed(sceNetSetDnsInfo(static_cast<const NetDnsInfo*>(page), 0), kEfault));
    NetDnsInfo read{};
    Require(sceNetGetDnsInfo(&read, 0) == 1 && read.primaryAddress == 0x08080808u);
    Require(sceNetSetDnsInfo(nullptr, 0) == 0);

    NetDns6Info info6{};
    info6.primaryAddress.bytes[0] = 0x20;
    Require(sceNetSetDns6Info(&info6, 0) == 0);
    Require(Failed(sceNetSetDns6Info(static_cast<const NetDns6Info*>(page), 1), kEinval));
    Require(Failed(sceNetSetDns6Info(static_cast<const NetDns6Info*>(page), 0), kEfault));
    NetDns6Info read6{};
    Require(sceNetGetDns6Info(&read6, 0) == 1 && std::memcmp(&read6, &info6, sizeof(info6)) == 0);
    Require(sceNetSetDns6Info(nullptr, 0) == 0);
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

int main() {
    CheckUninitialized();
    Require(sceNetInit_nid_postfix() == 0);
    CheckDns();
    CheckUnreadableInfo();
    Require(sceNetTerm() == 0);
    CheckUninitialized();
    return 0;
}
