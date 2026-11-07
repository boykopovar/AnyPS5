#include "prx/libSceVrHand/include/Api.hpp"
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrNotInit = static_cast<int>(0x8a870001);
constexpr int kErrAlreadyInit = static_cast<int>(0x8a870002);
constexpr int kErrNotStarted = static_cast<int>(0x8a870003);
constexpr int kErrAlreadyStarted = static_cast<int>(0x8a870004);
constexpr int kErrInvalidArg = static_cast<int>(0x8a870006);
constexpr std::uint8_t kFill = 0xaa;

struct GuardedRegion {
    std::size_t pageSize{0};
    unsigned char* memory{nullptr};

    GuardedRegion() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = static_cast<std::size_t>(info.dwPageSize);
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(memory != nullptr);
        DWORD previous;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_NOACCESS, &previous) != 0);
#else
        const long nativePageSize = sysconf(_SC_PAGESIZE);
        Require(nativePageSize > 0);
        pageSize = static_cast<std::size_t>(nativePageSize);
        void* allocation = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(allocation != MAP_FAILED);
        memory = static_cast<unsigned char*>(allocation);
        Require(mprotect(memory + pageSize, pageSize, PROT_NONE) == 0);
#endif
    }

    ~GuardedRegion() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, pageSize * 2);
#endif
    }
};

std::array<std::uint8_t, 0x40> ValidParam() {
    std::array<std::uint8_t, 0x40> param{};
    param[0] = 0x20;
    return param;
}

bool Untouched(const std::array<std::uint8_t, 0x28>& out) {
    for (const auto value : out) if (value != kFill) return false;
    return true;
}

int Query(const std::array<std::uint8_t, 0x40>& param, std::array<std::uint8_t, 0x28>& out) {
    out.fill(kFill);
    return sceVrHandQueryMemory(param.data(), out.data());
}

void CheckLifecycle() {
    Require(sceVrHandFinalize() == kErrNotInit);
    Require(sceVrHandStart() == kErrNotInit);
    Require(sceVrHandStop() == kErrNotInit);
    Require(sceVrHandGetMesh() == kErrNotInit);
    Require(sceVrHandGetMeshSize() == kErrNotInit);
    Require(sceVrHandGetResult() == kErrNotInit);

    Require(sceVrHandInitialize() == 0);
    Require(sceVrHandInitialize() == kErrAlreadyInit);

    Require(sceVrHandStop() == kErrNotStarted);
    Require(sceVrHandGetMesh() == kErrNotStarted);
    Require(sceVrHandGetMeshSize() == kErrNotStarted);
    Require(sceVrHandGetResult() == kErrNotStarted);

    Require(sceVrHandStart() == 0);
    Require(sceVrHandStart() == kErrAlreadyStarted);

    Require(sceVrHandStop() == 0);
    Require(sceVrHandStop() == kErrNotStarted);

    Require(sceVrHandFinalize() == 0);
    Require(sceVrHandFinalize() == kErrNotInit);
}

}

int main() {
    CheckLifecycle();
    std::array<std::uint8_t, 0x28> out{};
    auto param = ValidParam();

    out.fill(kFill);
    auto* const badPtr = reinterpret_cast<void*>(std::uintptr_t{0x10});
    Require(sceVrHandQueryMemory(badPtr, out.data()) == kErrInvalidArg && Untouched(out));
    Require(sceVrHandQueryMemory(param.data(), badPtr) == kErrInvalidArg);
    Require(sceVrHandQueryMemory(nullptr, out.data()) == kErrInvalidArg && Untouched(out));
    Require(sceVrHandQueryMemory(param.data(), nullptr) == kErrInvalidArg);

    GuardedRegion region;
    auto* const boundaryParam = region.memory + region.pageSize - 0x20;
    std::memset(boundaryParam, 0, 0x20);
    boundaryParam[0] = 0x20;
    out.fill(kFill);
    Require(sceVrHandQueryMemory(boundaryParam, out.data()) == 0);

    auto* const spanningParam = region.memory + region.pageSize - 0x10;
    out.fill(kFill);
    Require(sceVrHandQueryMemory(spanningParam, out.data()) == kErrInvalidArg && Untouched(out));

    auto* const boundaryOut = region.memory + region.pageSize - 28;
    std::memset(boundaryOut, kFill, 28);
    Require(sceVrHandQueryMemory(param.data(), boundaryOut) == 0);

    auto* const spanningOut = region.memory + region.pageSize - 8;
    Require(sceVrHandQueryMemory(param.data(), spanningOut) == kErrInvalidArg);

    for (const std::uint32_t size : {0u, 0x1fu, 0x21u, 0x40u, 0x120u, 0x10020u, 0x1000020u}) {
        param = ValidParam();
        std::memcpy(param.data(), &size, sizeof(size));
        Require(Query(param, out) == kErrInvalidArg && Untouched(out));
    }
    for (std::size_t index = 4; index < 0x20; ++index) {
        param = ValidParam();
        param[index] = 1;
        Require(Query(param, out) == kErrInvalidArg && Untouched(out));
    }

    param = ValidParam();
    std::memset(param.data() + 0x20, 0xcc, 0x20);
    Require(Query(param, out) == 0);
    std::uint32_t at0 = 0;
    std::uint32_t at4 = 0;
    std::memcpy(&at0, out.data(), sizeof(at0));
    std::memcpy(&at4, out.data() + 4, sizeof(at4));
    Require(at0 == 0x3c00000);
    Require(at4 == 0x10000);
    for (std::size_t index = 8; index < 0x1c; ++index) Require(out[index] == 0);
    for (std::size_t index = 0x1c; index < out.size(); ++index) Require(out[index] == kFill);
    return 0;
}
