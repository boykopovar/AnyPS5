#include "prx/libc/include/Api.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Check(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "find-last check failed at line %d\n", line);
    std::abort();
}
#define Require(condition) Check((condition), __LINE__)

class GuardedPage {
    unsigned char* allocation;
public:
    std::size_t size;
    unsigned char* data;

    GuardedPage() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        allocation = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 3, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(allocation != nullptr);
        DWORD previous;
        Require(VirtualProtect(allocation + size, size, PAGE_READWRITE, &previous) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        Require(pageSize > 0);
        size = static_cast<std::size_t>(pageSize);
        allocation = static_cast<unsigned char*>(mmap(nullptr, size * 3, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(allocation != MAP_FAILED);
        Require(mprotect(allocation + size, size, PROT_READ | PROT_WRITE) == 0);
#endif
        data = allocation + size;
        std::memset(data, 0xff, size);
#ifdef _WIN32
        Require(VirtualProtect(data, size, PAGE_READONLY, &previous) != 0);
#else
        Require(mprotect(data, size, PROT_READ) == 0);
#endif
    }

    GuardedPage(const GuardedPage&) = delete;
    GuardedPage& operator=(const GuardedPage&) = delete;

    ~GuardedPage() {
#ifdef _WIN32
        VirtualFree(allocation, 0, MEM_RELEASE);
#else
        munmap(allocation, size * 3);
#endif
    }
};

template<typename TValue, typename TFunction>
static void Invoke(TFunction function, const void* first, const void* last, TValue value, const void* expected) {
    *__error_nid_postfix() = 97;
    Require(function(first, last, value) == expected);
    Require(*__error_nid_postfix() == 97);
}

template<typename TValue, typename TFunction>
static void Empty(TFunction function) {
    GuardedPage page;
    for (const void* address : {static_cast<const void*>(nullptr), static_cast<const void*>(page.data),
            static_cast<const void*>(page.data - 1), static_cast<const void*>(page.data + page.size)}) {
        for (const TValue value : {TValue{}, TValue{1}, std::numeric_limits<TValue>::max()}) {
            Invoke(function, address, address, value, address);
        }
    }
}

template<typename TValue, typename TFunction>
static void Positions(TFunction function) {
    std::array<unsigned char, 1088> storage;
    const TValue zero{}, maximum = std::numeric_limits<TValue>::max();
    for (std::size_t alignment = 0; alignment < 32; ++alignment) {
        auto* first = storage.data() + alignment;
        for (std::size_t count = 1; count <= 129; ++count) {
            auto* last = first + count * sizeof(TValue);
            storage.fill(0xff);
            Invoke(function, first, last, zero, last);
            Invoke(function, first, last, maximum, last - sizeof(TValue));
            for (std::size_t index = 0; index < count; ++index) {
                auto* match = first + index * sizeof(TValue);
                std::memcpy(match, &zero, sizeof(zero));
                const auto before = storage;
                Invoke(function, first, last, zero, match);
                Require(storage == before);
            }
            Invoke(function, first, last, maximum, last);
        }
    }
}

template<typename TValue, typename TFunction>
static void Values(TFunction function) {
    std::array<unsigned char, 544> storage{};
    std::uint64_t state = 0xac431bd5708ef269ULL;
    const auto checkValue = [&](TValue value) {
        for (std::size_t alignment = 0; alignment < 32; ++alignment) {
            auto* first = storage.data() + alignment;
            auto* last = first + 64 * sizeof(TValue);
            const TValue other = static_cast<TValue>(~value);
            for (std::size_t index = 0; index < 64; ++index) std::memcpy(first + index * sizeof(TValue), &other, sizeof(other));
            Invoke(function, first, last, value, last);
            for (const std::size_t index : {std::size_t{0}, std::size_t{15}, std::size_t{31}, std::size_t{63}}) {
                auto* match = first + index * sizeof(TValue);
                std::memcpy(match, &value, sizeof(value));
                const auto before = storage;
                Invoke(function, first, last, value, match);
                Require(storage == before);
            }
        }
    };
    if constexpr (sizeof(TValue) <= 2) {
        for (std::uint32_t value = 0; value <= std::numeric_limits<TValue>::max(); ++value) checkValue(static_cast<TValue>(value));
    } else {
        for (unsigned bit = 0; bit < sizeof(TValue) * 8; ++bit) {
            const auto value = static_cast<TValue>(TValue{1} << bit);
            checkValue(value);
            checkValue(static_cast<TValue>(~value));
        }
        for (unsigned index = 0; index < 1024; ++index) {
            state ^= state << 13;
            state ^= state >> 7;
            state ^= state << 17;
            checkValue(static_cast<TValue>(state));
        }
    }
}

template<typename TValue, typename TFunction>
static void GuardPages(TFunction function) {
    GuardedPage page;
    for (std::size_t count = 1; count <= page.size / sizeof(TValue); ++count) {
        for (const auto* first : {page.data, page.data + page.size - count * sizeof(TValue)}) {
            const auto* last = first + count * sizeof(TValue);
            Invoke(function, first, last, TValue{}, last);
            Invoke(function, first, last, std::numeric_limits<TValue>::max(), last - sizeof(TValue));
        }
    }
}

template<typename TValue, typename TFunction>
static void Run(TFunction function, const char* mode) {
    if (std::strcmp(mode, "empty") == 0) Empty<TValue>(function);
    else if (std::strcmp(mode, "positions") == 0) Positions<TValue>(function);
    else if (std::strcmp(mode, "values") == 0) Values<TValue>(function);
    else if (std::strcmp(mode, "guard_pages") == 0) GuardPages<TValue>(function);
    else Require(false);
}

int main(int argc, char** argv) {
    Require(argc == 3);
    if (std::strcmp(argv[1], "1") == 0) Run<std::uint8_t>(__std_find_last_trivial_1_nid_postfix, argv[2]);
    else if (std::strcmp(argv[1], "2") == 0) Run<std::uint16_t>(__std_find_last_trivial_2_nid_postfix, argv[2]);
    else if (std::strcmp(argv[1], "4") == 0) Run<std::uint32_t>(__std_find_last_trivial_4_nid_postfix, argv[2]);
    else if (std::strcmp(argv[1], "8") == 0) Run<std::uint64_t>(__std_find_last_trivial_8_nid_postfix, argv[2]);
    else Require(false);
}
