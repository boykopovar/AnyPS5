#include "prx/libSceVrHand/include/Api.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <array>
#include <cerrno>
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

void ClobberErrno(std::uintptr_t, std::size_t) {
    errno = 99;
}

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrNotInit = static_cast<int>(0x8a870001);
constexpr int kErrAlreadyInit = static_cast<int>(0x8a870002);
constexpr int kErrInvalidArg = static_cast<int>(0x8a870006);
constexpr std::uint8_t kFill = 0xaa;

struct TestInitParam {
    std::uint32_t size{0x40};
    std::uint32_t unk04{0};
    std::uint32_t unk08{0x100};
    std::uint32_t pad0c{0};
    std::uint64_t unk10{0};
    std::uint32_t unk18{0};
    std::uint32_t unk1c{0};
    void* workMemory{nullptr};
    std::uint32_t memorySize{0x3c00000};
    std::uint32_t memoryAlignment{0x10000};
    std::uint32_t unk30{0};
    std::uint32_t unk34{0};
    std::uint32_t unk38{0};
    std::uint8_t unk3c{0};
    std::uint8_t unk3d{0};
    std::uint8_t pad3e[2]{};
};

struct TestStartParam {
    std::uint32_t size{0x10};
    std::uint32_t mode{0};
    std::uint8_t reserved[8]{};
};

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

void CheckMeshSize() {
    constexpr std::array<std::uint8_t, 0x1a> expected{
        0x1a, 0, 0, 0, 0x65, 0x09, 0, 0, 0x10, 0x32, 0, 0,
    };
    std::array<std::uint8_t, 32> buffer;
    buffer.fill(kFill);
    errno = 71;
    Require(sceVrHandGetMeshSize(buffer.data() + 1) == 0);
    Require(errno == 71);
    Require(std::memcmp(buffer.data() + 1, expected.data(), expected.size()) == 0);
    Require(buffer[0] == kFill);
    for (std::size_t i = 1 + expected.size(); i < buffer.size(); ++i) Require(buffer[i] == kFill);
}

int Query(const std::array<std::uint8_t, 0x40>& param, std::array<std::uint8_t, 0x28>& out) {
    out.fill(kFill);
    return sceVrHandQueryMemory(param.data(), out.data());
}

}

int main() {
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(ClobberErrno);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    std::array<std::uint8_t, 0x28> out{};
    auto param = ValidParam();

    out.fill(kFill);
    auto* const badPtr = reinterpret_cast<void*>(std::uintptr_t{0x10});
    errno = 71;
    Require(sceVrHandQueryMemory(badPtr, out.data()) == kErrInvalidArg && Untouched(out));
    Require(errno == 71);
    errno = 71;
    Require(sceVrHandQueryMemory(param.data(), badPtr) == kErrInvalidArg);
    Require(errno == 71);
    Require(sceVrHandQueryMemory(nullptr, out.data()) == kErrInvalidArg && Untouched(out));
    Require(sceVrHandQueryMemory(param.data(), nullptr) == kErrInvalidArg);

    GuardedRegion region;
    auto* const boundaryParam = region.memory + region.pageSize - 0x20;
    std::memset(boundaryParam, 0, 0x20);
    boundaryParam[0] = 0x20;
    out.fill(kFill);
    errno = 71;
    Require(sceVrHandQueryMemory(boundaryParam, out.data()) == 0);
    Require(errno == 71);

    auto* const spanningParam = region.memory + region.pageSize - 0x10;
    spanningParam[0] = 0x20;
    out.fill(kFill);
    errno = 71;
    Require(sceVrHandQueryMemory(spanningParam, out.data()) == kErrInvalidArg && Untouched(out));
    Require(errno == 71);

    auto* const boundaryOut = region.memory + region.pageSize - 28;
    std::memset(boundaryOut, kFill, 28);
    errno = 71;
    Require(sceVrHandQueryMemory(param.data(), boundaryOut) == 0);
    Require(errno == 71);

    auto* const spanningOut = region.memory + region.pageSize - 8;
    errno = 71;
    Require(sceVrHandQueryMemory(param.data(), spanningOut) == kErrInvalidArg);
    Require(errno == 71);

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
    GuestArena::GuestArenaSetHostReadHook_nid_no_patch(nullptr);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);

    Require(sceVrHandStop() == kErrNotInit);
    TestStartParam startParam{};
    Require(sceVrHandStart(&startParam) == kErrNotInit);
    Require(sceVrHandGetMesh() == kErrNotInit);
    out.fill(kFill);
    Require(sceVrHandGetMeshSize(nullptr) == kErrNotInit);
    Require(sceVrHandGetMeshSize(out.data()) == kErrNotInit && Untouched(out));
    Require(sceVrHandGetResult() == kErrNotInit);
    Require(sceVrHandFinalize() == 0);

    Require(sceVrHandInitialize(nullptr) == kErrInvalidArg);
    TestInitParam initParam{};
    Require(sceVrHandInitialize(&initParam) == kErrInvalidArg);
    std::uint8_t dummyMemory[16]{};
    initParam.workMemory = dummyMemory;
    Require(sceVrHandInitialize(&initParam) == 0);
    Require(sceVrHandInitialize(&initParam) == kErrAlreadyInit);
    Require(sceVrHandGetMeshSize(nullptr) == kErrInvalidArg);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(ClobberErrno);
    CheckMeshSize();
    auto* const meshBoundary = region.memory + region.pageSize - 0x1a;
    std::memset(meshBoundary, kFill, 0x1a);
    Require(sceVrHandGetMeshSize(meshBoundary) == 0 && meshBoundary[0x19] == 0);
    errno = 71;
    Require(sceVrHandGetMeshSize(region.memory + region.pageSize) == kErrInvalidArg);
    Require(errno == 71);
    GuestArena::GuestArenaSetHostWriteHook_nid_no_patch(nullptr);

    Require(sceVrHandStop() == 0);
    Require(sceVrHandStart(nullptr) == kErrInvalidArg);
    TestStartParam badStart{};
    badStart.size = 0x20;
    Require(sceVrHandStart(&badStart) == kErrInvalidArg);
    badStart.size = 0x10;
    badStart.mode = 5;
    Require(sceVrHandStart(&badStart) == kErrInvalidArg);

    Require(sceVrHandStart(&startParam) == 0);
    Require(sceVrHandStart(&startParam) == 0);
    CheckMeshSize();
    Require(sceVrHandStop() == 0);
    CheckMeshSize();
    Require(sceVrHandStop() == 0);

    Require(sceVrHandFinalize() == 0);
    out.fill(kFill);
    Require(sceVrHandGetMeshSize(out.data()) == kErrNotInit && Untouched(out));
    Require(sceVrHandStop() == kErrNotInit);

    return 0;
}
