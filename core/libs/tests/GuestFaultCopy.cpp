#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>
#ifdef __APPLE__
#include <mach/vm_page_size.h>
#include "prx/libc/include/GuestWriteWatch.hpp"
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "guest fault copy check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static void* const Unmapped = reinterpret_cast<void*>(0x10);
static void* const NonCanonical = reinterpret_cast<void*>(0x8000000000000000ull);

static volatile sig_atomic_t g_outside = 0;
static volatile sig_atomic_t g_seen = 0;
static volatile sig_atomic_t g_resetHandHits = 0;
static struct sigaction g_chained{};

static void Earlier(int, siginfo_t*, void*) {
    if (g_outside) _exit(42);
    _exit(3);
}

static void Later(int signal, siginfo_t* info, void* context) {
    ++g_seen;
    g_chained.sa_sigaction(signal, info, context);
}

static void OnResetHandSegv(int) {
    ++g_resetHandHits;
    if (g_resetHandHits >= 2) _exit(42);
}

static void DisableCoreDumps() {
    struct rlimit limit{};
    limit.rlim_cur = 0;
    limit.rlim_max = 0;
    setrlimit(RLIMIT_CORE, &limit);
}

static void RequireExit(pid_t child, int code) {
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    Require(WIFEXITED(status) && WEXITSTATUS(status) == code);
}

static void RequireEarlierHandlerKeepsOtherFaults() {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        struct sigaction action{};
        action.sa_sigaction = Earlier;
        action.sa_flags = SA_SIGINFO;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGSEGV, &action, nullptr) != 0) _exit(2);
        int value = 0;
        if (GuestCopyIn_nid_no_patch(&value, Unmapped, sizeof(value))) _exit(4);
        if (GuestCopyOut_nid_no_patch(Unmapped, &value, sizeof(value))) _exit(5);
        g_outside = 1;
        *static_cast<volatile int*>(Unmapped) = 1;
        _exit(6);
    }
    RequireExit(child, 42);
}

static void RequireSigIgnWithSigInfoChains() {
    std::fflush(nullptr);
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        DisableCoreDumps();
        alarm(10);
        if (unsetenv("APS5_GUEST_COPY_SYSCALL") != 0) _exit(2);
        struct sigaction action{};
        action.sa_handler = SIG_IGN;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_SIGINFO;
        if (sigaction(SIGSEGV, &action, nullptr) != 0) _exit(3);
        std::uint32_t source = 0x12345678u;
        std::uint32_t destination = 0u;
        if (!GuestCopyIn_nid_no_patch(&destination, &source, sizeof(destination)) || destination != source) _exit(4);
        if (GuestCopyIn_nid_no_patch(&destination, Unmapped, sizeof(destination))) _exit(5);
        if (raise(SIGSEGV) != 0) _exit(6);
        if (GuestCopyIn_nid_no_patch(&destination, Unmapped, sizeof(destination))) _exit(7);
        _exit(0);
    }
    RequireExit(child, 0);
}

#ifdef __APPLE__
static const int ResetHandUnreported = 9;

static bool ReportsResetHand(int signal) {
    struct sigaction reported{};
    return sigaction(signal, nullptr, &reported) == 0 && (reported.sa_flags & SA_RESETHAND) != 0;
}
#endif

static void RequireResetHandChainsToDefault() {
    std::fflush(nullptr);
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        DisableCoreDumps();
        alarm(10);
        if (unsetenv("APS5_GUEST_COPY_SYSCALL") != 0) _exit(2);
        g_resetHandHits = 0;
        struct sigaction action{};
        action.sa_handler = OnResetHandSegv;
        sigemptyset(&action.sa_mask);
        action.sa_flags = SA_RESETHAND;
        if (sigaction(SIGSEGV, &action, nullptr) != 0) _exit(3);
#ifdef __APPLE__
        if (!ReportsResetHand(SIGSEGV)) _exit(ResetHandUnreported);
#endif
        std::uint32_t source = 0x87654321u;
        std::uint32_t destination = 0u;
        if (!GuestCopyIn_nid_no_patch(&destination, &source, sizeof(destination)) || destination != source) _exit(4);
        if (raise(SIGSEGV) != 0) _exit(5);
        if (g_resetHandHits != 1) _exit(6);
        if (GuestCopyIn_nid_no_patch(&destination, Unmapped, sizeof(destination))) _exit(7);
        raise(SIGSEGV);
        _exit(8);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
#ifdef __APPLE__
    if (WIFEXITED(status) && WEXITSTATUS(status) == ResetHandUnreported) return;
#endif
    Require(WIFSIGNALED(status) && WTERMSIG(status) == SIGSEGV);
}

