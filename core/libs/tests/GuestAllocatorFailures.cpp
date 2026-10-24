#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <exception>
#include <limits>
#ifdef __linux__
#include <sys/resource.h>
#endif

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t);
void* APS5_VABI calloc_nid_postfix(std::size_t, std::size_t);
void* APS5_VABI realloc_nid_postfix(void*, std::size_t);
void* APS5_VABI memalign_nid_postfix(std::size_t, std::size_t);
void* APS5_VABI aligned_alloc_nid_postfix(std::size_t, std::size_t);
int APS5_VABI posix_memalign_nid_postfix(void**, std::size_t, std::size_t);
std::size_t APS5_VABI malloc_usable_size_nid_postfix(void*);
void APS5_VABI free_nid_postfix(void*);
int* APS5_VABI __error_nid_postfix();
}

namespace {

constexpr int NoMemory = 12;
constexpr int InvalidArgument = 22;
constexpr int Untouched = 77;
constexpr std::size_t Max = std::numeric_limits<std::size_t>::max();
constexpr std::size_t HalfPlusOne = static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max()) + 1;
constexpr std::size_t LastPlainFailure = Max - 0x1008f;

int failures = 0;

#ifdef __linux__
int overcommitMode() {
    int mode = -1;
    if (std::FILE* file = std::fopen("/proc/sys/vm/overcommit_memory", "r")) {
        if (std::fscanf(file, "%d", &mode) != 1) mode = -1;
        std::fclose(file);
    }
    return mode;
}
#endif

void report(const char* what, const char* detail) {
    std::fprintf(stderr, "%s: %s\n", what, detail);
    ++failures;
}

template<typename TCall>
void expectNull(const char* what, int expectedError, TCall call) {
    *__error_nid_postfix() = Untouched;
    void* result = nullptr;
    try {
        result = call();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: threw %s instead of returning NULL\n", what, error.what());
        ++failures;
        return;
    } catch (...) {
        report(what, "threw a non-standard exception instead of returning NULL");
        return;
    }
    if (result != nullptr) {
        report(what, "returned a block");
        free_nid_postfix(result);
        return;
    }
    if (*__error_nid_postfix() != expectedError) {
        std::fprintf(stderr, "%s: errno %d, expected %d\n", what, *__error_nid_postfix(), expectedError);
        ++failures;
    }
}

template<typename TCall>
void* expectBlock(const char* what, TCall call) {
    *__error_nid_postfix() = 77;
    void* result = nullptr;
    try {
        result = call();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s: threw %s\n", what, error.what());
        ++failures;
        return nullptr;
    }
    if (result == nullptr) report(what, "returned NULL");
    if (*__error_nid_postfix() != 77) report(what, "changed errno on success");
    return result;
}

}

