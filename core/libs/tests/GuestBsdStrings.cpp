#include "prx/libc/include/general/VabiMacros.hpp"

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

extern "C" {
void APS5_VABI bcopy_nid_postfix(const void*, void*, std::size_t);
char* APS5_VABI index_nid_postfix(const char*, int);
char* APS5_VABI rindex_nid_postfix(const char*, int);
void* APS5_VABI memrchr_nid_postfix(const void*, int, std::size_t);
std::size_t APS5_VABI wcscspn_nid_postfix(const char16_t*, const char16_t*);
int APS5_VABI flsl_nid_postfix(std::int64_t);
}

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Guest BSD strings: %s\n", message);
        std::abort();
    }
}

class GuardedBuffer {
public:
    GuardedBuffer() {
#ifdef _WIN32
        SYSTEM_INFO info;
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        memory = static_cast<unsigned char*>(VirtualAlloc(nullptr, 3 * pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_NOACCESS));
        Require(memory != nullptr, "allocate guarded pages");
        DWORD previous;
        Require(VirtualProtect(memory + pageSize, pageSize, PAGE_READWRITE, &previous) != 0, "enable middle page");
#else
        const auto size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "query page size");
        pageSize = static_cast<std::size_t>(size);
        memory = static_cast<unsigned char*>(mmap(nullptr, 3 * pageSize, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Require(memory != MAP_FAILED, "allocate guarded pages");
        Require(mprotect(memory + pageSize, pageSize, PROT_READ | PROT_WRITE) == 0, "enable middle page");
#endif
    }

    ~GuardedBuffer() {
#ifdef _WIN32
        VirtualFree(memory, 0, MEM_RELEASE);
#else
        munmap(memory, 3 * pageSize);
#endif
    }

    unsigned char* Begin() { return memory + pageSize; }
    unsigned char* End() { return memory + 2 * pageSize; }

private:
    unsigned char* memory = nullptr;
    std::size_t pageSize = 0;
};

void CheckCopy() {
    for (std::size_t source = 0; source < 16; ++source) {
        for (std::size_t destination = 0; destination < 16; ++destination) {
            for (std::size_t count = 0; count <= 16; ++count) {
                std::array<unsigned char, 32> actual;
                for (std::size_t index = 0; index < actual.size(); ++index) actual[index] = static_cast<unsigned char>(index);
                auto expected = actual;
                for (std::size_t index = 0; index < count; ++index) expected[destination + index] = actual[source + index];
                bcopy_nid_postfix(actual.data() + source, actual.data() + destination, count);
                Require(actual == expected, "bcopy overlap, argument order or canaries");
            }
        }
    }
}

void CheckReverseMemory() {
    unsigned char bytes[] = {0, 1, 0x80, 0xff, 1, 0, 0xff};
    for (std::size_t count = 0; count <= sizeof(bytes); ++count) {
        for (int value = -256; value < 512; ++value) {
            void* expected = nullptr;
            for (std::size_t index = 0; index < count; ++index) {
                if (bytes[index] == static_cast<unsigned char>(value)) expected = bytes + index;
            }
            Require(memrchr_nid_postfix(bytes, value, count) == expected, "memrchr last match, bound or byte conversion");
        }
    }
    GuardedBuffer guarded;
    auto* beginning = guarded.Begin();
    beginning[0] = 0x80;
    beginning[1] = 0xff;
    Require(memrchr_nid_postfix(beginning, 0x80, 2) == beginning, "memrchr beginning boundary");
    Require(memrchr_nid_postfix(beginning, 1, 2) == nullptr, "memrchr must not read before buffer");
    auto* ending = guarded.End() - 2;
    ending[0] = 0;
    ending[1] = 0xff;
    Require(memrchr_nid_postfix(ending, -1, 2) == ending + 1, "memrchr ending boundary");
    Require(memrchr_nid_postfix(ending, 1, 2) == nullptr, "memrchr must not read past buffer");
    Require(memrchr_nid_postfix(guarded.End(), 1, 0) == nullptr, "memrchr zero count must not read memory");
}

void CheckCharacterSearch() {
    const char text[] = {'a', static_cast<char>(0xff), 'b', 'a', static_cast<char>(0xff), 0};
    Require(index_nid_postfix(text, 'a') == text && rindex_nid_postfix(text, 'a') == text + 3, "index/rindex repeated character");
    Require(index_nid_postfix(text, -1) == text + 1 && rindex_nid_postfix(text, 0x1ff) == text + 4, "index/rindex character conversion");
    Require(index_nid_postfix(text, 0) == text + 5 && rindex_nid_postfix(text, 256) == text + 5, "index/rindex terminator");
    Require(index_nid_postfix(text, 'z') == nullptr && rindex_nid_postfix(text, 'z') == nullptr, "index/rindex missing character");
    const char empty[] = "";
    Require(index_nid_postfix(empty, 0) == empty && rindex_nid_postfix(empty, 0) == empty, "index/rindex empty terminator");
    GuardedBuffer guarded;
    auto* last = reinterpret_cast<char*>(guarded.End() - 1);
    *last = 0;
    Require(index_nid_postfix(last, 'a') == nullptr && rindex_nid_postfix(last, 'a') == nullptr, "index/rindex ending boundary");
}

void CheckWideSpan() {
    const char16_t text[] = {u'A', 0x03a9, 0xd83d, 0xde00, u'Z', 0, u'X'};
    Require(wcscspn_nid_postfix(text, u"") == 5, "wcscspn counts 16-bit units");
    Require(wcscspn_nid_postfix(text, u"Z") == 4, "wcscspn final unit");
    Require(wcscspn_nid_postfix(text, u"\u03a9") == 1, "wcscspn non-ASCII unit");
    const char16_t surrogate[] = {0xde00, 0};
    Require(wcscspn_nid_postfix(text, surrogate) == 3, "wcscspn surrogate unit");
    Require(wcscspn_nid_postfix(text, u"XA") == 0, "wcscspn first unit");
    Require(wcscspn_nid_postfix(text, u"X") == 5, "wcscspn stops at text terminator");
    const char16_t rejected[] = {u'X', 0, u'A'};
    Require(wcscspn_nid_postfix(text, rejected) == 5, "wcscspn stops at rejected terminator");
    Require(wcscspn_nid_postfix(u"", u"A") == 0, "wcscspn empty text");
    GuardedBuffer guarded;
    auto* suffix = reinterpret_cast<char16_t*>(guarded.End() - 3 * sizeof(char16_t));
    suffix[0] = 0x03a9;
    suffix[1] = u'B';
    suffix[2] = 0;
    Require(wcscspn_nid_postfix(suffix, u"X") == 2, "wcscspn text ending boundary");
    Require(wcscspn_nid_postfix(u"ABC", suffix) == 1, "wcscspn rejected ending boundary");
}

void CheckLastBit() {
    Require(flsl_nid_postfix(0) == 0, "flsl zero");
    for (int bit = 0; bit < 64; ++bit) {
        const auto mask = UINT64_C(1) << bit;
        Require(flsl_nid_postfix(std::bit_cast<std::int64_t>(mask)) == bit + 1, "flsl full 64-bit input");
        Require(flsl_nid_postfix(std::bit_cast<std::int64_t>(mask | (mask - 1))) == bit + 1, "flsl highest bit among lower bits");
    }
    Require(flsl_nid_postfix(-1) == 64 && flsl_nid_postfix(std::numeric_limits<std::int64_t>::max()) == 63, "flsl signed limits");
}

}

int main() {
    CheckCopy();
    CheckReverseMemory();
    CheckCharacterSearch();
    CheckWideSpan();
    CheckLastBit();
    return 0;
}