static void RequireLaterHandlerChains() {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        struct sigaction action{};
        action.sa_sigaction = Later;
        action.sa_flags = SA_SIGINFO | SA_NODEFER;
        sigemptyset(&action.sa_mask);
        if (sigaction(SIGSEGV, &action, &g_chained) != 0 || (g_chained.sa_flags & SA_SIGINFO) == 0) _exit(2);
        int value = 0;
        if (GuestCopyIn_nid_no_patch(&value, Unmapped, sizeof(value))) _exit(4);
        if (GuestCopyOut_nid_no_patch(Unmapped, &value, sizeof(value))) _exit(5);
        if (g_seen != 0 && g_seen != 2) _exit(7);
        _exit(0);
    }
    RequireExit(child, 0);
}

static void IgnoredSegvHandler(int) {}

static void RequireGuestRegistrationKeepsCopyFaults() {
    for (const int mode : {0, 1, 2, 3, 4, 5}) {
        const pid_t child = fork();
        Require(child >= 0);
        if (child == 0) {
            alarm(10);
            GuestSigaction action{};
            action.handler = mode == 2 || mode == 5 ? 1 : reinterpret_cast<std::uintptr_t>(&IgnoredSegvHandler);
            action.flags = mode == 1 ? 0x40 : mode == 4 ? 0x4 : 0;
            if (sigaction_nid_postfix(11, &action, nullptr) != 0) _exit(2);
            if ((mode == 4 || mode == 5) && raise(SIGSEGV) != 0) _exit(3);
            if (mode == 3) {
                const GuestSigaction reset{};
                if (sigaction_nid_postfix(11, &reset, nullptr) != 0) _exit(2);
            }
            int value = 0;
            if (GuestCopyIn_nid_no_patch(&value, Unmapped, sizeof(value))) _exit(4);
            if (GuestCopyOut_nid_no_patch(Unmapped, &value, sizeof(value))) _exit(5);
            _exit(0);
        }
        RequireExit(child, 0);
    }
}

static void RequireBusChainAfterGuestRegistration() {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        alarm(10);
        int value = 0;
        if (GuestCopyIn_nid_no_patch(&value, Unmapped, sizeof(value))) _exit(4);
        if (sceKernelInstallExceptionHandler(10, reinterpret_cast<void*>(&IgnoredSegvHandler)) != 0) _exit(2);
        GuestSigaction action{};
        action.handler = reinterpret_cast<std::uintptr_t>(&IgnoredSegvHandler);
        if (sigaction_nid_postfix(11, &action, nullptr) != 0) _exit(2);
        if (GuestCopyIn_nid_no_patch(&value, Unmapped, sizeof(value))) _exit(4);
        if (GuestCopyOut_nid_no_patch(Unmapped, &value, sizeof(value))) _exit(5);
        raise(SIGBUS);
        _exit(6);
    }
    int status = 0;
    Require(waitpid(child, &status, 0) == child);
    Require(WIFSIGNALED(status) && WTERMSIG(status) == SIGBUS);
}

static void RequireCopies() {
    for (const std::size_t bytes : {std::size_t{1}, std::size_t{2}, std::size_t{3}, std::size_t{4}, std::size_t{7}, std::size_t{8}, std::size_t{15}, std::size_t{16}, std::size_t{17}, std::size_t{31}, std::size_t{32}, std::size_t{33}, std::size_t{63}, std::size_t{4096}, std::size_t{4097}, std::size_t{65536}}) {
        for (std::size_t offset = 0; offset < 8; ++offset) {
            std::vector<unsigned char> source(bytes + offset), destination(bytes + offset, 0);
            for (std::size_t index = 0; index < source.size(); ++index) source[index] = static_cast<unsigned char>(index * 7 + bytes);
            Require(GuestCopyIn_nid_no_patch(destination.data() + offset, source.data() + offset, bytes));
            Require(std::memcmp(destination.data() + offset, source.data() + offset, bytes) == 0);
            std::fill(destination.begin(), destination.end(), 0);
            Require(GuestCopyOut_nid_no_patch(destination.data() + offset, source.data() + offset, bytes));
            Require(std::memcmp(destination.data() + offset, source.data() + offset, bytes) == 0);
        }
    }
}

