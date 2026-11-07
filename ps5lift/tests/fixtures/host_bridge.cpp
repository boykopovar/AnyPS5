#include <ps5lift/Bridge.hpp>
#include <ps5lift/State.h>

#include <pthread.h>

#include <link.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

extern "C" void ps5lift_import_host__write(Ps5LiftState* state) {
    const auto* text = reinterpret_cast<const char*>(state->Gpr[PS5LIFT_RDI]);
    const auto length = static_cast<std::size_t>(state->Gpr[PS5LIFT_RSI]);
    state->Gpr[PS5LIFT_RAX] = static_cast<std::uint64_t>(write(1, text, length));
}

extern "C" void ps5lift_import_host__exit(Ps5LiftState* state) {
    std::fflush(nullptr);
    std::_Exit(static_cast<int>(state->Gpr[PS5LIFT_RDI]));
}

extern "C" void ps5lift_import_host__ticks(Ps5LiftState* state) {
    timespec time{};
    clock_gettime(CLOCK_MONOTONIC, &time);
    state->Gpr[PS5LIFT_RAX] = static_cast<std::uint64_t>(time.tv_sec) * 1000000000u + static_cast<std::uint64_t>(time.tv_nsec);
}

using namespace Ps5Lift::Bridge;

namespace {

struct HostThread {
    pthread_t Handle;
    std::uint64_t Entry;
    std::uint64_t Argument;
    std::uint64_t Result;
};

void* RunHostThread(void* opaque) {
    auto& thread = *static_cast<HostThread*>(opaque);
    thread.Result = Callback<std::uint64_t(std::uint64_t)>(thread.Entry)(thread.Argument);
    return nullptr;
}

void Require(const int error, const char* what) {
    if (error == 0) return;
    std::fprintf(stderr, "host_bridge: cannot %s a thread: %s\n", what, std::strerror(error));
    std::abort();
}

}

extern "C" void ps5lift_import_host__thread__create(Ps5LiftState* state) {
    auto* thread = new HostThread{{}, state->Gpr[PS5LIFT_RDI], state->Gpr[PS5LIFT_RSI], 0};
    Require(pthread_create(&thread->Handle, nullptr, RunHostThread, thread), "start");
    state->Gpr[PS5LIFT_RAX] = reinterpret_cast<std::uint64_t>(thread);
}

extern "C" void ps5lift_import_host__thread__join(Ps5LiftState* state) {
    auto* thread = reinterpret_cast<HostThread*>(state->Gpr[PS5LIFT_RDI]);
    Require(pthread_join(thread->Handle, nullptr), "join");
    state->Gpr[PS5LIFT_RAX] = thread->Result;
    delete thread;
}

extern "C" void ps5lift_import_host__sort(Ps5LiftState* state) {
    auto* compare = Callback<int(const void*, const void*)>(Integer<std::uint64_t>(state, 3));
    std::qsort(Integer<void*>(state, 0), Integer<unsigned long>(state, 1), Integer<unsigned long>(state, 2), compare);
}

extern "C" void ps5lift_import_host__apply(Ps5LiftState* state) {
    auto* function = Callback<double(double, long)>(Integer<std::uint64_t>(state, 0));
    const auto count = Integer<long>(state, 1);
    Return(state, function(function(Floating<double>(state, 0), count), count + 1));
}

extern "C" void ps5lift_import_host__on__thread(Ps5LiftState* state) {
    struct Call {
        long (*Function)(long);
        long Argument;
        long Result;
    } call{Callback<long(long)>(Integer<std::uint64_t>(state, 0)), Integer<long>(state, 1), 0};
    pthread_t thread;
    pthread_create(&thread, nullptr, [](void* argument) -> void* {
        auto* call = static_cast<Call*>(argument);
        call->Result = call->Function(call->Argument);
        return nullptr;
    }, &call);
    pthread_join(thread, nullptr);
    Return(state, call.Result);
}

namespace {

std::uint64_t HostFloatingPointControl() {
#if defined(__aarch64__)
    std::uint64_t fpcr = 0;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    return fpcr;
#else
    return __builtin_ia32_stmxcsr() & ~0x3fu;
#endif
}

void ResetHostFloatingPointControl() {
#if defined(__aarch64__)
    __asm__ volatile("msr fpcr, %0" : : "r"(std::uint64_t{0}));
#else
    __builtin_ia32_ldmxcsr(0x1f80);
#endif
}

struct ThreadCall {
    long (*Function)(long);
    long Argument;
    long Result;
};

bool GuardedBelow(const std::uint64_t address) {
    std::FILE* maps = std::fopen("/proc/self/maps", "r");
    if (maps == nullptr) std::abort();
    char text[512];
    unsigned long previousEnd = 0;
    bool previousGuard = false;
    bool guarded = false;
    while (std::fgets(text, sizeof(text), maps) != nullptr) {
        unsigned long start = 0;
        unsigned long end = 0;
        char permissions[5] = {};
        if (std::sscanf(text, "%lx-%lx %4s", &start, &end, permissions) != 3) std::abort();
        if (address >= start && address < end) {
            guarded = previousGuard && previousEnd == start;
            break;
        }
        previousEnd = end;
        previousGuard = std::strcmp(permissions, "---p") == 0;
    }
    std::fclose(maps);
    return guarded;
}

}

