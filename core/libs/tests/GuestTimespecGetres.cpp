#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "timespec_getres check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

static int calls, observedClock, response, errorToSet;
static KernelTimespec* observedPointer;
static KernelTimespec resolution;
static bool writeResolution;

static int APS5_VABI ClockResolution(int clock, KernelTimespec* output) {
    ++calls;
    observedClock = clock;
    observedPointer = output;
    if (output != nullptr && writeResolution) *output = resolution;
    *__error_nid_postfix() = errorToSet;
    return response;
}

static void Mock(int result, bool write, KernelTimespec value = {0, 100}) {
    calls = 0;
    observedClock = -1;
    observedPointer = nullptr;
    response = result;
    writeResolution = write;
    resolution = value;
    errorToSet = 71;
    RegisterClockGetres_nid_no_patch(&ClockResolution);
}

static void Unsupported() {
    Mock(0, true);
#ifdef _WIN32
    auto* guard = static_cast<KernelTimespec*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    Require(guard != nullptr);
#else
    auto* guard = static_cast<KernelTimespec*>(mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(guard != MAP_FAILED);
#endif
    KernelTimespec value{123, 456};
    const auto checkBase = [&](int base) {
        if (base == 1) return;
        for (auto* output : {&value, guard, static_cast<KernelTimespec*>(nullptr)}) {
            *__error_nid_postfix() = 97;
            Require(timespec_getres_nid_postfix(output, base) == 0);
            Require(*__error_nid_postfix() == 97 && calls == 0);
            Require(value.tv_sec == 123 && value.tv_nsec == 456);
        }
    };
    for (int base = -4096; base <= 4096; ++base) checkBase(base);
    checkBase(INT_MIN);
    checkBase(INT_MAX);
#ifdef _WIN32
    VirtualFree(guard, 0, MEM_RELEASE);
#else
    munmap(guard, 4096);
#endif
}

struct Frame {
    std::uint64_t before;
    KernelTimespec value;
    std::uint64_t after;
};

static void CheckForward(int result, bool write, bool null, KernelTimespec value) {
    Mock(result, write, value);
    Frame frame{0x123456789ABCDEF0, {123, 456}, 0xFEDCBA9876543210};
    auto* output = null ? nullptr : &frame.value;
    *__error_nid_postfix() = 97;
    Require(timespec_getres_nid_postfix(output, 1) == (result == 0 ? 1 : 0));
    Require(calls == 1 && observedClock == 0 && observedPointer == output);
    Require(*__error_nid_postfix() == 71);
    const auto expected = !null && write ? value : KernelTimespec{123, 456};
    Require(frame.value.tv_sec == expected.tv_sec && frame.value.tv_nsec == expected.tv_nsec);
    Require(frame.before == 0x123456789ABCDEF0 && frame.after == 0xFEDCBA9876543210);
}

static void Forwarding() {
    for (const auto value : {KernelTimespec{0, 1}, KernelTimespec{2, 999999999},
                            KernelTimespec{INT64_MIN, INT64_MAX}}) {
        for (const bool null : {false, true}) CheckForward(0, true, null, value);
    }
    CheckForward(0, false, false, {0, 100});
}

static void Errors() {
    for (const int result : {-1, 1, INT_MIN, INT_MAX, static_cast<int>(0x8002000E)}) {
        for (const bool write : {false, true}) {
            for (const bool null : {false, true}) CheckForward(result, write, null, {7, 7654321});
        }
    }
}

static void MissingProvider() {
    RegisterClockGetres_nid_no_patch(nullptr);
    Require(timespec_getres_nid_postfix(nullptr, 0) == 0);
    bool threw = false;
    try {
        timespec_getres_nid_postfix(nullptr, 1);
    } catch (const std::runtime_error& error) {
        threw = std::string(error.what()).find("timespec_getres") != std::string::npos;
    }
    Require(threw);
}

static void Integration() {
    KernelTimespec expected{};
    Require(sceKernelClockGetres(0, &expected) == 0);
    Require(expected.tv_sec >= 0 && expected.tv_nsec >= 0 && expected.tv_nsec < 1000000000);
    for (int index = 0; index < 1024; ++index) {
        Frame frame{0x123456789ABCDEF0, {-1, -1}, 0xFEDCBA9876543210};
        *__error_nid_postfix() = 97;
        Require(timespec_getres_nid_postfix(&frame.value, 1) == 1);
        Require(*__error_nid_postfix() == 97);
        Require(frame.value.tv_sec == expected.tv_sec && frame.value.tv_nsec == expected.tv_nsec);
        Require(frame.before == 0x123456789ABCDEF0 && frame.after == 0xFEDCBA9876543210);
    }
    const int nullResult = sceKernelClockGetres(0, nullptr) == 0 ? 1 : 0;
    Require(timespec_getres_nid_postfix(nullptr, 1) == nullResult);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    if (std::strcmp(argv[1], "unsupported") == 0) Unsupported();
    else if (std::strcmp(argv[1], "forwarding") == 0) Forwarding();
    else if (std::strcmp(argv[1], "errors") == 0) Errors();
    else if (std::strcmp(argv[1], "missing_provider") == 0) MissingProvider();
    else if (std::strcmp(argv[1], "integration") == 0) Integration();
    else Require(false);
    RegisterClockGetres_nid_no_patch(&sceKernelClockGetres);
    std::puts("timespec_getres bases, delegation, errors and kernel integration passed");
}
