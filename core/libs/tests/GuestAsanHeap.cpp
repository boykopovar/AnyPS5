#include "prx/libSceAddressSanitizer/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include "prx/libc/include/MallocManagedSize.hpp"

#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <functional>
#include <initializer_list>
#include <sstream>
#include <string>
#include <system_error>
#include <sys/wait.h>
#include <unistd.h>

namespace {

std::string ThreadTag() {
    return "T" + std::to_string(scePthreadGetthreadid());
}

void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stdout, "failed: %s\n", what);
    std::fflush(stdout);
    std::abort();
}

struct Outcome {
    std::string output;
    int status = 0;
};

Outcome Run(const char* options, const std::function<void()>& body) {
    int channel[2] = {-1, -1};
    Require(::pipe(channel) == 0, "pipe");
    std::fflush(nullptr);
    const pid_t child = ::fork();
    Require(child >= 0, "fork");
    if (child == 0) {
        ::close(channel[0]);
        if (::dup2(channel[1], 2) != 2) ::_exit(70);
        ::close(channel[1]);
        if (options != nullptr) ::setenv("APS5_GUEST_ASAN_OPTIONS", options, 1);
        else ::unsetenv("APS5_GUEST_ASAN_OPTIONS");
        __asan_init();
        body();
        std::fflush(nullptr);
        ::_exit(0);
    }
    ::close(channel[1]);
    Outcome outcome;
    char buffer[512];
    for (ssize_t count; (count = ::read(channel[0], buffer, sizeof(buffer))) > 0;) outcome.output.append(buffer, static_cast<std::size_t>(count));
    ::close(channel[0]);
    Require(::waitpid(child, &outcome.status, 0) == child, "waitpid");
    return outcome;
}

void Fail(const Outcome& outcome, const char* what, const std::string& detail) {
    std::fprintf(stdout, "%s: %s; status %d, output:\n%s---\n", what, detail.c_str(), outcome.status, outcome.output.c_str());
    std::fflush(stdout);
    std::abort();
}

void Expect(const Outcome& outcome, int code, std::initializer_list<std::string> fragments, const char* what) {
    if (!WIFEXITED(outcome.status) || WEXITSTATUS(outcome.status) != code) Fail(outcome, what, "wrong exit status, expected " + std::to_string(code));
    for (const auto& fragment : fragments)
        if (outcome.output.find(fragment) == std::string::npos) Fail(outcome, what, "missing \"" + fragment + "\"");
}

void ExpectAbsent(const Outcome& outcome, const std::string& fragment, const char* what) {
    if (outcome.output.find(fragment) != std::string::npos) Fail(outcome, what, "unexpected \"" + fragment + "\"");
}

void ExpectClean(const Outcome& outcome, const char* what) {
    if (!WIFEXITED(outcome.status) || WEXITSTATUS(outcome.status) != 0 || !outcome.output.empty()) Fail(outcome, what, "expected a silent clean exit");
}

std::string Hex(const void* pointer) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%012zx", static_cast<std::size_t>(reinterpret_cast<std::uintptr_t>(pointer)));
    return text;
}

std::uintptr_t ShadowAddress(const void* address) {
    return (reinterpret_cast<std::uintptr_t>(address) >> 3) + (std::uintptr_t{1} << 40);
}

std::uint8_t ShadowByte(const void* address) {
    return *reinterpret_cast<const std::uint8_t*>(ShadowAddress(address));
}

unsigned char* Bytes(void* pointer) {
    return static_cast<unsigned char*>(pointer);
}

void Load1(const void* address) {
    __asan_load1(reinterpret_cast<std::uintptr_t>(address));
}

void ExpectAbort(const Outcome& outcome, const char* what);

void HeapLayout() {
    ExpectClean(Run(nullptr, [] {
                    auto* p = Bytes(__asan_malloc(10));
                    Require(p != nullptr && reinterpret_cast<std::uintptr_t>(p) % 8 == 0, "malloc is 8-byte aligned");
                    Require(ShadowByte(p - 16) == 0xfa && ShadowByte(p - 8) == 0xfa, "left redzone");
                    Require(ShadowByte(p) == 0 && ShadowByte(p + 8) == 2 && ShadowByte(p + 16) == 0xfa, "user bytes and partial granule");
                    Require(p[0] == 0xbe && p[9] == 0xbe, "malloc fills with 0xbe");
                    Require(__asan_malloc_usable_size(p) == 10, "usable size is the requested size");
                    auto* zero = Bytes(__asan_calloc(4, 5));
                    Require(zero[0] == 0 && zero[19] == 0 && ShadowByte(zero + 16) == 4, "calloc zeroes and sizes 20 bytes");
                    auto* empty = Bytes(__asan_malloc(0));
                    Require(empty != nullptr && ShadowByte(empty) == 1, "malloc(0) returns one addressable byte");
                    auto* big = Bytes(__asan_malloc(5000));
                    Require(ShadowByte(big - 256) == 0xfa && big[4095] == 0xbe && big[4096] != 0xbe, "5000 bytes use a 256-byte redzone and fill only 4K");
                    __asan_free(p);
                    Require(ShadowByte(p) == 0xfd && ShadowByte(p + 8) == 0xfd, "freed memory is poisoned fd");
                    __asan_free(nullptr);
                    __asan_free(zero);
                    __asan_free(empty);
                    __asan_free(big);
                }),
                "heap layout");
    ExpectClean(Run("redzone=64:malloc_fill_byte=170:max_malloc_fill_size=4:max_free_fill_size=2:free_fill_byte=17", [] {
                    auto* p = Bytes(__asan_malloc(8));
                    Require(ShadowByte(p - 64) == 0xfa, "redzone=64 widens the left redzone");
                    Require(p[0] == 0xaa && p[3] == 0xaa, "malloc_fill_byte");
                    __asan_free(p);
                    Require(p[0] == 17 && p[1] == 17, "free_fill_byte");
                }),
                "fill and redzone flags");
    ExpectClean(Run("poison_heap=0", [] {
                    auto* p = Bytes(__asan_malloc(10));
                    Require(ShadowByte(p - 8) == 0 && ShadowByte(p + 16) == 0, "poison_heap=0 leaves redzones addressable");
                    __asan_free(p);
                    Require(ShadowByte(p) == 0, "poison_heap=0 does not poison freed memory");
                }),
                "poison_heap=0");
    ExpectAbort(Run("redzone=24", [] { __asan_malloc(8); }), "redzone must be a power of two");
}

