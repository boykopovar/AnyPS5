#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <bit>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

constexpr std::array<std::array<int, 3>, 33> Expected = {{
    {7, 3, 6},
    {8, 4, 6},
    {42, 41, 6},
    {65, 64, 6},
    {66, 41, 116},
    {70, 64, 67},
    {96, 95, 6},
    {104, 4, 19},
    {113, 64, 116},
    {115, 64, 19},
    {119, 117, 6},
    {396, 257, 6},
    {416, 64, 408},
    {668, 672, 6},
    {669, 673, 6},
    {670, 674, 6},
    {671, 675, 6},
    {791, 0, 408},
    {792, 0, 408},
    {793, 675, 408},
    {794, 672, 408},
    {795, 673, 408},
    {796, 674, 408},
    {802, 675, 116},
    {803, 672, 116},
    {807, 809, 811},
    {808, 809, 812},
    {852, 809, 850},
    {853, 809, 851},
    {912, 0, 912},
    {948, 941, 811},
    {949, 942, 811},
    {952, 0, 952},
}};

std::size_t checks = 0;
std::size_t cases = 0;
std::size_t customObjects = 0;
int currentNid = 0;

void Require(bool condition) {
    ++checks;
    if (!condition) {
        std::fprintf(stderr, "signature mapping check %zu failed in case %zu, NID %d\n", checks, cases, currentNid);
        std::abort();
    }
}

struct Guarded {
    std::uint32_t before = 0x91ab72c3;
    int value = -17019;
    std::uint32_t after = 0x48d05e2f;
};

void CheckGuard(const Guarded& output) {
    Require(output.before == 0x91ab72c3);
    Require(output.after == 0x48d05e2f);
}

void RunCase(int signatureNid) {
    ++cases;
    currentNid = signatureNid;
    const std::array<int, 3>* expected = nullptr;
    for (const auto& row : Expected)
        if (row[0] == signatureNid)
            expected = &row;
    const int status = expected ? 1 : 0;
    for (unsigned mask = 0; mask != 4; ++mask) {
        Guarded digest;
        Guarded key;
        Require(OBJ_find_sigid_algs_nid_postfix(signatureNid,
            mask & 1 ? &digest.value : nullptr, mask & 2 ? &key.value : nullptr) == status);
        Require(digest.value == (expected && (mask & 1) ? (*expected)[1] : -17019));
        Require(key.value == (expected && (mask & 2) ? (*expected)[2] : -17019));
        CheckGuard(digest);
        CheckGuard(key);
    }
    Guarded aliased;
    Require(OBJ_find_sigid_algs_nid_postfix(signatureNid, &aliased.value, &aliased.value) == status);
    Require(aliased.value == (expected ? (*expected)[2] : -17019));
    CheckGuard(aliased);
    Guarded first;
    Guarded second;
    Require(OBJ_find_sigid_algs_nid_postfix(signatureNid, &second.value, &first.value) == status);
    Require(first.value == (expected ? (*expected)[2] : -17019));
    Require(second.value == (expected ? (*expected)[1] : -17019));
    CheckGuard(first);
    CheckGuard(second);
}

void CreatedObjects() {
    constexpr std::array<int, 18> collidingIds = {
        980, 981, 982, 983, 984, 985, 986, 987, 988, 989, 1045, 1046, 1047, 1048, 1049, 1050, 1051, 1052
    };
    std::array<int, 100> allocated{};
    OBJ_cleanup_nid_postfix();
    ERR_clear_error_nid_postfix();
    Require(OBJ_create_nid_postfix("3.1", nullptr, nullptr) == 0);
    const auto error = ERR_peek_error_nid_postfix();
    Require(error != 0);
    for (std::size_t index = 0; index != allocated.size(); ++index) {
        std::array<char, 64> oid{};
        const int length = std::snprintf(oid.data(), oid.size(), "1.3.6.1.4.1.55555.%zu", index);
        Require(length > 0 && static_cast<std::size_t>(length) < oid.size());
        const char* shortName = index & 1 ? "customSignature" : nullptr;
        const char* longName = index & 2 ? "custom signature object" : nullptr;
        const int nid = OBJ_create_nid_postfix(oid.data(), shortName, longName);
        ++customObjects;
        allocated[index] = nid;
        Require(nid == 956 + static_cast<int>(index));
        RunCase(nid);
        for (const auto& row : Expected)
            RunCase(row[0]);
    }
    Require(allocated[24] == 980);
    for (int nid : collidingIds)
        RunCase(nid);
    Require(ERR_get_error_nid_postfix() == error);
    Require(ERR_get_error_nid_postfix() == 0);
    const int repeated = OBJ_create_nid_postfix("1.3.6.1.4.1.55555.24", nullptr, nullptr);
    ++customObjects;
    Require(repeated == allocated.back() + 1);
    RunCase(repeated);
    OBJ_cleanup_nid_postfix();
    for (int nid : allocated)
        RunCase(nid);
    for (int nid : collidingIds)
        RunCase(nid);
    for (const auto& row : Expected)
        RunCase(row[0]);
    const int afterCleanup = OBJ_create_nid_postfix("1.3.6.1.4.1.55555.24", nullptr, nullptr);
    ++customObjects;
    Require(afterCleanup == repeated + 1);
    RunCase(afterCleanup);
    for (int nid : collidingIds)
        RunCase(nid);
    Require(ERR_get_error_nid_postfix() == 0);
    OBJ_cleanup_nid_postfix();
}

