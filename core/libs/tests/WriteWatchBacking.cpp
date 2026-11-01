#include "prx/libc/include/GuestWriteWatch.hpp"

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <stdexcept>
#include <string_view>
#include <thread>

#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <unistd.h>

using namespace GuestWriteWatch;

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Mapping {
    Mapping(int flags, int protection) : protection(protection) {
        char path[] = "/tmp/anyps5-watch-backing-XXXXXX";
        file = ::mkstemp(path);
        Require(file >= 0, "cannot create backing file");
        ::unlink(path);
        Require(::ftruncate(file, 2 * page) == 0, "cannot size backing file");
        base = static_cast<std::uint8_t*>(::mmap(nullptr, 2 * page, PROT_READ | PROT_WRITE, flags, file, 0));
        Require(base != MAP_FAILED, "cannot map backing file");
        Require(::mprotect(base, 2 * page, protection) == 0, "cannot set mapping protection");
    }
    ~Mapping() {
        GuestWriteWatchUnregister_nid_postfix(base, 2 * page);
        ::munmap(base, 2 * page);
        ::close(file);
    }
    std::set<std::uintptr_t> Collect() {
        std::set<std::uintptr_t> pages;
        Require(GuestWriteWatchCollect_nid_postfix(reinterpret_cast<std::uintptr_t>(base), 2 * page, [](void* context, std::uintptr_t begin, std::uintptr_t end) {
            auto& output = *static_cast<std::set<std::uintptr_t>*>(context);
            const auto page = static_cast<std::size_t>(::getpagesize());
            for (auto at = begin; at < end; at += page) output.insert(at);
        }, &pages), "cannot collect watched mapping");
        return pages;
    }
    void Watch(bool armed, bool hostWrite) {
        GuestWriteWatchRegister_nid_postfix(base, 2 * page);
        Require(GuestWriteWatchProtect_nid_postfix(base, 2 * page, protection) == 0, "cannot record mapping protection");
        if (armed) Require(Collect().size() == 2, "fresh mapping was not reported");
        if (hostWrite) GuestWriteWatchBeginHostWrite_nid_postfix(base + page, page);
    }
    const std::size_t page = static_cast<std::size_t>(::getpagesize());
    const int protection;
    int file = -1;
    std::uint8_t* base = nullptr;
};

template <typename TFunction>
int RunChild(TFunction function) {
    const auto child = ::fork();
    Require(child >= 0, "cannot fork fault test");
    if (child == 0) {
        ::alarm(2);
        try {
            function();
        } catch (...) {
            ::_exit(2);
        }
        ::_exit(0);
    }
    int status = 0;
    Require(::waitpid(child, &status, 0) == child, "cannot wait for fault test");
    return status;
}

template <typename TFunction>
void RequireFault(TFunction function) {
    const int status = RunChild(function);
    Require(WIFSIGNALED(status) && (WTERMSIG(status) == SIGBUS || WTERMSIG(status) == SIGSEGV), "unrecoverable write did not deliver its fault");
}

void CheckTruncation(int flags) {
    for (const int protection : {PROT_READ | PROT_WRITE, PROT_WRITE}) {
        for (const bool armed : {false, true}) {
            for (const bool hostWrite : {false, true}) {
                for (const bool keepFirst : {false, true}) {
                    const auto write = [&](bool watched) {
                        Mapping mapping(flags, protection);
                        if (watched) mapping.Watch(armed, hostWrite);
                        Require(::ftruncate(mapping.file, keepFirst ? mapping.page : 0) == 0, "cannot truncate backing file");
                        *static_cast<volatile std::uint8_t*>(mapping.base + mapping.page + 17) = 42;
                        if (watched) {
                            if (hostWrite) GuestWriteWatchEndHostWrite_nid_postfix(mapping.base + mapping.page, mapping.page);
                            Require(mapping.Collect().contains(reinterpret_cast<std::uintptr_t>(mapping.base + mapping.page)), "surviving private write was not tracked");
                        }
                    };
                    const int native = RunChild([&] { write(false); });
                    Require((WIFEXITED(native) && WEXITSTATUS(native) == 0) || (WIFSIGNALED(native) && (WTERMSIG(native) == SIGBUS || WTERMSIG(native) == SIGSEGV)), "native truncation control failed");
                    const int watched = RunChild([&] { write(true); });
                    if (watched != native) std::fprintf(stderr, "flags=%d protection=%d armed=%d host=%d keepFirst=%d native=%d watched=%d\n", flags, protection, armed, hostWrite, keepFirst, native, watched);
                    Require(watched == native, "write watch changed the native truncation result");
                }
            }
        }
    }
}

