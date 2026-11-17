#include "prx/libSceLibcInternal/include/VectorAlgorithms.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>
#include <xmmintrin.h>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool value, std::source_location location = std::source_location::current()) {
    if (value) return;
    std::fprintf(stderr, "native range extrema: assertion at line %u\n", location.line());
    std::abort();
}

template<typename TValue>
struct Api;

template<>
struct Api<float> {
    static MinMaxFloat Pair(const void* first, const void* last) { return __std_minmax_f_nid_postfix(first, last); }
    static const void* Min(const void* first, const void* last, bool flag) { return __std_min_element_f_nid_postfix(first, last, flag); }
    static const void* Max(const void* first, const void* last, bool flag) { return __std_max_element_f_nid_postfix(first, last, flag); }
    static MinMaxPointers Elements(const void* first, const void* last, bool flag) { return __std_minmax_element_f_nid_postfix(first, last, flag); }
};

template<>
struct Api<double> {
    static MinMaxDouble Pair(const void* first, const void* last) { return __std_minmax_d_nid_postfix(first, last); }
    static const void* Min(const void* first, const void* last, bool flag) { return __std_min_element_d_nid_postfix(first, last, flag); }
    static const void* Max(const void* first, const void* last, bool flag) { return __std_max_element_d_nid_postfix(first, last, flag); }
    static MinMaxPointers Elements(const void* first, const void* last, bool flag) { return __std_minmax_element_d_nid_postfix(first, last, flag); }
};

class Fingerprint {
public:
    std::uint64_t value = 14695981039346656037ULL;

    void Add(std::uint64_t item) {
        for (unsigned byte = 0; byte < 8; ++byte) {
            value = (value ^ static_cast<unsigned char>(item)) * 1099511628211ULL;
            item >>= 8;
        }
    }
};

struct Expected {
    const char* name;
    unsigned operation;
    bool wide;
    std::uint64_t fingerprint;
    unsigned rows;
    unsigned unresolved;
};

constexpr Expected expectations[] = {
    {"max-d", 2, true, 0x6c7ef5d89c7d6e28ULL, 7174, 0},
    {"max-f", 2, false, 0x8d28cffaaf485e08ULL, 7174, 652},
    {"min-d", 1, true, 0xd6901195437477b4ULL, 7174, 0},
    {"min-f", 1, false, 0xf5e50ccd16444a24ULL, 7174, 638},
    {"pair-d", 0, true, 0xb5f40c7234420398ULL, 3586, 0},
    {"elements-d", 3, true, 0xe0d612f46edda914ULL, 7174, 544},
    {"elements-f", 3, false, 0x1447cbc227cfc004ULL, 7174, 1006},
    {"pair-f", 0, false, 0xce7c5b51bcd26e05ULL, 3586, 0},
};

