#include "prx/libSceSulphaDrv/libSceSulphaDrv.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

constexpr int ErrorNotInitialized = static_cast<int>(0x804D0002u);
constexpr int ErrorInvalidArgument = static_cast<int>(0x804D0003u);

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

struct SulphaConfig {
    std::uint64_t size;
    std::uint64_t capacity;
    std::uint32_t setting;
    std::uint32_t port;
};
static_assert(sizeof(SulphaConfig) == 0x18);

constexpr SulphaConfig ValidConfig{0x18, 0x400000, 3, 4001};

class GuardedPage {
public:
    unsigned char* data = nullptr;
    std::size_t size = 0;
    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "VirtualAlloc failed");
        DWORD previous = 0;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0, "VirtualProtect failed");
#else
        const auto pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "sysconf failed");
        size = static_cast<std::size_t>(pageSize);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "mmap failed");
        data = static_cast<unsigned char*>(mapping);
        Require(mprotect(data + size, size, PROT_NONE) == 0, "mprotect failed");
#endif
    }
    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(data, 0, MEM_RELEASE);
#else
        munmap(data, size * 2);
#endif
    }
    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;
};

std::array<SulphaConfig, 13> InvalidConfigs() {
    std::array<SulphaConfig, 13> configs{};
    configs.fill(ValidConfig);
    configs[0].setting = 4;
    configs[1].setting = 0xFFFFFFFFu;
    configs[2].size = 0;
    configs[3].size = 0x17;
    configs[4].size = 0x19;
    configs[5].size = 0x231;
    configs[6].size = 0xFFFFFFFFFFFFFFFFull;
    configs[7].capacity = 0;
    configs[8].capacity = 0x3FF;
    configs[9].capacity = 0xFFFFFFFFu;
    configs[10].port = 0x10000;
    configs[11].port = 0xFFFFFFFFu;
    configs[12].size = 0x230;
    configs[12].capacity = 0x3FF;
    return configs;
}

void CheckNotInitializedAnswers() {
    std::uint64_t info = 0;
    Require(sceSulphaGetClientInfo(2, &info) == ErrorInvalidArgument, "GetClientInfo must reject client 2");
    Require(sceSulphaGetClientInfo(0xFFFFFFFFu, &info) == ErrorInvalidArgument, "GetClientInfo must reject client -1");
    Require(sceSulphaGetClientInfo(0, nullptr) == ErrorInvalidArgument, "GetClientInfo must reject a null info");
    Require(sceSulphaGetClientInfo(0, &info) == ErrorNotInitialized, "GetClientInfo(0) must report no init");
    Require(sceSulphaGetClientInfo(1, &info) == ErrorNotInitialized, "GetClientInfo(1) must report no init");
    Require(info == 0, "GetClientInfo must not write before initialization");

    Require(sceSulphaGetConnectedClientCount() == ErrorNotInitialized, "GetConnectedClientCount must report no init");

    const std::uint64_t message = 0;
    Require(sceSulphaMessage(3, &message) == ErrorInvalidArgument, "Message must reject type 3");
    Require(sceSulphaMessage(0xFFFFFFFFu, &message) == ErrorInvalidArgument, "Message must reject type -1");
    Require(sceSulphaMessage(0, nullptr) == ErrorInvalidArgument, "Message must reject a null message");
    for (const std::uint32_t type : {0u, 1u, 2u})
        Require(sceSulphaMessage(type, &message) == ErrorNotInitialized, "Message must report no init");

    const std::uint64_t bookmark = 0;
    Require(sceSulphaSetBookmark(nullptr) == ErrorInvalidArgument, "SetBookmark must reject a null bookmark");
    Require(sceSulphaSetBookmark(&bookmark) == ErrorNotInitialized, "SetBookmark must report no init");

    Require(sceSulphaShutdown() == ErrorNotInitialized, "Shutdown must report no init");
    Require(sceSulphaShutdown() == ErrorNotInitialized, "A second Shutdown must report no init");
}

void CheckConfigAnswers() {
    std::uint64_t needed = 0x5A5A;
    Require(sceSulphaGetNeededMemory(nullptr, &needed) == ErrorInvalidArgument,
            "GetNeededMemory must reject a null configuration");
    Require(sceSulphaGetNeededMemory(&ValidConfig, nullptr) == ErrorInvalidArgument,
            "GetNeededMemory must reject a null result pointer");
    Require(sceSulphaInit(nullptr) == ErrorInvalidArgument, "Init must reject a null configuration");
    for (const SulphaConfig& config : InvalidConfigs()) {
        Require(sceSulphaGetNeededMemory(&config, &needed) == ErrorInvalidArgument,
                "GetNeededMemory must reject an invalid configuration");
        Require(sceSulphaInit(&config) == ErrorInvalidArgument, "Init must reject an invalid configuration");
    }
    Require(needed == 0x5A5A, "GetNeededMemory must not write for a rejected configuration");
}

void CheckFieldsReadInOrder() {
    GuardedPage page;
    unsigned char* config = page.data + page.size - 0x14;
    std::uint64_t needed = 0;
    const SulphaConfig cases[]{
        {0x18, 0x400000, 4, 0},
        {0x19, 0x400000, 3, 0},
        {0x18, 0x3FF, 3, 0},
    };
    for (const SulphaConfig& value : cases) {
        std::memcpy(config, &value, 0x14);
        Require(sceSulphaGetNeededMemory(config, &needed) == ErrorInvalidArgument,
                "GetNeededMemory must answer an earlier field before reading an unreadable port");
        Require(sceSulphaInit(config) == ErrorInvalidArgument,
                "Init must answer an earlier field before reading an unreadable port");
    }
}

} // namespace

int main() {
    for (const int expectedErrno : {0, 7, 42}) {
        errno = expectedErrno;
        CheckNotInitializedAnswers();
        CheckConfigAnswers();
        CheckFieldsReadInOrder();
        Require(errno == expectedErrno, "The early answers must not change errno");
    }
    CheckNotInitializedAnswers();
}