void HeapReports() {
    static unsigned char* p = nullptr;
    Expect(Run(nullptr, [] {
               p = Bytes(__asan_malloc(10));
               Load1(p + 10);
           }),
           1,
           {"ERROR: AddressSanitizer: heap-buffer-overflow on address ", "READ of size 1 at ", " is located 0 bytes after 10-byte region [", "allocated by thread " + ThreadTag() + " here:\n    #0 0x",
            "SUMMARY: AddressSanitizer: heap-buffer-overflow", "  Heap left redzone:       fa\n"},
           "heap overflow");
    Expect(Run(nullptr, [] {
               p = Bytes(__asan_malloc(10));
               Load1(p - 3);
           }),
           1, {"heap-buffer-overflow", " is located 3 bytes before 10-byte region ["}, "heap underflow");
    Expect(Run(nullptr, [] {
               p = Bytes(__asan_malloc(32));
               __asan_free(p);
               __asan_load8(reinterpret_cast<std::uintptr_t>(p + 8));
           }),
           1, {"heap-use-after-free on address ", " is located 8 bytes inside of 32-byte region [", "freed by thread " + ThreadTag() + " here:\n    #0 0x", "previously allocated by thread " + ThreadTag() + " here:\n    #0 0x"},
           "use after free");
    Expect(Run(nullptr, [] {
               p = Bytes(__asan_malloc(16));
               __asan_free(p);
               __asan_free(p);
           }),
           1, {"ERROR: AddressSanitizer: attempting double-free on 0x", " in thread " + ThreadTag() + ":\n", "SUMMARY: AddressSanitizer: double-free"}, "double free");
    Expect(Run(nullptr, [] {
               p = Bytes(__asan_malloc(16));
               __asan_free(p + 4);
           }),
           1, {"attempting free on address which was not malloc()-ed: 0x", " is located 4 bytes inside of 16-byte region [", "SUMMARY: AddressSanitizer: bad-free"}, "interior free");
    static unsigned char outside[64];
    Expect(Run(nullptr, [] { __asan_free(outside + 8); }), 1, {"attempting free on address which was not malloc()-ed: " + Hex(outside + 8)}, "free of static memory");
    Expect(Run("halt_on_error=0", [] {
               p = Bytes(__asan_malloc(16));
               __asan_free(p);
               __asan_free(p);
               std::fputs("continued\n", stderr);
           }),
           0, {"attempting double-free", "continued\n"}, "recoverable double free");
    Expect(Run(nullptr, [] {
               p = Bytes(__asan_malloc(24));
               __asan_describe_address(reinterpret_cast<std::uintptr_t>(p + 30));
           }),
           0, {" is located 6 bytes after 24-byte region [", "allocated by thread " + ThreadTag() + " here:"}, "describe heap address");
}

