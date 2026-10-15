#include "SceTypes.hpp"
#include <atomic>
#include <cstdio>
#include <cstdint>
#include <cstdlib>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

extern "C" {
int APS5_VABI scePthreadCreate(Pthread* thread, const PthreadAttr* attr, PthreadEntry entry, void* arg, const char* name);
int APS5_VABI scePthreadJoin(Pthread thread, void** retval);
int APS5_VABI scePthreadKeyCreate(PthreadKey* key, pthread_key_destructor_func_t destructor);
int APS5_VABI scePthreadKeyDelete(PthreadKey key);
int APS5_VABI scePthreadSetspecific(PthreadKey key, void* value);
int APS5_VABI sceKernelIsStack(void* addr, void** start, void** end);
void* APS5_VABI mmap_nid_postfix(void* address, std::size_t length, int protection, int flags, int descriptor, std::int64_t offset) noexcept;
int APS5_VABI mprotect_nid_postfix(void* address, std::size_t length, int protection) noexcept;
int* APS5_VABI __error_nid_postfix();
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

#ifdef _WIN32
static void CheckFixedStackRemap(std::uintptr_t low, void* start) {
    constexpr int FixedPrivateAnonymous = 0x1012;
    auto* const before = reinterpret_cast<volatile unsigned char*>(low);
    auto* const page = reinterpret_cast<volatile unsigned char*>(low + Page);
    auto* const after = reinterpret_cast<volatile unsigned char*>(low + 2 * Page);
    auto* const address = const_cast<unsigned char*>(page);
    for (std::size_t index = 0; index < Page; ++index) {
        before[index] = 0x35;
        page[index] = 0xa7;
        after[index] = 0x59;
    }
    const auto checkBytes = [&](unsigned char expected) {
        for (std::size_t index = 0; index < Page; ++index) {
            Require(before[index] == 0x35);
            Require(page[index] == expected);
            Require(after[index] == 0x59);
        }
    };
    const auto reject = [&](void* candidate, std::size_t bytes, int flags, int descriptor, std::int64_t offset, int error) {
        *__error_nid_postfix() = 0;
        Require(mmap_nid_postfix(candidate, bytes, 0, flags, descriptor, offset) == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
        if (error != 0) Require(*__error_nid_postfix() == error);
        checkBytes(0xa7);
    };
    reject(address + 1, Page, FixedPrivateAnonymous, -1, 0, 22);
    reject(address, Page, FixedPrivateAnonymous, 0, 0, 22);
    reject(address, Page, FixedPrivateAnonymous, -1, Page, 22);
    reject(address, Page, FixedPrivateAnonymous | 0x4000, -1, 0, 0);
    int active = 0;
    const auto activePage = reinterpret_cast<std::uintptr_t>(&active) & ~(Page - 1);
    void* activeStart = nullptr;
    void* activeEnd = nullptr;
    Require(sceKernelIsStack(&active, &activeStart, &activeEnd) == SCE_OK);
    Require(activePage >= reinterpret_cast<std::uintptr_t>(activeStart));
    Require(activePage + Page <= reinterpret_cast<std::uintptr_t>(activeEnd));
    const auto activeBefore = QueryPage(activePage).raw;
    const auto activeEndBefore = QueryPage(activePage + Page - 1).raw;
    reject(reinterpret_cast<void*>(activePage), Page, FixedPrivateAnonymous, -1, 0, 22);
    Require(QueryPage(activePage).raw == activeBefore);
    Require(QueryPage(activePage + Page - 1).raw == activeEndBefore);
    const auto boundary = reinterpret_cast<std::uintptr_t>(start) & ~(Page - 1);
    Require(boundary >= Page);
    const auto outside = boundary - Page;
    const auto outsideBefore = QueryPage(outside).raw;
    reject(reinterpret_cast<void*>(outside), 2 * Page, FixedPrivateAnonymous, -1, 0, 22);
    Require(QueryPage(outside).raw == outsideBefore);

    void* const host = MapHostPages();
    Require(host != nullptr);
    auto* const hostBytes = static_cast<volatile unsigned char*>(host);
    hostBytes[0] = 0x63;
    hostBytes[HostBytes - 1] = 0x94;
    const auto hostBefore = QueryPage(reinterpret_cast<std::uintptr_t>(host)).raw;
    reject(host, Page, FixedPrivateAnonymous, -1, 0, 22);
    Require(QueryPage(reinterpret_cast<std::uintptr_t>(host)).raw == hostBefore);
    Require(hostBytes[0] == 0x63 && hostBytes[HostBytes - 1] == 0x94);
    UnmapHostPages(host);

    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const auto hostPage = static_cast<std::size_t>(system.dwPageSize);
    Require(hostPage != 0 && 2 * hostPage <= Page);
    auto* const guarded = address + hostPage;
    DWORD originalProtection = 0;
    Require(VirtualProtect(guarded, hostPage, PAGE_READWRITE | PAGE_GUARD, &originalProtection) != 0);
    Require(originalProtection == PAGE_READWRITE);
    *__error_nid_postfix() = 0;
    Require(mmap_nid_postfix(address, Page, 0, FixedPrivateAnonymous, -1, 0) == reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1)));
    Require(*__error_nid_postfix() == 22);
    MEMORY_BASIC_INFORMATION guardedMemory{};
    Require(VirtualQuery(guarded, &guardedMemory, sizeof(guardedMemory)) == sizeof(guardedMemory));
    Require(guardedMemory.State == MEM_COMMIT && guardedMemory.Protect == (PAGE_READWRITE | PAGE_GUARD));
    DWORD replacedProtection = 0;
    Require(VirtualProtect(guarded, hostPage, originalProtection, &replacedProtection) != 0);
    Require(replacedProtection == (PAGE_READWRITE | PAGE_GUARD));
    checkBytes(0xa7);

    Require(mmap_nid_postfix(address, Page, 0, FixedPrivateAnonymous, -1, 0) == address);
    MEMORY_BASIC_INFORMATION memory{};
    Require(VirtualQuery(address, &memory, sizeof(memory)) == sizeof(memory));
    Require(memory.State == MEM_COMMIT && memory.Protect == PAGE_NOACCESS);
    Require(reinterpret_cast<std::uintptr_t>(memory.BaseAddress) + memory.RegionSize >= low + 2 * Page);
    Require(mprotect_nid_postfix(address, Page, 3) == 0);
    checkBytes(0);
    for (std::size_t index = 0; index < Page; ++index) page[index] = 0xc8;
    Require(mmap_nid_postfix(address, Page, 3, FixedPrivateAnonymous, -1, 0) == address);
    Require(QueryPage(low + Page).writable);
    checkBytes(0);
    page[0] = 0x76;
    page[Page - 1] = 0x82;
    Require(page[0] == 0x76 && page[Page - 1] == 0x82);
}