static void RequireFaults() {
    const long page = sysconf(_SC_PAGESIZE);
    Require(page > 0);
    auto* pages = static_cast<unsigned char*>(mmap(nullptr, page * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED);
    std::memset(pages, 0x5a, page * 3);
    Require(mprotect(pages + page, page, PROT_NONE) == 0);
    Require(mprotect(pages + page * 2, page, PROT_READ) == 0);
    unsigned char buffer[64] = {};
    Require(GuestCopyIn_nid_no_patch(buffer, Unmapped, 0) && GuestCopyOut_nid_no_patch(Unmapped, buffer, 0));
    for (void* bad : {Unmapped, NonCanonical, static_cast<void*>(pages + page)}) {
        errno = 71;
        Require(!GuestCopyIn_nid_no_patch(buffer, bad, sizeof(buffer)));
        Require(errno == 71);
        Require(!GuestCopyOut_nid_no_patch(bad, buffer, sizeof(buffer)));
        Require(errno == 71);
    }
    errno = 71;
    Require(!GuestCopyIn_nid_no_patch(buffer, pages + page - 8, 16));
    Require(!GuestCopyOut_nid_no_patch(pages + page - 8, buffer, 16));
    Require(errno == 71);
    Require(GuestCopyIn_nid_no_patch(buffer, pages + page * 2, sizeof(buffer)));
    Require(buffer[0] == 0x5a && buffer[sizeof(buffer) - 1] == 0x5a);
    Require(!GuestCopyOut_nid_no_patch(pages + page * 2, buffer, sizeof(buffer)));
    Require(errno == 71);
    Require(munmap(pages, page * 3) == 0);
}

static void RequireThreads() {
    std::atomic<int> failures{0};
    std::vector<std::thread> threads;
    for (int thread = 0; thread < 4; ++thread) {
        threads.emplace_back([&failures, thread] {
            std::uint64_t source = 0x1122334455667788ull + thread;
            std::uint64_t destination = 0;
            for (int index = 0; index < 20000; ++index) {
                if (!GuestCopyIn_nid_no_patch(&destination, &source, sizeof(source)) || destination != source) ++failures;
                if (GuestCopyIn_nid_no_patch(&destination, Unmapped, sizeof(destination))) ++failures;
                if (GuestCopyOut_nid_no_patch(Unmapped, &source, sizeof(source))) ++failures;
            }
        });
    }
    for (auto& thread : threads) thread.join();
    Require(failures.load() == 0);
}

static void RequireShortFaultPrefixes() {
    const long page = sysconf(_SC_PAGESIZE);
    Require(page > 64);
    auto* pages = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED);
    std::memset(pages, 0x5a, page);
    Require(mprotect(pages + page, page, PROT_NONE) == 0);
    for (std::size_t bytes = 1; bytes <= 33; ++bytes) {
        unsigned char buffer[64];
        std::memset(buffer, 0xcc, sizeof(buffer));
        Require(GuestCopyIn_nid_no_patch(buffer, pages + page - bytes, bytes));
        for (std::size_t index = 0; index < bytes; ++index) Require(buffer[index] == 0x5a);
        Require(buffer[bytes] == 0xcc);
        Require(GuestCopyOut_nid_no_patch(pages + page - bytes, buffer, bytes));
        for (std::size_t prefix = 0; prefix < bytes; ++prefix) {
            std::memset(buffer, 0xcc, sizeof(buffer));
            errno = 71;
            Require(!GuestCopyIn_nid_no_patch(buffer, pages + page - prefix, bytes));
            Require(errno == 71);
            for (std::size_t index = 0; index < sizeof(buffer); ++index) Require(buffer[index] == (index < prefix ? 0x5a : 0xcc));
            for (std::size_t index = 0; index < bytes; ++index) buffer[index] = static_cast<unsigned char>(index * 13 + 1);
            Require(!GuestCopyOut_nid_no_patch(pages + page - prefix, buffer, bytes));
            Require(errno == 71);
            for (std::size_t index = 0; index < prefix; ++index) Require(pages[page - prefix + index] == buffer[index]);
            std::memset(pages + page - prefix, 0x5a, prefix);
        }
    }
    Require(munmap(pages, page * 2) == 0);
}

