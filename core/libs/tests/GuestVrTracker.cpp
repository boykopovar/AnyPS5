#include "prx/libSceVrTracker/include/Api.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr int NotInit = static_cast<int>(0x81260801u);
constexpr int ArgumentInvalid = static_cast<int>(0x81260806u);
constexpr int NotExecuteGpuSubmit = static_cast<int>(0x81260815u);
constexpr std::uint8_t Fill = 0xAA;
constexpr std::uint64_t FilledTime = 0xAAAAAAAAAAAAAAAAull;

using Buffer = std::array<std::uint8_t, 0x200>;

std::uint8_t g_onion[16];
std::uint8_t g_garlic[16];
std::uint8_t g_work[16];

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "VrTracker: %s\n", message);
        std::abort();
    }
}

bool Untouched(const Buffer& buffer) {
    for (std::uint8_t byte : buffer)
        if (byte != Fill) return false;
    return true;
}

template <typename TCall>
bool Throws(TCall call) {
    try {
        call();
    } catch (const std::runtime_error&) {
        return true;
    }
    return false;
}

class GuardedPage {
public:
    std::size_t size = 0;
    std::uint8_t* data = nullptr;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<std::uint8_t*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr, "guard page allocation");
        DWORD previous = 0;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0, "guard page protection");
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0, "page size");
        size = static_cast<std::size_t>(pageSize);
        void* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED, "guard page allocation");
        data = static_cast<std::uint8_t*>(mapping);
        Require(mprotect(data + size, size, PROT_NONE) == 0, "guard page protection");
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

    std::uint8_t* Guard() const { return data + size; }
};

const VrTrackerGpuSubmitParam* SubmitParam(const Buffer& buffer) {
    return reinterpret_cast<const VrTrackerGpuSubmitParam*>(buffer.data());
}

const VrTrackerGpuWaitParam* WaitParam(const Buffer& buffer) {
    return reinterpret_cast<const VrTrackerGpuWaitParam*>(buffer.data());
}

void SetSize(Buffer& buffer, std::uint32_t size) {
    std::memcpy(buffer.data(), &size, sizeof(size));
}

VrTrackerLegacy::InitParam ValidInit() {
    VrTrackerLegacy::InitParam init{};
    init.size = sizeof(init);
    init.direct_memory_onion_size = 0x400000;
    init.direct_memory_onion_alignment = 0x10000;
    init.direct_memory_garlic_size = 0x3000000;
    init.direct_memory_garlic_alignment = 0x10000;
    init.work_memory_size = 0x1000000;
    init.work_memory_alignment = 0x10000;
    init.direct_memory_onion = g_onion;
    init.direct_memory_garlic = g_garlic;
    init.work_memory = g_work;
    return init;
}

void CheckNotInit() {
    Require(sceVrTrackerGetTime(nullptr) == NotInit, "time null");
    std::uint64_t time = FilledTime;
    Require(sceVrTrackerGetTime(&time) == NotInit, "time return");
    Require(time == FilledTime, "time untouched");

    Buffer param{};
    param.fill(Fill);
    Require(sceVrTrackerGpuSubmit(nullptr) == NotInit, "submit null");
    Require(sceVrTrackerGpuSubmit(SubmitParam(param)) == NotInit, "submit return");
    Require(Untouched(param), "submit leaves param");
    Require(sceVrTrackerGpuWait(nullptr) == NotInit, "wait null");
    Require(sceVrTrackerGpuWait(WaitParam(param)) == NotInit, "wait return");
    Require(Untouched(param), "wait leaves param");
}

void CheckTimeInitialized(const GuardedPage& page) {
    Require(sceVrTrackerGetTime(nullptr) == ArgumentInvalid, "time null once initialized");

    std::uint64_t first = FilledTime;
    const std::uint64_t before = sceKernelGetProcessTime();
    Require(sceVrTrackerGetTime(&first) == 0, "time once initialized");
    const std::uint64_t after = sceKernelGetProcessTime();
    Require(before <= first && first <= after, "time is the process time");

    std::uint64_t second = FilledTime;
    Require(sceVrTrackerGetTime(&second) == 0, "second time once initialized");
    Require(first <= second, "time does not go back");

    auto* unwritable = reinterpret_cast<std::uint64_t*>(page.Guard());
    Require(sceVrTrackerGetTime(unwritable) == ArgumentInvalid, "time to an unwritable pointer");
}

void CheckWaitInitialized(const GuardedPage& page) {
    Require(sceVrTrackerGpuWait(nullptr) == ArgumentInvalid, "wait null once initialized");

    Buffer param{};
    param.fill(Fill);
    Require(sceVrTrackerGpuWait(WaitParam(param)) == ArgumentInvalid, "wait with a filled size");
    for (std::uint32_t size : {0u, 31u, 33u, 144u}) {
        SetSize(param, size);
        Require(sceVrTrackerGpuWait(WaitParam(param)) == ArgumentInvalid, "wait with a wrong size");
    }
    SetSize(param, sizeof(VrTrackerGpuWaitParam));
    Require(sceVrTrackerGpuWait(WaitParam(param)) == NotExecuteGpuSubmit, "wait without a submission");
    Buffer expected{};
    expected.fill(Fill);
    SetSize(expected, sizeof(VrTrackerGpuWaitParam));
    Require(param == expected, "wait leaves param");

    const auto* unreadable = reinterpret_cast<const VrTrackerGpuWaitParam*>(page.Guard());
    Require(sceVrTrackerGpuWait(unreadable) == ArgumentInvalid, "wait from an unreadable pointer");

    std::uint8_t* sizeOnly = page.Guard() - sizeof(std::uint32_t);
    const std::uint32_t size = sizeof(VrTrackerGpuWaitParam);
    std::memcpy(sizeOnly, &size, sizeof(size));
    Require(sceVrTrackerGpuWait(reinterpret_cast<const VrTrackerGpuWaitParam*>(sizeOnly)) == NotExecuteGpuSubmit,
            "wait reads only the size");
}

void CheckInitialized() {
    GuardedPage page;
    CheckTimeInitialized(page);
    CheckWaitInitialized(page);

    Buffer param{};
    param.fill(Fill);
    Require(Throws([] { sceVrTrackerGpuSubmit(nullptr); }), "submit null throws once initialized");
    Require(Throws([&] { sceVrTrackerGpuSubmit(SubmitParam(param)); }), "submit throws once initialized");
    Require(Untouched(param), "submit leaves param once initialized");
}

}

int main() {
    CheckNotInit();

    const auto init = ValidInit();
    Require(sceVrTrackerInit(reinterpret_cast<const VrTrackerInitParam*>(&init)) == 0, "init");
    CheckInitialized();

    Require(sceVrTrackerTerm() == 0, "term");
    CheckNotInit();
    return 0;
}