struct PeerStack {
    std::atomic<int> phase{0};
    void* start = nullptr;
    std::uintptr_t low = 0;
};

static void* APS5_VABI WaitForPeerRemap(void* arg) {
    auto& context = *static_cast<PeerStack*>(arg);
    int local = 0;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &context.start, &end) == SCE_OK);
    context.low = (reinterpret_cast<std::uintptr_t>(context.start) + Page - 1) & ~(Page - 1);
    Require(context.low + GuardBytes + Page < reinterpret_cast<std::uintptr_t>(&local));
    Require(mprotect_nid_postfix(reinterpret_cast<void*>(context.low), GuardBytes, 3) == 0);
    context.phase.store(1);
    context.phase.notify_one();
    context.phase.wait(1);
    Require(context.phase.load() == 2);
    return nullptr;
}

static void CheckPeerStackRemap() {
    PeerStack context;
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, WaitForPeerRemap, &context, nullptr) == SCE_OK);
    context.phase.wait(0);
    Require(context.phase.load() == 1);
    CheckFixedStackRemap(context.low, context.start);
    context.phase.store(2);
    context.phase.notify_one();
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
}
#endif

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
#ifdef _WIN32
    CheckFixedStackRemap(low, start);
#endif
    void* host = MapHostPages();
    Require(host != nullptr);
    Require(mprotect_nid_postfix(host, HostBytes, 1) == -1);
    Require(QueryPage(reinterpret_cast<std::uintptr_t>(host)).writable);
    static_cast<volatile unsigned char*>(host)[HostBytes - 1] = 5;
    UnmapHostPages(host);
    passed.store(true);
    return nullptr;
}

enum class GuardMode { Protect, Remap, Split };

struct GuardContext {
    GuardMode mode;
    PthreadKey key = -1;
    std::uintptr_t low = 0;
    std::atomic<unsigned> calls{0};
};