void CheckWritable() {
    for (const int flags : {MAP_SHARED, MAP_PRIVATE}) {
        for (const int protection : {PROT_READ | PROT_WRITE, PROT_WRITE}) {
            for (const bool armed : {false, true}) {
                for (const bool hostWrite : {false, true}) {
                    Mapping mapping(flags, protection);
                    mapping.Watch(armed, hostWrite);
                    mapping.base[mapping.page + 17] = 42;
                    if (hostWrite) GuestWriteWatchEndHostWrite_nid_postfix(mapping.base + mapping.page, mapping.page);
                    const auto pages = mapping.Collect();
                    Require(pages.size() == (armed ? 1u : 2u) && pages.contains(reinterpret_cast<std::uintptr_t>(mapping.base + mapping.page)), "valid write was not reported");
                    Require(mapping.Collect().empty(), "valid write was reported twice");
                    GuestWriteWatchUnregister_nid_postfix(mapping.base, 2 * mapping.page);
                    Require(mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(mapping.base), 2 * mapping.page, false, VM_PROT_READ | VM_PROT_WRITE) == KERN_SUCCESS, "cannot read back valid write");
                    Require(mapping.base[mapping.page + 17] == 42, "valid write was lost");
                }
            }
        }
    }
}

std::atomic<unsigned> arrived{0};
std::atomic<bool> firstHandled{false};
struct sigaction previousBus{};
struct sigaction previousSegv{};

static_assert(std::atomic<unsigned>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);

void DelaySecondFault(int signal, siginfo_t* info, void* context) {
    const auto order = arrived.fetch_add(1);
    if (order >= 2) ::_exit(4);
    while (arrived.load() < 2) {}
    if (order == 1) while (!firstHandled.load()) {}
    (signal == SIGBUS ? previousBus : previousSegv).sa_sigaction(signal, info, context);
    if (order == 0) firstHandled.store(true);
}

void CheckLateFault() {
    ::alarm(10);
    struct sigaction delayed{};
    delayed.sa_sigaction = &DelaySecondFault;
    delayed.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&delayed.sa_mask);
    Require(::sigaction(SIGBUS, &delayed, &previousBus) == 0, "cannot delay SIGBUS");
    Require(::sigaction(SIGSEGV, &delayed, &previousSegv) == 0, "cannot delay SIGSEGV");
    for (const int flags : {MAP_SHARED, MAP_PRIVATE}) {
        for (const int protection : {PROT_READ | PROT_WRITE, PROT_WRITE}) {
            Mapping mapping(flags, protection);
            mapping.Watch(true, false);
            for (unsigned round = 1; round <= 8; ++round) {
                arrived.store(0);
                firstHandled.store(false);
                std::thread first([&] { mapping.base[mapping.page + 32] = static_cast<std::uint8_t>(round); });
                std::thread second([&] { mapping.base[mapping.page + 64] = static_cast<std::uint8_t>(round); });
                first.join();
                second.join();
                Require(arrived.load() == 2 && firstHandled.load(), "two simultaneous faults were not exercised");
                Require(mapping.Collect() == std::set<std::uintptr_t>{reinterpret_cast<std::uintptr_t>(mapping.base + mapping.page)}, "late write fault was not tracked");
                Require(mapping.Collect().empty(), "late write fault was tracked twice");
            }
            GuestWriteWatchUnregister_nid_postfix(mapping.base, 2 * mapping.page);
            Require(mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(mapping.base), 2 * mapping.page, false, VM_PROT_READ | VM_PROT_WRITE) == KERN_SUCCESS, "cannot read back concurrent writes");
            Require(mapping.base[mapping.page + 32] == 8 && mapping.base[mapping.page + 64] == 8, "concurrent write was lost");
        }
    }
    Require(::sigaction(SIGBUS, &previousBus, nullptr) == 0 && ::sigaction(SIGSEGV, &previousSegv, nullptr) == 0, "cannot restore fault handlers");
    ::alarm(0);
}

void CheckRestoreFailure() {
    RequireFault([] {
        Mapping mapping(MAP_SHARED, PROT_READ | PROT_WRITE);
        mapping.Watch(true, false);
        Require(mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(mapping.base), 2 * mapping.page, true, VM_PROT_READ) == KERN_SUCCESS, "cannot lower maximum protection");
        *static_cast<volatile std::uint8_t*>(mapping.base + mapping.page + 17) = 42;
    });
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2 && GuestWriteWatchAvailable_nid_postfix(), "expected a mode and available write watch");
        const std::string_view mode(argv[1]);
        if (mode == "shared") CheckTruncation(MAP_SHARED);
        else if (mode == "private") CheckTruncation(MAP_PRIVATE);
        else if (mode == "writable") CheckWritable();
        else if (mode == "late") CheckLateFault();
        else if (mode == "restore") CheckRestoreFailure();
        else throw std::runtime_error("unknown write-watch backing test");
        std::printf("write watch backing %s passed\n", argv[1]);
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
