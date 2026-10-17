#include "SceTypes.hpp"
#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <initializer_list>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <cstdio>
#include <cstring>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <unistd.h>
#endif

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI sceKernelIsStack(void* addr, void** start, void** end);
int APS5_VABI mprotect_nid_postfix(void* address, std::size_t length, int protection) noexcept;
}

static constexpr int SCE_OK = 0;
static constexpr std::uintptr_t Page = 0x4000;
static constexpr std::size_t GuardBytes = 0x10000;
static constexpr std::size_t HostBytes = 0x40000;

#define Require(value) do { if (!(value)) { std::fprintf(stderr, "GuestKernelStackProtect failed at line %d\n", __LINE__); std::abort(); } } while (false)

struct PageState {
    bool mapped;
    bool writable;
    std::uint64_t raw;
};

static PageState QueryPage(std::uintptr_t address) {
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) == sizeof(memory));
    const bool mapped = memory.State == MEM_COMMIT;
    return {mapped, mapped && (memory.Protect & (PAGE_READWRITE | PAGE_EXECUTE_READWRITE)) != 0, (static_cast<std::uint64_t>(memory.State) << 32) | memory.Protect};
#else
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    Require(maps != nullptr);
    PageState state{false, false, 0};
    char line[512];
    while (std::fgets(line, sizeof(line), maps) != nullptr) {
        unsigned long long low = 0;
        unsigned long long high = 0;
        char permissions[5] = {};
        if (std::sscanf(line, "%llx-%llx %4s", &low, &high, permissions) != 3 || address < low || address >= high) continue;
        state = {true, permissions[0] == 'r' && permissions[1] == 'w', 1ull | (permissions[0] == 'r' ? 2ull : 0) | (permissions[1] == 'w' ? 4ull : 0) | (permissions[2] == 'x' ? 8ull : 0)};
        break;
    }
    std::fclose(maps);
    return state;
#endif
}