template<typename TValue>
void CheckCorpus(const Expected& expected) {
    using Bits = std::conditional_t<sizeof(TValue) == 4, std::uint32_t, std::uint64_t>;
    const auto saved = _mm_getcsr();
    Fingerprint fingerprint;
    unsigned rows = 0;
    unsigned unresolved = 0;
    const auto emit = [&](const std::vector<Bits>& values, std::size_t alignment, unsigned control = 0x1f80) {
        std::vector<unsigned char> storage(values.size() * sizeof(Bits) + alignment);
        auto* first = storage.data() + alignment;
        if (!values.empty()) std::memcpy(first, values.data(), values.size() * sizeof(Bits));
        const void* last = first + values.size() * sizeof(Bits);
        for (bool flag : {false, true}) {
            if (expected.operation == 0 && flag) continue;
            fingerprint.Add(values.size());
            fingerprint.Add(alignment);
            fingerprint.Add(flag);
            fingerprint.Add(control);
            for (const auto value : values) fingerprint.Add(value);
            std::array<std::uint64_t, 2> result{};
            unsigned count = 0;
            unsigned status = 0;
            bool failed = false;
            try {
                _mm_setcsr(control);
                if (expected.operation == 0) {
                    const auto found = Api<TValue>::Pair(first, last);
                    status = _mm_getcsr();
                    result = {std::bit_cast<Bits>(found.minimum), std::bit_cast<Bits>(found.maximum)};
                    count = 2;
                } else if (expected.operation == 3) {
                    const auto found = Api<TValue>::Elements(first, last, flag);
                    status = _mm_getcsr();
                    result = {reinterpret_cast<std::uintptr_t>(found.minimum) - reinterpret_cast<std::uintptr_t>(first),
                              reinterpret_cast<std::uintptr_t>(found.maximum) - reinterpret_cast<std::uintptr_t>(first)};
                    count = 2;
                } else {
                    const auto found = expected.operation == 1 ? Api<TValue>::Min(first, last, flag) : Api<TValue>::Max(first, last, flag);
                    status = _mm_getcsr();
                    result[0] = reinterpret_cast<std::uintptr_t>(found) - reinterpret_cast<std::uintptr_t>(first);
                    count = 1;
                }
            } catch (const std::runtime_error& error) {
                status = _mm_getcsr();
                Require(std::string(error.what()).ends_with(" not implemented"));
                failed = true;
            }
            _mm_setcsr(0x1f80);
            Require((status & ~0x3fU) == (control & ~0x3fU));
            fingerprint.Add(failed);
            if (failed) ++unresolved;
            else {
                for (unsigned i = 0; i < count; ++i) fingerprint.Add(result[i]);
                fingerprint.Add(status);
            }
            ++rows;
        }
    };
    if (expected.operation) emit({}, 1);
    const Bits sign = Bits{1} << (sizeof(Bits) * 8 - 1);
    for (std::size_t count = 1; count <= 33; ++count) {
        for (std::size_t position = 0; position < count; ++position) {
            for (Bits fill : {Bits{0}, sign}) {
                std::vector<Bits> values(count, fill);
                values[position] = fill ^ sign;
                emit(values, 0);
            }
        }
    }
    std::vector<Bits> special;
    if constexpr (sizeof(Bits) == 4) {
        special = {0x7f800000, 0xff800000, 0x00000001, 0x80000001, 0x7fc12345, 0xffc54321, 0x7f812345, 0xff854321};
    } else {
        special = {0x7ff0000000000000, 0xfff0000000000000, 0x0000000000000001, 0x8000000000000001,
                   0x7ff8123456789abc, 0xfff8abcdef012345, 0x7ff0123456789abc, 0xfff023456789abcd};
    }
    for (std::size_t count : {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33}) {
        for (std::size_t position = 0; position < count; ++position) {
            for (Bits value : special) {
                std::vector<Bits> values;
                for (std::size_t index = 0; index < count; ++index) {
                    values.push_back(std::bit_cast<Bits>(static_cast<TValue>(static_cast<int>(index % 9) - 4)));
                }
                values[position] = value;
                emit(values, position % sizeof(Bits));
            }
        }
    }
    special.push_back(0);
    special.push_back(sign);
    special.push_back(std::bit_cast<Bits>(TValue{1}));
    special.push_back(std::bit_cast<Bits>(TValue{-1}));
    std::uint64_t random = 0x9e3779b97f4a7c15;
    for (std::size_t sample = 0; sample < 200; ++sample) {
        std::vector<Bits> values;
        const std::size_t count = 1 + sample % 65;
        for (std::size_t index = 0; index < count; ++index) {
            random ^= random << 13;
            random ^= random >> 7;
            random ^= random << 17;
            values.push_back(special[random % special.size()]);
        }
        for (unsigned control : {0x1f80u, 0x1fc0u, 0x9f80u, 0x9fc0u, 0x1fbfu}) emit(values, sample % 16, control);
    }
    _mm_setcsr(saved);
    std::fprintf(stderr, "%s: %u records, %u unresolved, fingerprint %016llx\n", expected.name, rows, unresolved,
                 static_cast<unsigned long long>(fingerprint.value));
    Require(rows == expected.rows && unresolved == expected.unresolved && fingerprint.value == expected.fingerprint);
}

class Guarded {
public:
    unsigned char* data;
    std::size_t size;

    Guarded() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        size = info.dwPageSize;
        data = static_cast<unsigned char*>(VirtualAlloc(nullptr, size * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(data != nullptr);
        DWORD previous;
        Require(VirtualProtect(data + size, size, PAGE_NOACCESS, &previous) != 0);
#else
        const auto page = sysconf(_SC_PAGESIZE);
        Require(page > 0);
        size = static_cast<std::size_t>(page);
        auto* mapping = mmap(nullptr, size * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapping != MAP_FAILED);
        data = static_cast<unsigned char*>(mapping);
        Require(mprotect(data + size, size, PROT_NONE) == 0);
#endif
    }

    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(data, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(data, size * 2) == 0);
#endif
    }

    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
};

template<typename TValue>
void CheckGuarded() {
    Guarded memory;
    for (std::size_t count = 1; count <= 65; ++count) {
        auto* first = memory.data + memory.size - count * sizeof(TValue);
        for (std::size_t i = 0; i < count; ++i) {
            const auto value = static_cast<TValue>(i + 1);
            std::memcpy(first + i * sizeof(TValue), &value, sizeof(value));
        }
        const auto* last = first + count * sizeof(TValue);
        const auto pair = Api<TValue>::Pair(first, last);
        Require(pair.minimum == TValue{1} && pair.maximum == static_cast<TValue>(count));
        for (bool flag : {false, true}) {
            Require(Api<TValue>::Min(first, last, flag) == first);
            Require(Api<TValue>::Max(first, last, flag) == last - sizeof(TValue));
            const auto elements = Api<TValue>::Elements(first, last, flag);
            Require(elements.minimum == first && elements.maximum == last - sizeof(TValue));
        }
    }
}

}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    if (std::string(argv[1]) == "guarded") {
        CheckGuarded<float>();
        CheckGuarded<double>();
        return 0;
    }
    for (const auto& expected : expectations) {
        if (std::string(argv[1]) != expected.name) continue;
        if (expected.wide) CheckCorpus<double>(expected);
        else CheckCorpus<float>(expected);
        return 0;
    }
    return 2;
}
