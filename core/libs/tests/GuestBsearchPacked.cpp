#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestErrno.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <source_location>
#include <stdexcept>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static std::size_t g_heapsortCompareCalls = 0;

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "check failed at line %u\n", location.line());
        std::abort();
    }
}

static int APS5_VABI Compare(const void* key, const void* record) {
    const auto left = *static_cast<const unsigned char*>(key);
    const auto right = *static_cast<const unsigned char*>(record);
    return (left > right) - (left < right);
}

static int APS5_VABI CountingNonDereferencingCompare(const void* left, const void* right) {
    ++g_heapsortCompareCalls;
    const auto a = reinterpret_cast<std::uintptr_t>(left);
    const auto b = reinterpret_cast<std::uintptr_t>(right);
    return (a > b) - (a < b);
}

static int APS5_VABI CompareUint32(const void* left, const void* right) {
    std::uint32_t a = 0;
    std::uint32_t b = 0;
    std::memcpy(&a, left, sizeof(a));
    std::memcpy(&b, right, sizeof(b));
    return (a > b) - (a < b);
}

int main() {
    std::array<void*, 10> heapApi{};
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    const std::array<unsigned char, 9> keys{1, 3, 3, 7, 9, 11, 13, 17, 21};
    for (std::size_t stride : {1, 3, 5, 7, 17}) {
        for (std::size_t offset : {0, 1, 2, 5}) {
            std::vector<unsigned char> records(offset + keys.size() * stride + 7, 0xa5);
            auto* base = records.data() + offset;
            for (std::size_t i = 0; i < keys.size(); ++i) base[i * stride] = keys[i];
            const auto before = records;
            for (unsigned char key = 0; key < 24; ++key) {
                auto* found = static_cast<unsigned char*>(bsearch_nid_postfix(&key, base, keys.size(), stride, Compare));
                const bool present = std::find(keys.begin(), keys.end(), key) != keys.end();
                Require((found != nullptr) == present);
                if (found) Require(found >= base && found < base + keys.size() * stride &&
                                   (found - base) % stride == 0 && *found == key);
                Require(records == before);
            }
        }
    }

    unsigned char sample = 7;
    Require(bsearch_nid_postfix(nullptr, nullptr, 0, 0, nullptr) == nullptr);
    Require(bsearch_nid_postfix(&sample, nullptr, 0, sizeof(sample), Compare) == nullptr);
    Require(bsearch_nid_postfix(&sample, keys.data(), 0, sizeof(unsigned char), Compare) == nullptr);

    auto expectInvalidArgument = [](const void* key, const void* base, std::size_t count, std::size_t size,
                                    int (APS5_VABI* compare)(const void*, const void*)) {
        try {
            bsearch_nid_postfix(key, base, count, size, compare);
            Require(false);
        } catch (const std::invalid_argument&) {
        }
    };
    expectInvalidArgument(nullptr, keys.data(), keys.size(), sizeof(unsigned char), Compare);
    expectInvalidArgument(&sample, nullptr, keys.size(), sizeof(unsigned char), Compare);
    expectInvalidArgument(&sample, keys.data(), keys.size(), 0, Compare);
    expectInvalidArgument(&sample, keys.data(), keys.size(), sizeof(unsigned char), nullptr);

    bool overflowed = false;
    try {
        bsearch_nid_postfix(&sample, keys.data(), (std::numeric_limits<std::size_t>::max() / 4) + 1, 4, Compare);
    } catch (const std::overflow_error&) {
        overflowed = true;
    }
    Require(overflowed);

    g_heapsortCompareCalls = 0;
    bool heapsortOverflowed = false;
    try {
        heapsort_nid_postfix(reinterpret_cast<void*>(UINTPTR_MAX - 3), 2, 4, CountingNonDereferencingCompare);
    } catch (const std::overflow_error&) {
        heapsortOverflowed = true;
    }
    Require(heapsortOverflowed);
    Require(g_heapsortCompareCalls == 0);

    *__error_nid_postfix() = 1234;
    g_heapsortCompareCalls = 0;
    bool heapsortRejectedUnmapped = false;
    try {
        heapsort_nid_postfix(reinterpret_cast<void*>(0x10), 2, 4, CountingNonDereferencingCompare);
    } catch (const std::invalid_argument&) {
        heapsortRejectedUnmapped = true;
    }
    Require(heapsortRejectedUnmapped);
    Require(g_heapsortCompareCalls == 0);
    Require(*__error_nid_postfix() == 1234);

    std::array<std::uint32_t, 2> zeroSizeInput{9, 1};
    *__error_nid_postfix() = 0;
    Require(heapsort_nid_postfix(zeroSizeInput.data(), 2, 0, CompareUint32) == -1);
    Require(*__error_nid_postfix() == 22);

    std::array<std::uint32_t, 7> sortValues{42, 7, 99, 7, 0, 123456, 15};
    Require(heapsort_nid_postfix(sortValues.data(), sortValues.size(), sizeof(std::uint32_t), CompareUint32) == 0);
    Require((sortValues == std::array<std::uint32_t, 7>{0, 7, 7, 15, 42, 99, 123456}));

    constexpr std::size_t pageSize = 4096;
#ifdef _WIN32
    auto* guarded = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(guarded != nullptr);
    DWORD oldProtect = 0;
    Require(VirtualProtect(guarded + pageSize, pageSize, PAGE_NOACCESS, &oldProtect) != 0);
#else
    void* mappedGuarded = mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    Require(mappedGuarded != MAP_FAILED);
    auto* guarded = static_cast<unsigned char*>(mappedGuarded);
    Require(mprotect(guarded + pageSize, pageSize, PROT_NONE) == 0);
#endif
    *__error_nid_postfix() = 1234;
    g_heapsortCompareCalls = 0;
    bool rejectedCrossPage = false;
    try {
        heapsort_nid_postfix(guarded + pageSize - 4, 2, 4, CountingNonDereferencingCompare);
    } catch (const std::invalid_argument&) {
        rejectedCrossPage = true;
    }
    Require(rejectedCrossPage);
    Require(g_heapsortCompareCalls == 0);
    Require(*__error_nid_postfix() == 1234);

#ifdef _WIN32
    Require(VirtualProtect(guarded, pageSize, PAGE_READONLY, &oldProtect) != 0);
#else
    Require(mprotect(guarded, pageSize, PROT_READ) == 0);
#endif
    *__error_nid_postfix() = 1234;
    g_heapsortCompareCalls = 0;
    bool rejectedReadOnly = false;
    try {
        heapsort_nid_postfix(guarded, 2, 4, CountingNonDereferencingCompare);
    } catch (const std::invalid_argument&) {
        rejectedReadOnly = true;
    }
    Require(rejectedReadOnly);
    Require(g_heapsortCompareCalls == 0);
    Require(*__error_nid_postfix() == 1234);

#ifdef _WIN32
    Require(VirtualFree(guarded, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(guarded, pageSize * 2) == 0);
#endif
}