void Reallocation() {
    ExpectClean(Run(nullptr, [] {
                    auto* p = Bytes(__asan_malloc(8));
                    std::memcpy(p, "abcdefgh", 8);
                    auto* q = Bytes(__asan_realloc(p, 20));
                    Require(q != p && std::memcmp(q, "abcdefgh", 8) == 0 && ShadowByte(q + 16) == 4, "realloc grows and copies");
                    Require(ShadowByte(p) == 0xfd, "realloc frees the old block");
                    auto* r = Bytes(__asan_realloc(q, 3));
                    Require(std::memcmp(r, "abc", 3) == 0 && __asan_malloc_usable_size(r) == 3, "realloc shrinks");
                    auto* fresh = Bytes(__asan_realloc(nullptr, 5));
                    Require(__asan_malloc_usable_size(fresh) == 5, "realloc of null allocates");
                    Require(__asan_realloc(fresh, 0) == nullptr && ShadowByte(fresh) == 0xfd, "realloc to zero frees and returns null");
                    auto* aligned = Bytes(__asan_reallocalign(r, 40, 64));
                    Require(reinterpret_cast<std::uintptr_t>(aligned) % 64 == 0 && std::memcmp(aligned, "abc", 3) == 0, "reallocalign aligns and copies");
                    auto* alignedFresh = Bytes(__asan_reallocalign(nullptr, 7, 128));
                    Require(reinterpret_cast<std::uintptr_t>(alignedFresh) % 128 == 0, "reallocalign of null allocates aligned");
                }),
                "realloc");
    ExpectClean(Run("allocator_frees_and_returns_null_on_realloc_zero=0", [] {
                    auto* p = Bytes(__asan_malloc(8));
                    auto* q = Bytes(__asan_realloc(p, 0));
                    Require(q != nullptr && __asan_malloc_usable_size(q) == 1, "realloc to zero keeps one byte when configured");
                }),
                "realloc zero kept");
    Expect(Run(nullptr, [] {
               auto* p = Bytes(__asan_malloc(8));
               __asan_free(p);
               __asan_realloc(p, 16);
           }),
           1, {"attempting double-free"}, "realloc of freed memory");
    const auto twice = Run("halt_on_error=0", [] {
        auto* p = Bytes(__asan_malloc(8));
        __asan_free(p);
        __asan_realloc(p, 16);
    });
    std::size_t reports = 0;
    for (std::size_t at = 0; (at = twice.output.find("attempting double-free", at)) != std::string::npos; ++at) ++reports;
    if (reports != 2) Fail(twice, "realloc of freed memory in recovery", "expected the realloc and the release to report");
}

void Alignment() {
    ExpectClean(Run(nullptr, [] {
                    auto* p = Bytes(__asan_memalign(256, 10));
                    Require(reinterpret_cast<std::uintptr_t>(p) % 256 == 0 && ShadowByte(p - 8) == 0xfa, "memalign 256");
                    auto* q = Bytes(__asan_aligned_alloc(32, 64));
                    Require(reinterpret_cast<std::uintptr_t>(q) % 32 == 0, "aligned_alloc 32");
                    void* r = nullptr;
                    Require(__asan_posix_memalign(&r, 64, 3) == 0 && reinterpret_cast<std::uintptr_t>(r) % 64 == 0, "posix_memalign 64");
                    __asan_free(p);
                    __asan_free(q);
                    __asan_free(r);
                }),
                "aligned allocation");
    Expect(Run(nullptr, [] { __asan_memalign(3, 8); }), 1,
           {"ERROR: AddressSanitizer: invalid allocation alignment: 3, alignment must be a power of two (thread " + ThreadTag() + ")\n", "HINT: if you don't care about these errors you may set allocator_may_return_null=1\n",
            "SUMMARY: AddressSanitizer: invalid-allocation-alignment"},
           "memalign alignment");
    Expect(Run(nullptr, [] { __asan_aligned_alloc(16, 20); }), 1,
           {"invalid alignment requested in aligned_alloc: 16, alignment must be a power of two and the requested size 0x14 must be a multiple of alignment (thread " + ThreadTag() + ")\n"}, "aligned_alloc size");
    Expect(Run(nullptr, [] {
               void* r = nullptr;
               __asan_posix_memalign(&r, 4, 8);
           }),
           1, {"invalid alignment requested in posix_memalign: 4, alignment must be a power of two and a multiple of sizeof(void*) == 8 (thread " + ThreadTag() + ")\n"}, "posix_memalign alignment");
    Expect(Run(nullptr, [] { __asan_reallocalign(nullptr, 8, 12); }), 1, {"invalid allocation alignment: 12"}, "reallocalign alignment");
    ExpectClean(Run("allocator_may_return_null=1", [] {
                    *__error_nid_postfix() = 0;
                    Require(__asan_memalign(3, 8) == nullptr && *__error_nid_postfix() == 22, "memalign returns null with EINVAL");
                    Require(__asan_aligned_alloc(16, 20) == nullptr && *__error_nid_postfix() == 22, "aligned_alloc returns null with EINVAL");
                    void* r = nullptr;
                    Require(__asan_posix_memalign(&r, 4, 8) == 22 && r == nullptr, "posix_memalign returns EINVAL");
                    *__error_nid_postfix() = 0;
                    Require(__asan_calloc(~std::size_t{0}, 2) == nullptr && *__error_nid_postfix() == 12, "calloc overflow returns null with ENOMEM");
                }),
                "may return null");
}