static void APS5_VABI CheckRestoredGuard(void* arg) {
    auto& context = *static_cast<GuardContext*>(arg);
    Require(context.low != 0);
    auto* const bytes = reinterpret_cast<volatile unsigned char*>(context.low);
    for (std::uintptr_t offset = 0; offset < GuardBytes; offset += Page) {
        Require(QueryPage(context.low + offset).writable);
        bytes[offset] = 4;
        bytes[offset + Page - 1] = 5;
        Require(bytes[offset] == 4 && bytes[offset + Page - 1] == 5);
    }
    context.calls.fetch_add(1);
}

static void* APS5_VABI LeaveGuard(void* arg) {
    auto& context = *static_cast<GuardContext*>(arg);
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto low = (reinterpret_cast<std::uintptr_t>(start) + Page - 1) & ~(Page - 1);
    Require(low + GuardBytes + Page < reinterpret_cast<std::uintptr_t>(&local));
    context.low = low;
    Require(scePthreadSetspecific(context.key, &context) == SCE_OK);
    const auto mode = context.mode;
    if (mode == GuardMode::Remap) {
        Require(mmap_nid_postfix(reinterpret_cast<void*>(low), Page, 0, 0x1012, -1, 0) == reinterpret_cast<void*>(low));
    } else {
        Require(mprotect_nid_postfix(reinterpret_cast<void*>(low), GuardBytes, 0) == 0);
        if (mode == GuardMode::Split) {
            const auto leftBefore = QueryPage(low).raw;
            const auto rightBefore = QueryPage(low + 2 * Page).raw;
            const auto rightEndBefore = QueryPage(low + 3 * Page).raw;
            auto* const middle = reinterpret_cast<void*>(low + Page);
            Require(mmap_nid_postfix(middle, Page, 3, 0x1012, -1, 0) == middle);
            Require(QueryPage(low + Page).writable);
            Require(QueryPage(low).raw == leftBefore);
            Require(QueryPage(low + 2 * Page).raw == rightBefore);
            Require(QueryPage(low + 3 * Page).raw == rightEndBefore);
        }
    }
    return nullptr;
}

static void CheckGuardCleanup(GuardMode mode) {
    GuardContext context{mode};
    Require(scePthreadKeyCreate(&context.key, reinterpret_cast<pthread_key_destructor_func_t>(CheckRestoredGuard)) == SCE_OK);
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, LeaveGuard, &context, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(context.calls.load() == 1);
    Require(scePthreadKeyDelete(context.key) == SCE_OK);
}

#ifdef _WIN32
static void ProtectWholeStack() {
    int local = 0;
    void* start = nullptr;
    void* end = nullptr;
    Require(sceKernelIsStack(&local, &start, &end) == SCE_OK);
    const auto stackBytes = reinterpret_cast<std::uintptr_t>(end) - reinterpret_cast<std::uintptr_t>(start);
    Require(mprotect_nid_postfix(start, stackBytes, 0xc2) == 0);
    Require(QueryPage(reinterpret_cast<std::uintptr_t>(start)).writable);
    static_cast<volatile unsigned char*>(start)[0] = 6;
}

static DWORD WINAPI ProtectReservedStack(void*) {
    ProtectWholeStack();
    return 0;
}

static void ProtectHostStacks() {
    ProtectWholeStack();
    HANDLE thread = CreateThread(nullptr, HostBytes * 4, ProtectReservedStack, nullptr, STACK_SIZE_PARAM_IS_A_RESERVATION, nullptr);
    Require(thread != nullptr);
    Require(WaitForSingleObject(thread, INFINITE) == WAIT_OBJECT_0);
    DWORD exitCode = 1;
    Require(GetExitCodeThread(thread, &exitCode) != 0 && exitCode == 0);
    Require(CloseHandle(thread) != 0);
}
#endif

int main() {
#ifdef _WIN32
    ProtectHostStacks();
#endif
    std::atomic<bool> passed{false};
    Pthread thread = nullptr;
    Require(scePthreadCreate(&thread, nullptr, Worker, &passed, nullptr) == SCE_OK);
    Require(scePthreadJoin(thread, nullptr) == SCE_OK);
    Require(passed.load());
    CheckGuardCleanup(GuardMode::Protect);
#ifdef _WIN32
    CheckPeerStackRemap();
    CheckGuardCleanup(GuardMode::Remap);
    CheckGuardCleanup(GuardMode::Split);
#endif
}
