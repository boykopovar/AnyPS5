#include "prx/libSceZlib/include/Api.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <span>
#include <string>
#include <vector>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

static void Require(bool value, std::source_location location = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "boundary check failed at line %u\n", location.line());
        std::abort();
    }
}

class Guarded {
    unsigned char* memory;
    std::size_t page;
    std::size_t writable;
public:
    explicit Guarded(std::size_t capacity = 1) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        page = info.dwPageSize;
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0);
        page = static_cast<std::size_t>(size);
#endif
        writable = ((capacity + 128 + page - 1) / page) * page;
#ifdef _WIN32
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, writable + page * 2, MEM_RESERVE | MEM_COMMIT, PAGE_NOACCESS));
        Require(memory != nullptr);
#else
        memory = static_cast<unsigned char*>(mmap(nullptr, writable + page * 2, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED);
#endif
        Reset();
    }
    ~Guarded() {
#ifdef _WIN32
        Require(VirtualFree(memory, 0, MEM_RELEASE) != 0);
#else
        Require(munmap(memory, writable + page * 2) == 0);
#endif
    }
    Guarded(const Guarded&) = delete;
    Guarded& operator=(const Guarded&) = delete;
    void Protect(bool readOnly) {
#ifdef _WIN32
        DWORD previous;
        Require(VirtualProtect(memory + page, writable, readOnly ? PAGE_READONLY : PAGE_READWRITE, &previous) != 0);
#else
        Require(mprotect(memory + page, writable, readOnly ? PROT_READ : PROT_READ | PROT_WRITE) == 0);
#endif
    }
    void Reset() {
        Protect(false);
        std::memset(memory + page, 0xa5, writable);
    }
    unsigned char* End(std::size_t count) {
        Require(count + 64 <= writable);
        return memory + page + writable - (std::max)(count, std::size_t{1});
    }
    void Prefix(const void* address) const {
        const auto* bytes = static_cast<const unsigned char*>(address);
        for (int index = 1; index <= 32; ++index) Require(bytes[-index] == 0xa5);
    }
};

static unsigned Reverse(unsigned value, unsigned count) {
    unsigned result = 0;
    for (unsigned i = 0; i < count; ++i) { result = (result << 1) | (value & 1); value >>= 1; }
    return result;
}
struct Bits {
    std::vector<unsigned char> data;
    unsigned used = 0;
    void Add(unsigned value, unsigned count) {
        for (unsigned i = 0; i < count; ++i) {
            if (used == 0) data.push_back(0);
            data.back() |= ((value >> i) & 1) << used;
            used = (used + 1) % 8;
        }
    }
};
static std::vector<unsigned char> Compressed(std::uint32_t distance, std::vector<unsigned char>& expected) {
    std::vector<unsigned char> result{0x78, 0x9c, 0};
    for (unsigned value : {distance, distance ^ 0xffff}) {
        result.push_back(value & 255);
        result.push_back(value >> 8);
    }
    for (std::uint32_t i = 0; i < distance; ++i) expected.push_back(static_cast<unsigned char>(i * 73 + i / 31));
    result.insert(result.end(), expected.begin(), expected.end());
    for (std::uint32_t i = 0; i < 258; ++i) expected.push_back(expected[i % distance]);
    constexpr std::array<unsigned, 30> bases{1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,1025,1537,2049,3073,4097,6145,8193,12289,16385,24577};
    constexpr std::array<unsigned, 30> extras{0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13};
    unsigned symbol = 0;
    while (symbol + 1 < bases.size() && bases[symbol + 1] <= distance) ++symbol;
    Bits bits;
    bits.Add(1, 1); bits.Add(1, 2);
    bits.Add(Reverse(0xc5, 8), 8);
    bits.Add(Reverse(symbol, 5), 5);
    bits.Add(distance - bases[symbol], extras[symbol]);
    bits.Add(0, 7);
    result.insert(result.end(), bits.data.begin(), bits.data.end());
    unsigned a = 1, b = 0;
    for (auto value : expected) { a = (a + value) % 65521; b = (b + a) % 65521; }
    const unsigned adler = (b << 16) | a;
    for (int shift : {24, 16, 8, 0}) result.push_back(static_cast<unsigned char>(adler >> shift));
    return result;
}
int main() {
    Require(sceZlibInitialize(nullptr, 0) == 0);
    std::size_t cases = 0;
    for (unsigned distance : {1,2,3,4,1024,16384,32767,32768}) {
        std::vector<unsigned char> expected;
        const auto compressed = Compressed(distance, expected);
        Guarded input(compressed.size());
        auto* source = input.End(compressed.size());
        std::copy(compressed.begin(), compressed.end(), source);
        input.Protect(true);
        const auto capacity = static_cast<std::uint32_t>((expected.size() + 2047) / 2048 * 2048);
        for (std::uint32_t size : {std::uint32_t{2048}, capacity, std::uint32_t{65536}}) {
            Guarded output(size);
            auto* destination = output.End(size);
            Require(reinterpret_cast<std::uintptr_t>(destination) % 2048 == 0);
            std::uint64_t id = 0, completed = 0;
            Require(sceZlibInflate(source, compressed.size(), destination, size, &id) == 0);
            std::uint32_t timeout = 1000000;
            Require(sceZlibWaitForDone(&completed, &timeout) == 0 && completed == id);
            std::uint32_t produced = 0;
            std::int32_t status = 1;
            Require(sceZlibGetResult(id, &produced, &status) == 0);
            Require(produced <= size);
            if (size >= expected.size()) {
                Require(status == 0 && produced == expected.size());
                Require(std::equal(expected.begin(), expected.end(), destination));
            } else {
                Require(status != 0);
                Require(std::equal(destination, destination + produced, expected.begin()));
            }
            output.Prefix(destination);
            Require(std::equal(compressed.begin(), compressed.end(), source));
            ++cases;
        }
    }
    Require(sceZlibFinalize() == 0);
    std::printf("%zu maximum-distance/overlap and capacity cases passed\n", cases);
}
