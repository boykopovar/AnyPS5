#include "prx/libc/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include <climits>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "timespec_get check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

static constexpr std::int64_t NANOS_PER_SECOND = 1000000000;

struct Frame {
    std::uint64_t before;
    KernelTimespec value;
    std::uint64_t after;
};

static void Unsupported() {
#ifdef _WIN32
    auto* guard = static_cast<KernelTimespec*>(VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
    Require(guard != nullptr);
#else
    auto* guard = static_cast<KernelTimespec*>(mmap(nullptr, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(guard != MAP_FAILED);
#endif
    const auto checkBase = [&](int base) {
        if (base == 1) return;
        Frame frame{0x123456789ABCDEF0, {123, 456}, 0xFEDCBA9876543210};
        for (auto* output : {&frame.value, guard}) {
            Require(timespec_get_nid_postfix(output, base) == 0);
            Require(frame.value.tv_sec == 123 && frame.value.tv_nsec == 456);
            Require(frame.before == 0x123456789ABCDEF0 && frame.after == 0xFEDCBA9876543210);
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

static void Value() {
    for (int index = 0; index < 1024; ++index) {
        Frame frame{0x123456789ABCDEF0, {-1, -1}, 0xFEDCBA9876543210};
        Require(timespec_get_nid_postfix(&frame.value, 1) == 1);
        Require(frame.value.tv_sec > 1600000000);
        Require(frame.value.tv_nsec >= 0 && frame.value.tv_nsec < NANOS_PER_SECOND);
        Require(frame.before == 0x123456789ABCDEF0 && frame.after == 0xFEDCBA9876543210);
    }
}

static void Agreement() {
    for (int index = 0; index < 256; ++index) {
        KernelTimespec kernel{};
        Require(sceKernelClockGettime(0, &kernel) == 0);
        KernelTimespec answered{};
        Require(timespec_get_nid_postfix(&answered, 1) == 1);
        const std::int64_t difference = answered.tv_sec - kernel.tv_sec;
        Require(difference >= -1 && difference <= 1);
        Require(answered.tv_nsec >= 0 && answered.tv_nsec < NANOS_PER_SECOND);
    }
}

static void Monotonic() {
    KernelTimespec previous{};
    Require(timespec_get_nid_postfix(&previous, 1) == 1);
    for (int index = 0; index < 4096; ++index) {
        KernelTimespec current{};
        Require(timespec_get_nid_postfix(&current, 1) == 1);
        const bool ordered = current.tv_sec > previous.tv_sec ||
            (current.tv_sec == previous.tv_sec && current.tv_nsec >= previous.tv_nsec);
        Require(ordered);
        previous = current;
    }
}

static void Rejected() {
    Require(timespec_get_nid_postfix(nullptr, 1) == 0);
    Require(timespec_get_nid_postfix(nullptr, 0) == 0);
    Require(timespec_get_nid_postfix(nullptr, INT_MIN) == 0);
}

int main(int argc, char** argv) {
    Require(argc == 2);
    if (std::strcmp(argv[1], "unsupported") == 0) Unsupported();
    else if (std::strcmp(argv[1], "value") == 0) Value();
    else if (std::strcmp(argv[1], "agreement") == 0) Agreement();
    else if (std::strcmp(argv[1], "monotonic") == 0) Monotonic();
    else if (std::strcmp(argv[1], "rejected") == 0) Rejected();
    else Require(false);
    std::puts("timespec_get bases, value, kernel agreement, ordering and null output passed");
}