int main() {
    void* api[10]{};
    ApplicationHeapRegister_nid_no_patch(api);

    expectNull("malloc(SIZE_MAX)", Untouched, [] { return malloc_nid_postfix(Max); });
    expectNull("malloc(SIZE_MAX - 0x1008e)", Untouched, [] { return malloc_nid_postfix(LastPlainFailure + 1); });
    expectNull("malloc(SIZE_MAX - 0x1008f)", InvalidArgument, [] { return malloc_nid_postfix(LastPlainFailure); });
    expectNull("malloc(PTRDIFF_MAX + 1)", InvalidArgument, [] { return malloc_nid_postfix(HalfPlusOne); });
    expectNull("calloc(SIZE_MAX, 2)", Untouched, [] { return calloc_nid_postfix(Max, 2); });
    expectNull("calloc(2^33, 2^33)", Untouched, [] { return calloc_nid_postfix(std::size_t{1} << 33, std::size_t{1} << 33); });
    expectNull("calloc(1, SIZE_MAX - 0x1008e)", Untouched, [] { return calloc_nid_postfix(1, LastPlainFailure + 1); });
    expectNull("calloc(PTRDIFF_MAX + 1, 1)", InvalidArgument, [] { return calloc_nid_postfix(HalfPlusOne, 1); });
    expectNull("realloc(NULL, SIZE_MAX)", Untouched, [] { return realloc_nid_postfix(nullptr, Max); });
    expectNull("realloc(NULL, PTRDIFF_MAX + 1)", InvalidArgument, [] { return realloc_nid_postfix(nullptr, HalfPlusOne); });
    expectNull("aligned_alloc(64, SIZE_MAX)", Untouched, [] { return aligned_alloc_nid_postfix(64, Max); });
    expectNull("aligned_alloc(16, PTRDIFF_MAX + 1)", InvalidArgument, [] { return aligned_alloc_nid_postfix(16, HalfPlusOne); });
    expectNull("memalign(64, PTRDIFF_MAX + 1)", InvalidArgument, [] { return memalign_nid_postfix(64, HalfPlusOne); });
    expectNull("memalign(4096, SIZE_MAX - 0x110af)", InvalidArgument, [] { return memalign_nid_postfix(4096, Max - 0x110af); });
    expectNull("memalign(4096, SIZE_MAX - 0x110ae)", Untouched, [] { return memalign_nid_postfix(4096, Max - 0x110ae); });
    expectNull("memalign(100, SIZE_MAX - 0x1012f)", InvalidArgument, [] { return memalign_nid_postfix(100, Max - 0x1012f); });
    expectNull("memalign(100, SIZE_MAX - 0x1012e)", Untouched, [] { return memalign_nid_postfix(100, Max - 0x1012e); });
    expectNull("memalign(3, SIZE_MAX - 0x1008f)", InvalidArgument, [] { return memalign_nid_postfix(3, LastPlainFailure); });
    expectNull("memalign(3, SIZE_MAX - 0x1008e)", Untouched, [] { return memalign_nid_postfix(3, LastPlainFailure + 1); });
    expectNull("memalign(2^63, 16)", Untouched, [] { return memalign_nid_postfix(HalfPlusOne, 16); });
    expectNull("memalign(SIZE_MAX, 16)", Untouched, [] { return memalign_nid_postfix(Max, 16); });
    expectNull("memalign(2^62 + 1, 16)", InvalidArgument, [] { return memalign_nid_postfix((std::size_t{1} << 62) + 1, 16); });
    expectNull("aligned_alloc(24, 64)", Untouched, [] { return aligned_alloc_nid_postfix(24, 64); });
    expectNull("aligned_alloc(0, 64)", Untouched, [] { return aligned_alloc_nid_postfix(0, 64); });
    expectNull("aligned_alloc(2^63, 16)", Untouched, [] { return aligned_alloc_nid_postfix(HalfPlusOne, 16); });

    struct RoundedAlignment {
        std::size_t requested;
        std::size_t guaranteed;
    };
    for (const RoundedAlignment& alignment : {RoundedAlignment{0, 32}, RoundedAlignment{3, 32}, RoundedAlignment{24, 32}, RoundedAlignment{48, 64},
             RoundedAlignment{100, 128}, RoundedAlignment{3000, 4096}}) {
        void* aligned = expectBlock("memalign with an alignment that is not a power of two", [&alignment] { return memalign_nid_postfix(alignment.requested, 40); });
        if (aligned != nullptr && reinterpret_cast<std::uintptr_t>(aligned) % alignment.guaranteed != 0) report("memalign", "did not round the alignment up to a power of two");
        free_nid_postfix(aligned);
    }

    auto* block = static_cast<unsigned char*>(expectBlock("malloc(40)", [] { return malloc_nid_postfix(40); }));
    if (block != nullptr) {
        std::memset(block, 0x6b, 40);
        expectNull("realloc(block, SIZE_MAX)", Untouched, [block] { return realloc_nid_postfix(block, Max); });
        expectNull("realloc(block, PTRDIFF_MAX + 1)", InvalidArgument, [block] { return realloc_nid_postfix(block, HalfPlusOne); });
        for (unsigned index = 0; index < 40; ++index) {
            if (block[index] != 0x6b) report("realloc failure", "damaged the original block");
        }
        void* emptied = expectBlock("realloc(block, 0)", [block] { return realloc_nid_postfix(block, 0); });
        if (emptied != block) report("realloc(block, 0)", "did not return the same block");
        if (emptied != nullptr && malloc_usable_size_nid_postfix(emptied) != 0) report("realloc(block, 0)", "left usable bytes in the block");
        void* regrown = expectBlock("realloc(emptied, 64)", [emptied] { return realloc_nid_postfix(emptied, 64); });
        if (regrown != nullptr && malloc_usable_size_nid_postfix(regrown) != 64) report("realloc(emptied, 64)", "did not hold 64 bytes");
        free_nid_postfix(regrown);
    }

#ifdef ANYPS5_ENABLE_ASAN
    std::fputs("skipped: AddressSanitizer cannot map the shadow of a 300 GiB commit under a 128 MiB RLIMIT_DATA, so the failed commit and arena release checks did not run\n", stderr);
#else
    if (GuestArena::GuestArenaAvailable_nid_postfix()) {
#ifdef __linux__
        rlimit originalLimit{};
        if (getrlimit(RLIMIT_DATA, &originalLimit) != 0) {
            report("failed commit setup", "could not read RLIMIT_DATA");
            return 1;
        }
        auto limited = originalLimit;
        limited.rlim_cur = std::min(limited.rlim_cur, rlim_t{128} << 20);
        if (setrlimit(RLIMIT_DATA, &limited) != 0) {
            report("failed commit setup", "could not limit RLIMIT_DATA");
            return 1;
        }
#endif
        constexpr std::size_t beyondCommitLimit = std::size_t{300} << 30;
        constexpr std::size_t probeBytes = std::size_t{100} << 30;
        void* before = GuestArena::GuestArenaAllocate_nid_postfix(probeBytes, 0x4000);
        GuestArena::GuestArenaRelease_nid_postfix(before, probeBytes);
        bool committed = false;
        int failedCommits = 0;
        for (int attempt = 0; attempt < 10 && !committed; ++attempt) {
            *__error_nid_postfix() = 0;
            void* huge = nullptr;
            try {
                huge = malloc_nid_postfix(beyondCommitLimit);
            } catch (const std::exception& error) {
                std::fprintf(stderr, "malloc(300 GiB): threw %s\n", error.what());
                ++failures;
                break;
            }
            if (huge != nullptr) {
                free_nid_postfix(huge);
                committed = true;
            } else if (*__error_nid_postfix() != InvalidArgument) {
                report("malloc(300 GiB)", "returned NULL without EINVAL");
            } else {
                ++failedCommits;
            }
        }
#ifdef __linux__
        if (setrlimit(RLIMIT_DATA, &originalLimit) != 0) report("failed commit cleanup", "could not restore RLIMIT_DATA");
#endif
        if (committed) {
#ifdef __linux__
            int mode = overcommitMode();
            if (mode == 1) {
                std::fprintf(stderr, "skipped: the host can commit 300 GiB (vm.overcommit_memory=%d), so the failed commit and arena release checks did not run\n", mode);
            } else {
                char detail[64];
                std::snprintf(detail, sizeof(detail), "committed 300 GiB with vm.overcommit_memory=%d", mode);
                report("malloc(300 GiB)", detail);
            }
#else
            std::fputs("skipped: the host can commit 300 GiB, so the failed commit and arena release checks did not run\n", stderr);
#endif
        } else {
            if (failedCommits != 10) report("failed heap commits", "did not return EINVAL for all ten attempts");
            void* after = nullptr;
            try {
                after = GuestArena::GuestArenaAllocate_nid_postfix(probeBytes, 0x4000);
                GuestArena::GuestArenaRelease_nid_postfix(after, probeBytes);
            } catch (const std::exception& error) {
                std::fprintf(stderr, "failed commit range probe: %s\n", error.what());
                ++failures;
            }
            if (after != before) report("malloc(300 GiB)", "left its arena range reserved after failing");
            std::fprintf(stderr, "Checked %d failed heap commits and arena range reuse\n", failedCommits);
        }
        void* small = malloc_nid_postfix(100);
        if (small == nullptr) report("malloc(100) after a failed huge malloc", "returned NULL");
        free_nid_postfix(small);
        void* large = malloc_nid_postfix(std::size_t{1} << 20);
        if (large == nullptr) report("malloc(1 MiB) after a failed huge malloc", "returned NULL");
        free_nid_postfix(large);
    }
#endif

    struct PosixCase {
        const char* what;
        std::size_t alignment;
        std::size_t size;
        int result;
        int error;
    };
    for (const PosixCase& check : {PosixCase{"posix_memalign(PTRDIFF_MAX + 1, 8)", HalfPlusOne, 8, NoMemory, Untouched},
             PosixCase{"posix_memalign(64, PTRDIFF_MAX + 1)", 64, HalfPlusOne, NoMemory, InvalidArgument},
             PosixCase{"posix_memalign(64, SIZE_MAX)", 64, Max, NoMemory, Untouched},
             PosixCase{"posix_memalign(64, SIZE_MAX - 0x100ef)", 64, Max - 0x100ef, NoMemory, InvalidArgument},
             PosixCase{"posix_memalign(64, SIZE_MAX - 0x100ee)", 64, Max - 0x100ee, NoMemory, Untouched},
             PosixCase{"posix_memalign(4, 8)", 4, 8, InvalidArgument, Untouched}, PosixCase{"posix_memalign(24, 8)", 24, 8, InvalidArgument, Untouched},
             PosixCase{"posix_memalign(PTRDIFF_MAX + 2, 8)", HalfPlusOne + 1, 8, InvalidArgument, Untouched}}) {
        void* untouched = &failures;
        *__error_nid_postfix() = Untouched;
        const int result = posix_memalign_nid_postfix(&untouched, check.alignment, check.size);
        if (result != check.result) report(check.what, "returned the wrong error number");
        if (*__error_nid_postfix() != check.error) report(check.what, "left the wrong errno");
        if (untouched != &failures) report(check.what, "touched the output");
    }

    free_nid_postfix(expectBlock("calloc(3, 5)", [] { return calloc_nid_postfix(3, 5); }));
    free_nid_postfix(expectBlock("calloc(0, 5)", [] { return calloc_nid_postfix(0, 5); }));
    free_nid_postfix(expectBlock("aligned_alloc(128, 1)", [] { return aligned_alloc_nid_postfix(128, 1); }));
    free_nid_postfix(expectBlock("memalign(4096, 1)", [] { return memalign_nid_postfix(4096, 1); }));
    void* first = expectBlock("malloc(0)", [] { return malloc_nid_postfix(0); });
    void* second = expectBlock("malloc(0)", [] { return malloc_nid_postfix(0); });
    if (first != nullptr && first == second) report("malloc(0)", "returned the same block twice");
    free_nid_postfix(first);
    free_nid_postfix(second);

    return failures == 0 ? 0 : 1;
}