class ReadOnlyPage {
public:
    explicit ReadOnlyPage(const void* initial = nullptr, std::size_t size = 0) {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        _size = info.dwPageSize != 0 ? info.dwPageSize : 4096u;
        _page = VirtualAlloc(nullptr, _size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
        Require(_page != nullptr);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        DWORD oldProtect = 0;
        Require(VirtualProtect(_page, _size, PAGE_READONLY, &oldProtect) != 0);
#else
        const long pageSize = sysconf(_SC_PAGESIZE);
        _size = pageSize > 0 ? static_cast<std::size_t>(pageSize) : 4096u;
        _page = mmap(nullptr, _size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(_page != MAP_FAILED);
        if (initial && size > 0) std::memcpy(_page, initial, size);
        Require(mprotect(_page, _size, PROT_READ) == 0);
#endif
    }

    ~ReadOnlyPage() {
        if (!_page) return;
#ifdef _WIN32
        VirtualFree(_page, 0, MEM_RELEASE);
#else
        munmap(_page, _size);
#endif
    }

    ReadOnlyPage(const ReadOnlyPage&) = delete;
    ReadOnlyPage& operator=(const ReadOnlyPage&) = delete;

    int* IntPtr() const { return static_cast<int*>(_page); }

private:
    void* _page = nullptr;
    std::size_t _size = 0;
};

void TestGuestOutputs() {
    const int initialValue = -17019;
    ReadOnlyPage roOut(&initialValue, sizeof(initialValue));
    errno = 73;
    Require(OBJ_find_sigid_algs_nid_postfix(999999, roOut.IntPtr(), roOut.IntPtr()) == 0);
    Require(errno == 73);

    Guarded digest;
    Guarded key;
    bool threwFirst = false;
    errno = 73;
    try { OBJ_find_sigid_algs_nid_postfix(668, roOut.IntPtr(), &key.value); }
    catch (const std::runtime_error&) { threwFirst = true; }
    Require(threwFirst && errno == 73 && key.value == -17019);
    CheckGuard(key);

    bool threwSecond = false;
    errno = 73;
    try { OBJ_find_sigid_algs_nid_postfix(668, &digest.value, roOut.IntPtr()); }
    catch (const std::runtime_error&) { threwSecond = true; }
    Require(threwSecond && errno == 73 && digest.value == -17019);
    CheckGuard(digest);
}

}

int main() {
    static_assert(sizeof(int) == sizeof(std::uint32_t));
    std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    CreatedObjects();
    for (const auto& row : Expected)
        RunCase(row[0]);
    for (int nid = -4096; nid <= 4096; ++nid)
        RunCase(nid);
    RunCase(std::numeric_limits<int>::min());
    RunCase(std::numeric_limits<int>::max());
    for (unsigned bit = 0; bit != 32; ++bit) {
        const std::uint32_t value = std::uint32_t{1} << bit;
        RunCase(std::bit_cast<int>(value));
        RunCase(std::bit_cast<int>(~value));
    }
    std::uint32_t state = 0x8e1b0537;
    for (unsigned i = 0; i != 4096; ++i) {
        state = state * 1664525u + 1013904223u;
        RunCase(std::bit_cast<int>(state));
    }
    TestGuestOutputs();
    std::printf("signature mappings: %zu cases, %zu checks, %zu mappings, %zu custom objects\n",
        cases, checks, Expected.size(), customObjects);
}