void Limits() {
    Expect(Run(nullptr, [] { __asan_calloc(~std::size_t{0} / 2, 3); }), 1,
           {"ERROR: AddressSanitizer: calloc parameters overflow: count * size (9223372036854775807 * 3) cannot be represented in type size_t (thread " + ThreadTag() + ")\n", "SUMMARY: AddressSanitizer: calloc-overflow"},
           "calloc overflow");
    Expect(Run(nullptr, [] { __asan_malloc(std::size_t{1} << 41); }), 1,
           {"ERROR: AddressSanitizer: requested allocation size 0x20000000000 (0x", " after adjustments for alignment, red zones etc.) exceeds maximum supported size of 0x10000000000 (thread " + ThreadTag() + ")\n",
            "SUMMARY: AddressSanitizer: allocation-size-too-big"},
           "too big");
    Expect(Run("max_allocation_size_mb=1", [] { __asan_malloc(std::size_t{2} << 20); }), 1, {"requested allocation size 0x200000 ", "exceeds maximum supported size of 0x100000"},
           "max_allocation_size_mb");
    Expect(Run("allocator_may_return_null=1", [] { Require(__asan_malloc(std::size_t{1} << 41) == nullptr, "too big returns null"); }), 0,
           {"WARNING: AddressSanitizer failed to allocate 0x20000000000 bytes\n"}, "too big returns null");
    Expect(Run(nullptr, [] { __asan_malloc(std::size_t{0xfff} << 28); }), 1,
           {"ERROR: AddressSanitizer: out of memory: allocator is trying to allocate 0xfff0000000 bytes\n", "SUMMARY: AddressSanitizer: out-of-memory"}, "out of memory");
}

void Ownership() {
    static unsigned char outside[32];
    ExpectClean(Run(nullptr, [] {
                    auto* p = Bytes(__asan_malloc(40));
                    Require(__sanitizer_get_ownership(p) == 1 && __sanitizer_get_ownership(p + 1) == 0 && __sanitizer_get_ownership(outside) == 0, "ownership");
                    Require(__sanitizer_get_allocated_size(p) == 40 && __sanitizer_get_allocated_size_fast(p) == 40, "allocated size");
                    Require(__sanitizer_get_allocated_begin(p + 39) == p && __sanitizer_get_allocated_begin(p + 40) == nullptr, "allocated begin");
                    Require(__sanitizer_get_allocated_size(nullptr) == 0 && __asan_malloc_usable_size(nullptr) == 0, "null sizes");
                    Require(__sanitizer_get_estimated_allocated_size(77) == 77, "estimated size");
                    Require(__asan_update_allocation_context(p) == 1, "update context of a live chunk");
                    __asan_free(p);
                    Require(__sanitizer_get_ownership(p) == 0 && __sanitizer_get_allocated_begin(p) == nullptr, "freed memory is not owned");
                    Require(__asan_update_allocation_context(p) == 0, "update context of a freed chunk");
                }),
                "ownership");
    Expect(Run(nullptr, [] { __asan_malloc_usable_size(outside); }), 1,
           {"ERROR: AddressSanitizer: attempting to call malloc_usable_size() for pointer which is not owned: " + Hex(outside) + "\n", "SUMMARY: AddressSanitizer: bad-malloc_usable_size"},
           "usable size not owned");
    ExpectClean(Run("check_malloc_usable_size=0", [] { Require(__asan_malloc_usable_size(outside) == 0, "unchecked usable size"); }), "check_malloc_usable_size=0");
    Expect(Run(nullptr, [] { __sanitizer_get_allocated_size(outside); }), 1,
           {"attempting to call __sanitizer_get_allocated_size() for pointer which is not owned: " + Hex(outside), "SUMMARY: AddressSanitizer: bad-__sanitizer_get_allocated_size"},
           "allocated size not owned");
    ExpectAbort(Run(nullptr, [] { __sanitizer_get_allocated_size_fast(outside); }), "fast size of a foreign pointer");
}