extern "C" void ps5lift_import_host__stack__guarded(Ps5LiftState* state) {
    Return(state, static_cast<long>(GuardedBelow(state->Gpr[PS5LIFT_RSP])));
}

extern "C" void ps5lift_import_host__thread__keeps__environment(Ps5LiftState* state) {
    ThreadCall call{Callback<long(long)>(Integer<std::uint64_t>(state, 0)), Integer<long>(state, 1), 0};
    pthread_t thread;
    pthread_create(&thread, nullptr, [](void* argument) -> void* {
        auto* call = static_cast<ThreadCall*>(argument);
        ResetHostFloatingPointControl();
        const auto before = HostFloatingPointControl();
        ps5lift_current_state();
        const auto afterQuery = HostFloatingPointControl();
        call->Function(call->Argument);
        call->Result = afterQuery == before && HostFloatingPointControl() == before;
        return nullptr;
    }, &call);
    pthread_join(thread, nullptr);
    Return(state, call.Result);
}

extern "C" void ps5lift_import_host__call__at__thread__exit(Ps5LiftState* state) {
    ThreadCall call{Callback<long(long)>(Integer<std::uint64_t>(state, 0)), Integer<long>(state, 1), 0};
    pthread_t thread;
    pthread_create(&thread, nullptr, [](void* argument) -> void* {
        struct Farewell {
            ThreadCall* Pending = nullptr;
            ~Farewell() { Pending->Result = Pending->Function(Pending->Argument); }
        };
        thread_local Farewell farewell;
        farewell.Pending = static_cast<ThreadCall*>(argument);
        farewell.Pending->Function(0);
        return nullptr;
    }, &call);
    pthread_join(thread, nullptr);
    Return(state, call.Result);
}

extern "C" int GuestCpuIterateImages_nid_no_patch(int (*callback)(void*, std::size_t, void*), void* data);

extern "C" void ps5lift_import_dl__iterate__phdr(Ps5LiftState* state) {
    Return(state, GuestCpuIterateImages_nid_no_patch(Callback<int(void*, std::size_t, void*)>(Integer<std::uint64_t>(state, 0)), Integer<void*>(state, 1)));
}

extern "C" void ps5lift_import_host__walk__then__take__stack(Ps5LiftState* state) {
    constexpr std::size_t StackSize = 1 << 20;
    ThreadCall call{Callback<long(long)>(Integer<std::uint64_t>(state, 0)), Integer<long>(state, 1), 0};
    void* const stack = mmap(nullptr, StackSize, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) std::abort();
    std::thread([&call, stack] {
        bool sawTlsBlock = false;
        GuestCpuIterateImages_nid_no_patch([](void* image, std::size_t, void* data) {
            const auto* info = static_cast<const dl_phdr_info*>(image);
            if (info->dlpi_tls_modid != 0 && info->dlpi_tls_data != nullptr) *static_cast<bool*>(data) = true;
            return 0;
        }, &sawTlsBlock);
        ps5lift_use_guest_stack(reinterpret_cast<std::uint64_t>(stack) + StackSize);
        call.Result = sawTlsBlock ? -1 : call.Function(call.Argument);
    }).join();
    munmap(stack, StackSize);
    Return(state, call.Result);
}

extern "C" void ps5lift_import_host__call__eight(Ps5LiftState* state) {
    auto* function = Callback<long(long, long, long, long, long, long, long, long)>(Integer<std::uint64_t>(state, 0));
    Return(state, function(1, 2, 3, 4, 5, 6, 7, 8) + function(10, 20, 30, 40, 50, 60, 70, 80));
}

struct RecordedCalls {
    long (*integers)(long, long, long, long, long, long, long, long);
    double (*mixed)(double, long, double);
    long (*unaligned)(long, long);
};

extern "C" void ps5lift_import_host__call__recorded(Ps5LiftState* state) {
    const auto* calls = Integer<const RecordedCalls*>(state, 0);
    const auto base = Integer<long>(state, 1);
    Return(state, calls->integers(base, base + 1, base + 2, base + 3, base + 4, base + 5, base + 6, base + 7) + static_cast<long>(calls->mixed(1.5, base, 0.25) * 1000.0) + calls->unaligned(base, 3));
}

extern "C" void ps5lift_import_host__extended(Ps5LiftState* state) {
    long sum = Stack<long>(state, 0);
    for (int index = 0; index < 6; ++index) sum += Integer<long>(state, index);
    ReturnExtended(state, sum != 28 ? LongDouble() : Extended(state, 2) / Extended(state, 4));
}
