#include "prx/libkernel/include/Api.hpp"
#include "prx/libkernel/Pthread/include/Pthread.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>

#ifdef ANYPS5_ENABLE_ASAN
static constexpr std::uintptr_t GuestArenaOffset = 0x0000100000000000ull;
#else
static constexpr std::uintptr_t GuestArenaOffset = 0;
#endif
static constexpr std::uintptr_t ConsoleMainStackTop = 0x00000007EEFFC000ull + GuestArenaOffset;
static constexpr std::uintptr_t SmallestStack = 0x100000;
static int marker = 0;
static bool expectRefusal = false;

#define Require(value) do { if (!(value)) { std::fprintf(stderr, "GuestMainStack failed at line %d\n", __LINE__); std::_Exit(1); } } while (false)

static char Permissions(std::uintptr_t address) {
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    Require(maps != nullptr);
    char result = 0;
    char line[512];
    while (std::fgets(line, sizeof(line), maps) != nullptr) {
        unsigned long long low = 0;
        unsigned long long high = 0;
        char permissions[5] = {};
        if (std::sscanf(line, "%llx-%llx %4s", &low, &high, permissions) != 3 || address < low || address >= high) continue;
        result = permissions[0] == 'r' && permissions[1] == 'w' ? 'w' : permissions[0] == 'r' ? 'r' : '-';
        break;
    }
    std::fclose(maps);
    return result;
}

struct WorkerContext {
    std::atomic<bool> queried{false};
    void* local = nullptr;
    void* start = nullptr;
    void* end = nullptr;
    int result = -1;
};

static void* APS5_VABI Worker(void* arg) {
    auto& context = *static_cast<WorkerContext*>(arg);
    int local = 0;
    context.local = &local;
    context.result = sceKernelIsStack(&local, &context.start, &context.end);
    context.queried.store(true);
    return nullptr;
}

static void APS5_VABI Entry(void* arguments) {
    Require(!expectRefusal);
    Require(arguments == &marker);
    int local = 0;
    const auto address = reinterpret_cast<std::uintptr_t>(&local);
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == 0);
    const auto low = reinterpret_cast<std::uintptr_t>(start);
    const auto high = reinterpret_cast<std::uintptr_t>(end);
    Require(high == ConsoleMainStackTop);
    Require(low <= address && address < high);
    Require(high - low >= SmallestStack);
    Require(Permissions(low) == 'w' && Permissions(high - 1) == 'w');
    Require(Permissions(low - 1) == '-');

    WorkerContext context;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, &context, nullptr) == 0);
    Require(scePthreadJoin(thread, nullptr) == 0);
    Require(context.queried.load() && context.result == 0);
    Require(reinterpret_cast<std::uintptr_t>(context.end) <= low || reinterpret_cast<std::uintptr_t>(context.start) >= high);
    std::exit(0);
}

static void RefusesAnOverlap() {
    constexpr std::size_t Bytes = 0x4000;
    constexpr std::uint32_t Pattern = 0xa55a1234u;
    void* sentinel = reinterpret_cast<void*>(ConsoleMainStackTop - 0x10000);
    Require(sceKernelMapFlexibleMemory(&sentinel, Bytes, 3, 0x10) == 0);
    Require(reinterpret_cast<std::uintptr_t>(sentinel) == ConsoleMainStackTop - 0x10000);
    auto* words = static_cast<std::uint32_t*>(sentinel);
    for (std::size_t i = 0; i < Bytes / 4; ++i) words[i] = Pattern;
    bool refused = false;
    expectRefusal = true;
    try {
        EnterGuestMainStack_nid_no_patch(&marker, Entry);
    } catch (const std::runtime_error& error) {
        refused = std::string(error.what()).find("overlaps a guest mapping") != std::string::npos;
    }
    expectRefusal = false;
    Require(refused);
    Require(Permissions(reinterpret_cast<std::uintptr_t>(sentinel)) == 'w');
    for (std::size_t i = 0; i < Bytes / 4; ++i) Require(words[i] == Pattern);
    Require(sceKernelMunmap(reinterpret_cast<std::uint64_t>(sentinel), Bytes) == 0);
}

int main() {
    RefusesAnOverlap();
    EnterGuestMainStack_nid_no_patch(&marker, Entry);
}