void Statistics() {
    Expect(Run(nullptr, [] {
               const std::size_t before = __sanitizer_get_current_allocated_bytes();
               auto* p = __asan_malloc(1000);
               auto* q = __asan_malloc(500);
               Require(__sanitizer_get_current_allocated_bytes() >= 1500 && __sanitizer_get_current_allocated_bytes() - (before > 1 ? before : 0) >= 1500, "current allocated bytes");
               Require(__sanitizer_get_heap_size() > 1500 && __sanitizer_get_free_bytes() >= 1 && __sanitizer_get_unmapped_bytes() == 0, "heap figures");
               __asan_free(p);
               MallocManagedSize managed{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
               Require(__asan_malloc_stats(&managed) == 0, "malloc_stats");
               Require(managed.currentInuseSize == 500 && managed.maxInuseSize >= 1500 && managed.currentSystemSize >= 1500 && managed.maxSystemSize >= managed.currentSystemSize, "malloc_stats values");
               MallocManagedSize wrong{sizeof(MallocManagedSize), 2, 0, 0, 0, 0, 0};
               Require(__asan_malloc_stats(&wrong) == 1, "malloc_stats rejects another version");
               __asan_print_accumulated_stats();
               __asan_free(q);
           }),
           0, {"Stats: 0M malloced (0M for red zones) by 2 calls\n", "Stats: 0M freed by 1 calls\n", "Stats: 0M really freed by 0 calls\n", "mmaped; 2 maps, 0 unmaps\n"}, "statistics");
}

void Quarantine() {
    ExpectClean(Run("quarantine_size_mb=0", [] {
                    auto* p = Bytes(__asan_malloc(32));
                    __asan_free(p);
                    Require(ShadowByte(p) == 0 && ShadowByte(p - 8) == 0, "without a quarantine freed blocks return unpoisoned");
                }),
                "no quarantine");
    ExpectClean(Run(nullptr, [] {
                    auto* p = Bytes(__asan_malloc(32));
                    __asan_free(p);
                    Require(ShadowByte(p) == 0xfd, "quarantined");
                    __sanitizer_purge_allocator();
                    Require(ShadowByte(p) == 0, "purge drains the quarantine");
                    MallocManagedSize managed{sizeof(MallocManagedSize), 1, 0, 0, 0, 0, 0};
                    __asan_malloc_stats(&managed);
                    Require(managed.currentSystemSize == 0, "purged blocks are released");
                }),
                "purge");
    ExpectClean(Run("quarantine_size_mb=1", [] {
                    auto* first = Bytes(__asan_malloc(600 * 1024));
                    __asan_free(first);
                    Require(ShadowByte(first) == 0xfd, "first block quarantined");
                    auto* second = Bytes(__asan_malloc(600 * 1024));
                    __asan_free(second);
                    Require(ShadowByte(first) == 0 && ShadowByte(second) == 0xfd, "oldest block leaves a full quarantine first");
                }),
                "quarantine limit");
}

void Debugging() {
    static unsigned char global[48];
    static std::uintptr_t trace[4];
    static std::uint32_t thread = 0;
    ExpectClean(Run(nullptr, [] {
                    auto* p = Bytes(__asan_malloc(24));
                    Require(__asan_get_alloc_stack(reinterpret_cast<std::uintptr_t>(p), trace, 4, &thread) == 4 && trace[0] != 0 && trace[1] != 0 && trace[3] != 0 && trace[0] != trace[1] && trace[1] != trace[2] && thread == static_cast<std::uint32_t>(scePthreadGetthreadid()), "alloc stack holds four frames and the thread");
                    Require(__asan_get_free_stack(reinterpret_cast<std::uintptr_t>(p), trace, 4, &thread) == 0, "no free stack before free");
                    __asan_free(p);
                    const std::uintptr_t allocationFrame = trace[0];
                    trace[0] = 0;
                    Require(__asan_get_free_stack(reinterpret_cast<std::uintptr_t>(p + 4), trace, 4, nullptr) == 4 && trace[0] != 0, "free stack");
                    Require(trace[0] != allocationFrame, "the free stack is the free site, not the allocation site");
                    trace[0] = 0;
                    Require(__asan_get_alloc_stack(reinterpret_cast<std::uintptr_t>(p), trace, 4, nullptr) == 4 && trace[0] == allocationFrame, "the allocation stack survives the free");
                    Require(__asan_get_alloc_stack(reinterpret_cast<std::uintptr_t>(global), trace, 4, nullptr) == 0, "no stack for non-heap memory");
                    char name[16] = "x";
                    std::uintptr_t begin = 1;
                    std::uintptr_t size = 1;
                    auto* q = Bytes(__asan_malloc(40));
                    Require(std::strcmp(__asan_locate_address(reinterpret_cast<std::uintptr_t>(q + 41), name, sizeof(name), &begin, &size), "heap") == 0 && begin == reinterpret_cast<std::uintptr_t>(q) &&
                                size == 40 && name[0] == 0,
                            "locate heap");
                    Require(std::strcmp(__asan_locate_address(reinterpret_cast<std::uintptr_t>(global), name, sizeof(name), &begin, &size), "heap-invalid") == 0 && begin == 0 && size == 0, "locate wild");
                    Require(std::strcmp(__asan_locate_address((std::uintptr_t{1} << 40) + 8, nullptr, 0, nullptr, nullptr), "high shadow") == 0, "locate shadow");
                    struct GlobalDescriptor {
                        std::uintptr_t beg, size, sizeWithRedzone;
                        const char* name;
                        const char* moduleName;
                        std::uintptr_t hasDynamicInit, location, odrIndicator;
                    };
                    alignas(64) static GlobalDescriptor descriptor;
                    descriptor = GlobalDescriptor{reinterpret_cast<std::uintptr_t>(global), 40, 48, "global_table", "game.elf", 0, 0, 0};
                    __asan_register_globals(&descriptor, 1);
                    Require(std::strcmp(__asan_locate_address(reinterpret_cast<std::uintptr_t>(global + 3), name, 7, &begin, &size), "global") == 0 && std::strcmp(name, "global") == 0 &&
                                begin == reinterpret_cast<std::uintptr_t>(global) && size == 40,
                            "locate global with a short name buffer");
                }),
                "debugging");
}

[[gnu::noinline]] std::uintptr_t AllocateFrameDeeper() {
    volatile char pad[256] = {};
    const std::uintptr_t frame = __asan_stack_malloc_always_0(64);
    return frame + static_cast<std::uintptr_t>(pad[0]);
}

void FakeStack() {
    ExpectClean(Run(nullptr, [] {
                    Require(__asan_stack_malloc_0(64) == 0 && __asan_stack_malloc_10(65536) == 0, "fake stack is off by default");
                    Require(__asan_get_current_fake_stack() == nullptr, "no fake stack by default");
                    const std::uintptr_t frame = __asan_stack_malloc_always_2(256);
                    Require(frame != 0 && frame % 256 == 0, "malloc_always makes a frame");
                    Require(__asan_get_current_fake_stack() != nullptr, "the always form creates the thread's fake stack");
                    __asan_stack_free_2(frame, 256);
                }),
                "fake stack off");
    ExpectClean(Run("detect_stack_use_after_return=1", [] {
                    const std::uintptr_t frame = __asan_stack_malloc_0(64);
                    Require(frame != 0 && ShadowByte(reinterpret_cast<void*>(frame)) == 0 && ShadowByte(reinterpret_cast<void*>(frame + 56)) == 0, "frame is addressable");
                    std::uint8_t* flag = *reinterpret_cast<std::uint8_t**>(frame + 64 - sizeof(void*));
                    Require(flag != nullptr && *flag == 1, "the frame stores a pointer to its in-use flag");
                    const std::uintptr_t second = __asan_stack_malloc_0(64);
                    Require(second != frame && second != 0, "a second frame of the same class");
                    void* fake = __asan_get_current_fake_stack();
                    Require(fake != nullptr, "fake stack exists");
                    auto* header = reinterpret_cast<std::uintptr_t*>(frame);
                    header[0] = 0x41B58AB3;
                    void* begin = nullptr;
                    void* end = nullptr;
                    void* real = __asan_addr_is_in_fake_stack(fake, reinterpret_cast<void*>(frame + 40), &begin, &end);
                    Require(real != nullptr && reinterpret_cast<std::uintptr_t>(begin) == frame + 32 && reinterpret_cast<std::uintptr_t>(end) == frame + 64, "frame bounds");
                    header[0] = 0;
                    Require(__asan_addr_is_in_fake_stack(fake, reinterpret_cast<void*>(frame + 40), nullptr, nullptr) == nullptr, "a retired frame is not reported");
                    Require(__asan_addr_is_in_fake_stack(nullptr, reinterpret_cast<void*>(frame), nullptr, nullptr) == nullptr, "null fake stack");
                    static int local = 0;
                    Require(__asan_addr_is_in_fake_stack(fake, &local, nullptr, nullptr) == nullptr, "address outside the fake stack");
                    __asan_stack_free_0(frame, 64);
                    Require(*flag == 0 && ShadowByte(reinterpret_cast<void*>(frame)) == 0xf5 && ShadowByte(reinterpret_cast<void*>(frame + 56)) == 0xf5, "free retires the frame");
                    const std::uintptr_t large = __asan_stack_malloc_8(16384);
                    Require(large != 0 && ShadowByte(reinterpret_cast<void*>(large + 16000)) == 0, "class 8 frame");
                    __asan_stack_free_8(large, 16384);
                    Require(ShadowByte(reinterpret_cast<void*>(large + 16000)) == 0xf5, "class 8 frame retired");
                }),
                "fake stack frames");
    ExpectClean(Run("detect_stack_use_after_return=1:min_uar_stack_size_log=16:max_uar_stack_size_log=16", [] {
                    const std::uintptr_t only = __asan_stack_malloc_10(65536);
                    Require(only != 0, "a 64K stack holds one class 10 frame");
                    Require(__asan_stack_malloc_10(65536) == 0, "a used frame is not handed out again");
                    __asan_stack_free_10(only, 65536);
                    Require(ShadowByte(reinterpret_cast<void*>(only)) == 0xf5, "the released frame is retired");
                    Require(__asan_stack_malloc_10(65536) == only && ShadowByte(reinterpret_cast<void*>(only)) == 0 && ShadowByte(reinterpret_cast<void*>(only + 65528)) == 0, "a released frame is reused and addressable");
                }),
                "fake stack exhaustion");
    Expect(Run("detect_stack_use_after_return=1", [] {
               static const char description[] = "2 32 4 1 a 48 8 9 buffer:12";
               const std::uintptr_t frame = __asan_stack_malloc_0(64);
               auto* header = reinterpret_cast<std::uintptr_t*>(frame);
               header[0] = 0x41B58AB3;
               header[1] = reinterpret_cast<std::uintptr_t>(description);
               header[2] = 0x1234;
               __asan_stack_free_0(frame, 64);
               __asan_load2(frame + 50);
           }),
           1,
           {"ERROR: AddressSanitizer: stack-use-after-return on address ", " is located in stack of thread " + ThreadTag() + " at offset 50 in frame\n    #0 0x1233  (<unknown module>)\n\n",
            "  This frame has 2 object(s):\n", "    [32, 36) 'a'\n", "    [48, 56) 'buffer' (line 12) <== Memory access at offset 50 is inside this variable\n",
            "HINT: this may be a false positive if your program uses some custom stack unwind mechanism, swapcontext or vfork\n"},
           "stack use after return description");
    ExpectClean(Run("detect_stack_use_after_return=1", [] {
                    const std::uintptr_t deep = AllocateFrameDeeper();
                    std::uint8_t* flag = *reinterpret_cast<std::uint8_t**>(deep + 64 - sizeof(void*));
                    Require(*flag == 1, "deep frame in use");
                    __asan_handle_no_return();
                    const std::uintptr_t shallow = __asan_stack_malloc_0(64);
                    Require(shallow != 0 && *flag == 0, "handle_no_return collects frames below the current stack");
                }),
                "fake stack collection");
}

void RealStack() {
    Expect(Run(nullptr, [] {
               static const char description[] = "1 32 16 1 x";
               alignas(64) std::uintptr_t frame[16] = {};
               frame[0] = 0x41B58AB3;
               frame[1] = reinterpret_cast<std::uintptr_t>(description);
               frame[2] = 0x5678;
               __asan_set_shadow_f1(ShadowAddress(frame), 4);
               __asan_set_shadow_f3(ShadowAddress(frame + 6), 2);
               __asan_load4(reinterpret_cast<std::uintptr_t>(frame + 6));
           }),
           1, {"stack-buffer-overflow on address ", " is located in stack of thread " + ThreadTag() + " at offset 48 in frame\n    #0 0x5677  (<unknown module>)\n\n", "    [32, 48) 'x' <== Memory access at offset 48 overflows this variable\n"},
           "real stack frame description");
    Expect(Run(nullptr, [] {
               alignas(64) std::uintptr_t frame[8] = {};
               __asan_set_shadow_f8(ShadowAddress(frame), 1);
               __asan_load1(reinterpret_cast<std::uintptr_t>(frame));
           }),
           1, {"stack-use-after-scope on address ", " is located in stack of thread " + ThreadTag()}, "stack address without a frame");
    ExpectClean(Run(nullptr, [] {
                    alignas(64) static unsigned char fiber[4096];
                    void* save = reinterpret_cast<void*>(1);
                    __sanitizer_start_switch_fiber(&save, fiber, sizeof(fiber));
                    Require(save == nullptr, "no fake stack to save");
                    const void* oldBottom = nullptr;
                    std::size_t oldSize = 0;
                    __sanitizer_finish_switch_fiber(save, &oldBottom, &oldSize);
                    Require(oldBottom != nullptr && oldSize > 0, "finish returns the old stack");
                    __asan_poison_memory_region(fiber + 64, 64);
                    __sanitizer_start_switch_fiber(nullptr, oldBottom, oldSize);
                    const void* fiberBottom = nullptr;
                    std::size_t fiberSize = 0;
                    __sanitizer_finish_switch_fiber(nullptr, &fiberBottom, &fiberSize);
                    Require(fiberBottom == fiber && fiberSize == sizeof(fiber), "the fiber's bounds were current");
                }),
                "fibers");
    Expect(Run(nullptr, [] {
               alignas(64) static unsigned char fiber[4096];
               __sanitizer_start_switch_fiber(nullptr, fiber, sizeof(fiber));
               __sanitizer_start_switch_fiber(nullptr, fiber, sizeof(fiber));
           }),
           1, {"ERROR: starting fiber switch while in fiber switch\n"}, "nested fiber switch");
    Expect(Run(nullptr, [] { __sanitizer_finish_switch_fiber(nullptr, nullptr, nullptr); }), 1, {"ERROR: finishing a fiber switch that has not started\n"}, "finish without start");
}

alignas(64) unsigned char storage[64];

void Containers() {
    ExpectClean(Run(nullptr, [] {
                    __sanitizer_annotate_contiguous_container(storage, storage + 32, storage + 32, storage + 10);
                    Require(ShadowByte(storage) == 0 && ShadowByte(storage + 8) == 2 && ShadowByte(storage + 16) == 0xfc && ShadowByte(storage + 24) == 0xfc, "shrink to 10");
                    Require(__sanitizer_verify_contiguous_container(storage, storage + 10, storage + 32) == 1, "verify the annotation");
                    Require(__sanitizer_contiguous_container_find_bad_address(storage, storage + 10, storage + 32) == nullptr, "no bad address");
                    Require(__sanitizer_contiguous_container_find_bad_address(storage, storage + 20, storage + 32) == storage + 10, "first bad address for a wrong middle");
                    Require(__sanitizer_verify_contiguous_container(storage, storage + 20, storage + 32) == 0, "verify fails for a wrong middle");
                    __sanitizer_annotate_contiguous_container(storage, storage + 32, storage + 10, storage + 27);
                    Require(ShadowByte(storage + 16) == 0 && ShadowByte(storage + 24) == 3, "grow to 27");
                    __sanitizer_annotate_contiguous_container(storage, storage + 32, storage + 27, storage + 32);
                    Require(ShadowByte(storage + 24) == 0, "grow to the full storage");
                    __sanitizer_annotate_double_ended_contiguous_container(storage, storage + 64, storage, storage + 64, storage + 16, storage + 40);
                    Require(ShadowByte(storage) == 0xfc && ShadowByte(storage + 8) == 0xfc && ShadowByte(storage + 16) == 0 && ShadowByte(storage + 32) == 0 && ShadowByte(storage + 40) == 0xfc &&
                                ShadowByte(storage + 56) == 0xfc,
                            "double-ended shrink");
                    Require(__sanitizer_verify_double_ended_contiguous_container(storage, storage + 16, storage + 40, storage + 64) == 1, "verify double-ended");
                    Require(__sanitizer_double_ended_contiguous_container_find_bad_address(storage, storage + 8, storage + 40, storage + 64) == storage + 8, "double-ended bad address");
                    __sanitizer_annotate_double_ended_contiguous_container(storage, storage + 64, storage + 16, storage + 40, storage + 8, storage + 45);
                    Require(ShadowByte(storage + 8) == 0 && ShadowByte(storage + 40) == 5 && ShadowByte(storage + 48) == 0xfc, "double-ended grow both ends");
                    __sanitizer_annotate_double_ended_contiguous_container(storage, storage + 64, storage + 8, storage + 45, storage + 48, storage + 56);
                    Require(ShadowByte(storage + 8) == 0xfc && ShadowByte(storage + 40) == 0xfc && ShadowByte(storage + 48) == 0, "double-ended disjoint move");
                }),
                "containers");
    Expect(Run(nullptr, [] {
               __sanitizer_annotate_contiguous_container(storage, storage + 32, storage + 32, storage + 10);
               __asan_load1(reinterpret_cast<std::uintptr_t>(storage + 12));
           }),
           1, {"container-overflow on address " + Hex(storage + 12), "HINT: if you don't care about these errors you may set ASAN_OPTIONS=detect_container_overflow=0.\n", "  Container overflow:      fc\n"},
           "container overflow");
    Expect(Run(nullptr, [] { __sanitizer_annotate_contiguous_container(storage + 1, storage + 32, storage + 40, storage + 10); }), 1,
           {"ERROR: AddressSanitizer: bad parameters to __sanitizer_annotate_contiguous_container:\n      beg     : " + Hex(storage + 1) + "\n", "      old_mid : " + Hex(storage + 40) + "\n",
            "ERROR: beg is not aligned by 8\n", "SUMMARY: AddressSanitizer: bad-__sanitizer_annotate_contiguous_container"},
           "bad container parameters");
    Expect(Run(nullptr, [] { __sanitizer_annotate_double_ended_contiguous_container(storage, storage + 32, storage + 8, storage + 4, storage, storage); }), 1,
           {"bad parameters to __sanitizer_annotate_double_ended_contiguous_container:\n      storage_beg        : " + Hex(storage) + "\n", "      old_container_end  : " + Hex(storage + 4) + "\n",
            "SUMMARY: AddressSanitizer: bad-__sanitizer_annotate_double_ended_contiguous_container"},
           "bad double-ended parameters");
    ExpectClean(Run("detect_container_overflow=0", [] {
                    __sanitizer_annotate_contiguous_container(storage, storage + 32, storage + 32, storage + 10);
                    Require(ShadowByte(storage + 16) == 0, "detect_container_overflow=0 skips annotations");
                    Require(__sanitizer_verify_contiguous_container(storage, storage + 20, storage + 32) == 1, "and verification");
                }),
                "containers off");
}

void Reports() {
    static std::string directory;
    directory = "/tmp/asan-report-" + std::to_string(::getpid());
    static std::string prefix;
    prefix = directory + "/nested/log";
    const auto outcome = Run(nullptr, [] {
        __sanitizer_set_report_path(prefix.c_str());
        const std::string path = __sanitizer_get_report_path();
        Require(path == prefix + "." + std::to_string(::getpid()), "report path names the file by pid");
        __asan_describe_address(reinterpret_cast<std::uintptr_t>(storage));
        __sanitizer_set_report_path("stderr");
        Require(std::strcmp(__sanitizer_get_report_path(), "") == 0, "stderr has no report path");
        __sanitizer_print_stack_trace();
        std::ifstream file(ResolvePath_nid_no_patch(path.c_str()));
        std::stringstream contents;
        contents << file.rdbuf();
        Require(contents.str() == "AddressSanitizer can not describe address in more detail (wild memory access suspected).\n", "the report went to the file");
    });
    std::error_code removed;
    std::filesystem::remove_all(ResolvePath_nid_no_patch(directory.c_str()), removed);
    Expect(outcome, 0, {"    #0 0x"}, "report path");
    ExpectAbsent(outcome, "wild memory access", "report path keeps stderr clean");
    ExpectClean(Run(nullptr, [] {
                    __sanitizer_ptr_cmp(storage, storage + 1);
                    __sanitizer_ptr_sub(storage, nullptr);
                    __sanitizer_sandbox_on_notify(nullptr);
                    __sanitizer_malloc_hook(storage, 1);
                    __sanitizer_free_hook(storage);
                }),
                "default hooks");
}

void ExpectAbort(const Outcome& outcome, const char* what) {
    if (!WIFSIGNALED(outcome.status) || WTERMSIG(outcome.status) != SIGABRT) Fail(outcome, what, "expected an abort");
}

}

int main() {
#if defined(ANYPS5_ENABLE_ASAN) || defined(__SANITIZE_ADDRESS__)
    std::fputs("skipped: the host AddressSanitizer shadow covers the PS guest shadow range at 1 << 40\n", stderr);
    return 77;
#endif
    HeapLayout();
    HeapReports();
    Reallocation();
    Alignment();
    Limits();
    Ownership();
    Statistics();
    Quarantine();
    Debugging();
    FakeStack();
    RealStack();
    Containers();
    Reports();
    std::puts("guest_asan_heap passed");
    return 0;
}
