#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "Testing.hpp"

#include <atomic>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <sys/mman.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>
#include <vector>

static void* const Unmapped = reinterpret_cast<void*>(0x10);
static void* const NonCanonical = reinterpret_cast<void*>(0x8000000000000000ull);

static volatile sig_atomic_t g_outside = 0;
static volatile sig_atomic_t g_seen = 0;
static struct sigaction g_chained{};

static void Earlier(int, siginfo_t*, void*) {
    if (g_outside) _exit(42);
    _exit(3);
}

static void Later(int signal, siginfo_t* info, void* context) {
    ++g_seen;
    g_chained.sa_sigaction(signal, info, context);
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
    for (const std::size_t bytes : {std::size_t{1}, std::size_t{7}, std::size_t{8}, std::size_t{63}, std::size_t{4096}, std::size_t{4097}, std::size_t{65536}}) {
        for (std::size_t offset = 0; offset < 4; ++offset) {
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

int main() {
    RequireEarlierHandlerKeepsOtherFaults();
    RequireCopies();
    RequireFaults();
    RequireThreads();
    RequireLaterHandlerChains();
    RequireGuestRegistrationKeepsCopyFaults();
    RequireBusChainAfterGuestRegistration();
    std::puts("guest fault copy tests passed");
    return 0;
}