static void RequireLongFaultPrefixes() {
    const long page = sysconf(_SC_PAGESIZE);
    Require(page > 512);
    auto* pages = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED);
    std::memset(pages, 0x5a, page);
    Require(mprotect(pages + page, page, PROT_NONE) == 0);
    for (const std::size_t bytes : {std::size_t{48}, std::size_t{64}, std::size_t{65}, std::size_t{127}, std::size_t{128}, std::size_t{200}, std::size_t{257}}) {
        unsigned char buffer[512];
        std::memset(buffer, 0xcc, sizeof(buffer));
        Require(GuestCopyIn_nid_no_patch(buffer, pages + page - bytes, bytes));
        for (std::size_t index = 0; index < sizeof(buffer); ++index) Require(buffer[index] == (index < bytes ? 0x5a : 0xcc));
        for (std::size_t index = 0; index < bytes; ++index) buffer[index] = static_cast<unsigned char>(index * 13 + 1);
        Require(GuestCopyOut_nid_no_patch(pages + page - bytes, buffer, bytes));
        Require(std::memcmp(pages + page - bytes, buffer, bytes) == 0);
        std::memset(pages + page - bytes, 0x5a, bytes);
        for (const std::size_t prefix : {std::size_t{0}, std::size_t{1}, std::size_t{15}, std::size_t{16}, std::size_t{17}, std::size_t{47}, bytes - 17, bytes - 16, bytes - 1}) {
            std::memset(buffer, 0xcc, sizeof(buffer));
            errno = 71;
            Require(!GuestCopyIn_nid_no_patch(buffer, pages + page - prefix, bytes));
            Require(errno == 71);
            for (std::size_t index = 0; index < sizeof(buffer); ++index) Require(buffer[index] == (index < prefix ? 0x5a : 0xcc));
            for (std::size_t index = 0; index < bytes; ++index) buffer[index] = static_cast<unsigned char>(index * 13 + 1);
            Require(!GuestCopyOut_nid_no_patch(pages + page - prefix, buffer, bytes));
            Require(errno == 71);
            for (std::size_t index = 0; index < prefix; ++index) Require(pages[page - prefix + index] == buffer[index]);
            std::memset(pages + page - prefix, 0x5a, prefix);
        }
    }
    Require(munmap(pages, page * 2) == 0);
}

static void RequireShortBusFaults() {
#ifdef __APPLE__
    const long page = static_cast<long>(vm_kernel_page_size);
#else
    const long page = sysconf(_SC_PAGESIZE);
#endif
    Require(page > 0);
    FILE* file = std::tmpfile();
    Require(file != nullptr);
    Require(ftruncate(fileno(file), page) == 0);
    auto* pages = static_cast<unsigned char*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fileno(file), 0));
    Require(pages != MAP_FAILED);
    unsigned char buffer[33]{};
    for (std::size_t bytes = 1; bytes <= sizeof(buffer); ++bytes) {
        errno = 71;
        Require(!GuestCopyIn_nid_no_patch(buffer, pages + page, bytes));
        Require(errno == 71);
        Require(!GuestCopyOut_nid_no_patch(pages + page, buffer, bytes));
        Require(errno == 71);
    }
    Require(munmap(pages, page * 2) == 0);
    Require(std::fclose(file) == 0);
}

#ifdef __APPLE__
static std::atomic<int> g_trapResolves{0};

static void CountTrapResolve(std::uintptr_t, std::uintptr_t) {
    ++g_trapResolves;
}

static void RequireTrappedPagesFailCopies() {
    const pid_t child = fork();
    Require(child >= 0);
    if (child == 0) {
        alarm(10);
        const long page = sysconf(_SC_PAGESIZE);
        auto* pages = static_cast<unsigned char*>(mmap(nullptr, page, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        if (pages == MAP_FAILED) _exit(2);
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pages, page);
        if (!GuestWriteWatch::GuestWriteWatchSetReadTrapResolver_nid_postfix(&CountTrapResolve)) _exit(3);
        if (!GuestWriteWatch::GuestWriteWatchTrapReads_nid_postfix(reinterpret_cast<std::uintptr_t>(pages), page)) _exit(4);
        int value = 0;
        if (GuestCopyIn_nid_no_patch(&value, pages, sizeof(value))) _exit(5);
        if (GuestCopyInUnsynced_nid_no_patch(&value, pages, sizeof(value))) _exit(6);
        if (g_trapResolves.load() != 0) _exit(7);
        _exit(0);
    }
    RequireExit(child, 0);
}
#endif

int main() {
    RequireEarlierHandlerKeepsOtherFaults();
    RequireSigIgnWithSigInfoChains();
    RequireResetHandChainsToDefault();
    RequireCopies();
    RequireFaults();
    RequireShortFaultPrefixes();
    RequireLongFaultPrefixes();
    RequireShortBusFaults();
    RequireThreads();
    RequireLaterHandlerChains();
    RequireGuestRegistrationKeepsCopyFaults();
    RequireBusChainAfterGuestRegistration();
#ifdef __APPLE__
    RequireTrappedPagesFailCopies();
#endif
    std::puts("guest fault copy tests passed");
    return 0;
}
