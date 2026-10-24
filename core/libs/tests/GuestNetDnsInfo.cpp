#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

struct NetDnsInfo {
    std::uint32_t primaryAddress;
    std::uint32_t secondaryAddress;
};

struct NetDns6Info {
    std::uint8_t primaryAddress[16];
    std::uint8_t secondaryAddress[16];
};

extern "C" {
int APS5_VABI sceNetGetDnsInfo(NetDnsInfo* info, std::uint32_t flags);
int APS5_VABI sceNetSetDnsInfo(const NetDnsInfo* info, std::uint32_t flags);
int APS5_VABI sceNetGetDns6Info(NetDns6Info* info, std::uint32_t flags);
int APS5_VABI sceNetSetDns6Info(const NetDns6Info* info, std::uint32_t flags);
int* APS5_VABI sceNetErrnoLoc();
}

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrorBase = static_cast<int>(0x80410100);
constexpr int kEfault = 14;
constexpr int kEinval = 22;

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

bool Failed(int result, int error) { return result == (kErrorBase | error) && *sceNetErrnoLoc() == error; }

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
    Require(Throws([] { NetDns6Info unused{}; sceNetGetDns6Info(&unused, 1); }));
    Require(Throws([] { sceNetGetDns6Info(nullptr, 0); }));
    std::memset(&info6, 0xff, sizeof(info6));
    Require(sceNetGetDns6Info(&info6, 0) == 0);
    for (const auto byte : info6.primaryAddress) Require(byte == 0);
    for (const auto byte : info6.secondaryAddress) Require(byte == 0);

    NetDns6Info set6{};
    Require(Failed(sceNetSetDns6Info(&set6, 0), kEinval));
    set6.primaryAddress[0] = 0x20;
    set6.primaryAddress[15] = 0x01;
    Require(Failed(sceNetSetDns6Info(&set6, 2), kEinval));
    Require(sceNetSetDns6Info(&set6, 0) == 0);
    Require(sceNetGetDns6Info(&info6, 0) == 1 && std::memcmp(&info6, &set6, sizeof(set6)) == 0);
    set6.secondaryAddress[0] = 0x26;
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
    Require(Failed(sceNetSetDnsInfo(static_cast<const NetDnsInfo*>(page), 0), kEfault));
    NetDnsInfo read{};
    Require(sceNetGetDnsInfo(&read, 0) == 1 && read.primaryAddress == 0x08080808u);
    Require(sceNetSetDnsInfo(nullptr, 0) == 0);

    NetDns6Info info6{};
    info6.primaryAddress[0] = 0x20;
    Require(sceNetSetDns6Info(&info6, 0) == 0);
    Require(Failed(sceNetSetDns6Info(static_cast<const NetDns6Info*>(page), 0), kEfault));
    NetDns6Info read6{};
    Require(sceNetGetDns6Info(&read6, 0) == 1 && std::memcmp(&read6, &info6, sizeof(info6)) == 0);
    Require(sceNetSetDns6Info(nullptr, 0) == 0);
}

int main() {
    CheckDns();
    CheckUnreadableInfo();
    return 0;
}