static void* MapHostPages() {
#ifdef _WIN32
    return VirtualAlloc(nullptr, HostBytes, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    void* mapped = mmap(nullptr, HostBytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return mapped == MAP_FAILED ? nullptr : mapped;
#endif
}

static void UnmapHostPages(void* pages) {
#ifdef _WIN32
    Require(VirtualFree(pages, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(pages, HostBytes) == 0);
#endif
}

static void* APS5_VABI Worker(void* arg) {
    auto& passed = *static_cast<std::atomic<bool>*>(arg);
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = (reinterpret_cast<std::uintptr_t>(start) + Page - 1) & ~(Page - 1);
    Require(low + GuardBytes + Page < reinterpret_cast<std::uintptr_t>(&local));
#ifdef __linux__
    std::uintptr_t hostGuard = 0;
    for (auto cursor = low; cursor >= Page && low - cursor <= 4 * 1024 * 1024; cursor -= Page) {
        const auto page = QueryPage(cursor);
        if (!page.mapped) break;
        if (!page.writable) { hostGuard = cursor; break; }
    }
    Require(hostGuard != 0);
    const auto hostGuardBefore = QueryPage(hostGuard).raw;
    Require(mprotect_nid_postfix(reinterpret_cast<void*>(hostGuard), Page, 3) == -1);
    Require(QueryPage(hostGuard).raw == hostGuardBefore);
#endif
    auto* guard = reinterpret_cast<volatile unsigned char*>(low);
    Require(mprotect_nid_postfix(const_cast<unsigned char*>(guard), GuardBytes, 0) == 0);
#ifdef _WIN32
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(const_cast<unsigned char*>(guard), &memory, sizeof(memory)) == sizeof(memory));
    Require(memory.Protect == PAGE_NOACCESS && memory.RegionSize >= GuardBytes);
#endif
    Require(mprotect_nid_postfix(const_cast<unsigned char*>(guard), GuardBytes, 3) == 0);
    guard[0] = 1;
    guard[GuardBytes - 1] = 2;
    Require(guard[0] == 1 && guard[GuardBytes - 1] == 2);
    const auto below = reinterpret_cast<std::uintptr_t>(start) - 1;
    const auto belowBefore = QueryPage(below).raw;
    Require(mprotect_nid_postfix(start, GuardBytes, 0) == 0);
    Require(QueryPage(below).raw == belowBefore);
    Require(mprotect_nid_postfix(start, GuardBytes, 3) == 0);
    Require(QueryPage(below).raw == belowBefore);
    static_cast<volatile unsigned char*>(start)[0] = 3;
    const auto outside = reinterpret_cast<std::uintptr_t>(start) & ~(Page - 1);
    if (outside >= Page) Require(mprotect_nid_postfix(reinterpret_cast<void*>(outside - Page), 2 * Page, 3) == -1);
    if (outside >= 2 * Page) Require(mprotect_nid_postfix(reinterpret_cast<void*>(outside - 2 * Page), Page, 3) == -1);
    const auto stackBytes = reinterpret_cast<std::uintptr_t>(end) - reinterpret_cast<std::uintptr_t>(start);
    Require(mprotect_nid_postfix(start, stackBytes, 0xc2) == 0);
    Require(QueryPage(low).writable);
    static_cast<volatile unsigned char*>(start)[0] = 4;
    Require(mprotect_nid_postfix(start, stackBytes, 3) == 0);
    void* host = MapHostPages();
    Require(host != nullptr);
    Require(mprotect_nid_postfix(host, HostBytes, 1) == -1);
    Require(QueryPage(reinterpret_cast<std::uintptr_t>(host)).writable);
    static_cast<volatile unsigned char*>(host)[HostBytes - 1] = 5;
    UnmapHostPages(host);
    passed.store(true);
    return nullptr;
}

static std::atomic<std::uintptr_t> leftGuard{0};

static void* APS5_VABI LeaveGuard(void*) {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = (reinterpret_cast<std::uintptr_t>(start) + Page - 1) & ~(Page - 1);
    Require(mprotect_nid_postfix(reinterpret_cast<void*>(low), GuardBytes, 0) == 0);
    leftGuard.store(low);
    return nullptr;
}

static void* APS5_VABI WriteLeftGuard(void*) {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto guard = leftGuard.load();
    if (guard >= reinterpret_cast<std::uintptr_t>(start) && guard + GuardBytes <= reinterpret_cast<std::uintptr_t>(&local)) {
        reinterpret_cast<volatile unsigned char*>(guard)[0] = 4;
    }
    return nullptr;
}

#ifdef _WIN32
struct HostStackLayout {
    std::uintptr_t guard;
    std::uintptr_t committed;
};

static HostStackLayout QueryHostStack(std::uintptr_t start, std::uintptr_t end) {
    HostStackLayout layout{0, end};
    for (auto cursor = start; cursor < end;) {
        MEMORY_BASIC_INFORMATION memory{};
        Require(VirtualQuery(reinterpret_cast<const void*>(cursor), &memory, sizeof(memory)) == sizeof(memory));
        if (memory.State == MEM_COMMIT && (memory.Protect & PAGE_GUARD) != 0) layout.guard = cursor;
        else if (memory.State == MEM_COMMIT && layout.guard != 0 && layout.committed == end) layout.committed = cursor;
        cursor = std::min(end, reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize);
    }
    return layout;
}

static void GrowStack(std::uintptr_t below) {
    static void (*volatile const next)(std::uintptr_t) = GrowStack;
    volatile unsigned char frame[1024];
    frame[0] = 1;
    if (reinterpret_cast<std::uintptr_t>(&frame[0]) > below) next(below);
    frame[sizeof(frame) - 1] = 2;
}

static std::uintptr_t StackLimit() {
    return reinterpret_cast<std::uintptr_t>(reinterpret_cast<NT_TIB*>(NtCurrentTeb())->StackLimit);
}

static bool Readable(std::uintptr_t address) {
    unsigned char byte = 0;
    SIZE_T read = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(address), &byte, 1, &read) != 0 && read == 1;
}

static void RequireGrowableStack(std::uintptr_t start, std::uintptr_t end) {
    const auto before = QueryHostStack(start, end);
    Require(before.guard > start && before.guard < before.committed);
    Require(QueryPage(before.committed).writable);
    Require(StackLimit() == before.committed);
    GrowStack(before.guard - 0x2000);
    const auto after = QueryHostStack(start, end);
    Require(after.guard != 0 && after.guard < before.guard);
    Require(QueryPage(before.guard).writable);
    Require(StackLimit() == after.committed);
}
#endif

static void ProtectStack(std::uintptr_t skippedBytes) {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = reinterpret_cast<std::uintptr_t>(start) + skippedBytes;
    const auto high = reinterpret_cast<std::uintptr_t>(end);
    Require(mprotect_nid_postfix(reinterpret_cast<void*>(low), high - low, 0xc2) == 0);
#ifdef _WIN32
    RequireGrowableStack(reinterpret_cast<std::uintptr_t>(start), high);
    Require(mprotect_nid_postfix(start, Page, 0) == 0);
    RequireGrowableStack(reinterpret_cast<std::uintptr_t>(start), high);
#else
    Require(QueryPage(low).writable);
    reinterpret_cast<volatile unsigned char*>(low)[0] = 6;
#endif
}

#ifdef _WIN32
static DWORD WINAPI ProtectReservedStack(void* partial) {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = reinterpret_cast<std::uintptr_t>(start);
    const auto guard = QueryHostStack(low, reinterpret_cast<std::uintptr_t>(end)).guard;
    Require(guard - low >= 2 * Page);
    ProtectStack(partial != nullptr ? ((guard - low) / 2) & ~(Page - 1) : 0);
    return 0;
}

static DWORD WINAPI ProtectStackBottom(void*) {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = reinterpret_cast<std::uintptr_t>(start);
    const auto high = reinterpret_cast<std::uintptr_t>(end);
    const auto island = low + 3 * Page;
    Require(QueryHostStack(low, high).guard - low >= 4 * Page);
    Require(mprotect_nid_postfix(start, 3 * Page, 3) == 0);
    Require(QueryPage(low).writable && QueryPage(island - 1).writable);
    reinterpret_cast<volatile unsigned char*>(low)[0] = 7;
    Require(mprotect_nid_postfix(start, 3 * Page, 0) == 0);
    Require(QueryPage(low).mapped && !QueryPage(island - 1).writable);
    Require(!Readable(low) && !Readable(island - 1));
    RequireGrowableStack(low, high);
    const auto layout = QueryHostStack(low, high);
    const auto guardBytes = layout.committed - layout.guard;
    GrowStack(island + guardBytes + 0xf00);
    const auto aboveIsland = QueryHostStack(low, high);
    Require(aboveIsland.guard == island);
    Require(StackLimit() == aboveIsland.committed);
    Require(QueryPage(island - 1).mapped && !QueryPage(island - 1).writable && !Readable(island - 1));
    GrowStack(island + guardBytes - 0x100);
    const auto intoIsland = QueryHostStack(low, high);
    Require(intoIsland.guard == island - 0x1000);
    Require(StackLimit() == intoIsland.committed);
    Require(QueryPage(island - 1).raw == ((static_cast<std::uint64_t>(MEM_COMMIT) << 32) | (PAGE_READWRITE | PAGE_GUARD)));
    Require(QueryPage(island - 0x1001).mapped && !QueryPage(island - 0x1001).writable && !Readable(island - 0x1001) && !Readable(low));
    return 0;
}

static void RunOnReservedStack(LPTHREAD_START_ROUTINE routine, void* arg) {
    HANDLE thread = CreateThread(nullptr, HostBytes * 4, routine, arg, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    Require(thread != nullptr);
    Require(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    DWORD exitCode = 1;
    Require(GetExitCodeThread(thread, &exitCode) != 0 && exitCode == 0);
    Require(CloseHandle(thread) != 0);
}

static void ProtectHostStacks() {
    ProtectStack(0);
    for (void* partial : {static_cast<void*>(nullptr), reinterpret_cast<void*>(1)}) RunOnReservedStack(ProtectReservedStack, partial);
    RunOnReservedStack(ProtectStackBottom, nullptr);
}
#else
static int ProtectUnlimitedStack() {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = (reinterpret_cast<std::uintptr_t>(start) + Page - 1) & ~(Page - 1);
    const auto high = reinterpret_cast<std::uintptr_t>(end);
    if (high - low < (std::uintptr_t{1} << 40)) {
        std::puts("skipped the unlimited main stack check, the reported stack is smaller than 1 TiB");
        return 0;
    }
    alarm(5);
    Require(mprotect_nid_postfix(reinterpret_cast<void*>(low), high - low, 3) == 0);
    reinterpret_cast<volatile unsigned char*>(low)[0] = 9;
    return 0;
}

static int ProtectMainStackBottom(std::initializer_list<int> protections, std::uint64_t raw) {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = (reinterpret_cast<std::uintptr_t>(start) + Page - 1) & ~(Page - 1);
    if (QueryPage(low + GuardBytes + 0x100000).mapped) {
        std::puts("skipped the main stack bottom checks, the stack is mapped within 1 MiB of its bottom");
        return 0;
    }
    for (const int protection : protections) {
        Require(mprotect_nid_postfix(reinterpret_cast<void*>(low), GuardBytes, protection) == 0);
        void* startAfter = nullptr;
        void* endAfter = nullptr;
        Require(sceKernelIsStack(&local, &startAfter, &endAfter) == SCE_OK && startAfter == start && endAfter == end);
    }
    for (const auto offset : {GuardBytes, GuardBytes + 0x40000}) reinterpret_cast<volatile unsigned char*>(low + offset)[0] = 8;
    Require(QueryPage(low).raw == raw);
    return 0;
}

static bool OvercommitStrict() {
    std::FILE* setting = std::fopen("/proc/sys/vm/overcommit_memory", "r");
    if (setting == nullptr) return false;
    const bool strict = std::fgetc(setting) == '2';
    std::fclose(setting);
    return strict;
}

static bool ChildPasses(const char* mode, bool unlimitedStack) {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        rlimit limit{};
        if (unlimitedStack && getrlimit(RLIMIT_STACK, &limit) == 0) limit.rlim_cur = RLIM_INFINITY;
        if (!unlimitedStack || setrlimit(RLIMIT_STACK, &limit) == 0) execl("/proc/self/exe", "/proc/self/exe", mode, static_cast<char*>(nullptr));
        std::_Exit(127);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    return WIFEXITED(status) && WEXITSTATUS(status) == 0;
}

static void ProtectMainStacks() {
    for (const char* mode : {"--read-only-bottom", "--reprotect-bottom"}) Require(ChildPasses(mode, false));
    rlimit limit{};
    Require(getrlimit(RLIMIT_STACK, &limit) == 0);
    if (limit.rlim_max != RLIM_INFINITY || OvercommitStrict()) {
        std::puts("skipped the unlimited main stack check, the stack limit cannot be raised or overcommit is strict");
        return;
    }
    Require(ChildPasses("--unlimited-stack", true));
}
#endif

int main(int argc, char** argv) {
#ifdef _WIN32
    (void)argc;
    (void)argv;
    ProtectHostStacks();
#else
    if (argc > 1 && std::strcmp(argv[1], "--unlimited-stack") == 0) return ProtectUnlimitedStack();
    if (argc > 1 && std::strcmp(argv[1], "--read-only-bottom") == 0) return ProtectMainStackBottom({1}, 3);
    if (argc > 1 && std::strcmp(argv[1], "--reprotect-bottom") == 0) return ProtectMainStackBottom({0, 3}, 7);
    ProtectMainStacks();
    ProtectStack(0);
#endif
    std::atomic<bool> passed{false};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, &passed, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(passed.load());
    Require(scePthreadCreate(&thread, nullptr, LeaveGuard, nullptr, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    for (std::uintptr_t offset = 0; offset < GuardBytes; offset += Page) {
        const auto left = QueryPage(leftGuard.load() + offset);
#ifdef __linux__
        Require(!left.mapped || left.writable || left.raw == 1);
#else
        Require(!left.mapped || left.writable);
#endif
    }
    Require(scePthreadCreate(&thread, nullptr, WriteLeftGuard, nullptr, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
}
