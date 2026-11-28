#include "prx/libScePsm/libScePsm.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "PSM MONO OBJECTS: %s\n", message);
    std::abort();
}

struct alignas(8) TestString {
    void* vtable;
    void* synchronisation;
    std::int32_t length;
    std::uint16_t chars[8];
};

struct alignas(8) TestArray {
    void* vtable;
    void* synchronisation;
    void* bounds;
    std::uintptr_t maxLength;
    std::uint64_t vector[4];
};

void RequireUtf8(const TestString& string, const char* expected, const char* message) {
    char* utf8 = ScePsmMonoStringToUtf8(const_cast<TestString*>(&string));
    Require(utf8 != nullptr && std::strcmp(utf8, expected) == 0, message);
    ScePsmMonoFree(utf8);
}

}

int main() {
    TestString string{nullptr, nullptr, 4, {u'P', 0x00E9, 0xD83D, 0xDE00}};
    Require(ScePsmMonoStringLength(&string) == 4, "string length");
    Require(ScePsmMonoStringChars(&string) == string.chars, "string chars");
    RequireUtf8(string, "P\xC3\xA9\xF0\x9F\x98\x80", "utf8 conversion");

    TestString empty{nullptr, nullptr, 0, {}};
    RequireUtf8(empty, "", "empty string");
    TestString embeddedNul{nullptr, nullptr, 3, {u'a', 0, 0xDC00}};
    RequireUtf8(embeddedNul, "a", "conversion stops at NUL");
    TestString loneLow{nullptr, nullptr, 2, {u'a', 0xDC00}};
    Require(ScePsmMonoStringToUtf8(&loneLow) == nullptr, "lone low surrogate");
    TestString trailingHigh{nullptr, nullptr, 2, {u'a', 0xD800}};
    Require(ScePsmMonoStringToUtf8(&trailingHigh) == nullptr, "trailing high surrogate");
    TestString unpairedHigh{nullptr, nullptr, 2, {0xD800, u'a'}};
    Require(ScePsmMonoStringToUtf8(&unpairedHigh) == nullptr, "unpaired high surrogate");
    Require(ScePsmMonoStringToUtf8(nullptr) == nullptr, "null string");

    TestArray array{nullptr, nullptr, nullptr, 3, {}};
    Require(ScePsmMonoArrayLength(&array) == 3, "array length");
    Require(ScePsmMonoArrayAddrWithSize(&array, 4, 2) == reinterpret_cast<char*>(array.vector) + 8, "array element address");

    void* slot = nullptr;
    ScePsmMonoGcWbarrierGenericStore(&slot, &array);
    Require(slot == &array, "write barrier store");

    auto throwsInvalidArgument = [](auto&& fn) {
        try {
            fn();
            return false;
        } catch (const std::invalid_argument&) {
            return true;
        }
    };
    const auto* unmapped = reinterpret_cast<const void*>(std::uintptr_t{0x10});
    errno = 71;
    Require(throwsInvalidArgument([] { ScePsmMonoArrayLength(nullptr); }) && errno == 71, "null array throws");
    Require(throwsInvalidArgument([&] { ScePsmMonoArrayLength(unmapped); }) && errno == 71, "unreadable array throws");
    Require(throwsInvalidArgument([&] { ScePsmMonoGcWbarrierGenericStore(nullptr, &array); }) && errno == 71, "null slot throws");
    Require(throwsInvalidArgument([&] { ScePsmMonoGcWbarrierGenericStore(const_cast<void*>(unmapped), &array); }) && errno == 71, "unwritable slot throws");
    static void* const readOnlySlot = nullptr;
    Require(throwsInvalidArgument([&] { ScePsmMonoGcWbarrierGenericStore(const_cast<void**>(&readOnlySlot), &array); }) && errno == 71, "const slot throws");
    Require(readOnlySlot == nullptr, "const slot unchanged");
    Require(throwsInvalidArgument([] { ScePsmMonoStringLength(nullptr); }) && errno == 71, "null string length throws");
    Require(throwsInvalidArgument([&] { ScePsmMonoStringLength(unmapped); }) && errno == 71, "unreadable string length throws");
    Require(throwsInvalidArgument([&] { ScePsmMonoStringToUtf8(const_cast<void*>(unmapped)); }) && errno == 71, "unreadable StringToUtf8 throws");

    constexpr std::size_t longCount = 3000;
    std::vector<std::uint8_t> longBuf(0x14 + longCount * sizeof(std::uint16_t), 0);
    const std::int32_t longLen = static_cast<std::int32_t>(longCount);
    std::memcpy(longBuf.data() + 0x10, &longLen, sizeof(longLen));
    for (std::size_t i = 0; i < longCount; ++i) {
        const std::uint16_t ch = u'x';
        std::memcpy(longBuf.data() + 0x14 + i * sizeof(ch), &ch, sizeof(ch));
    }
    char* longUtf8 = ScePsmMonoStringToUtf8(longBuf.data());
    Require(longUtf8 != nullptr && std::strlen(longUtf8) == longCount && std::string(longUtf8) == std::string(longCount, 'x'), "chunked long StringToUtf8");
    ScePsmMonoFree(longUtf8);

#ifdef _WIN32
    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);
    const std::size_t pageSize = sysInfo.dwPageSize;
    auto* mapped = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(mapped != nullptr, "allocate mapped pages");
#else
    const std::size_t pageSize = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* mapped = static_cast<unsigned char*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(mapped != MAP_FAILED, "allocate mapped pages");
#endif
    unsigned char* crossSurrogate = mapped + pageSize - (0x14 + sizeof(std::uint16_t));
    const std::int32_t pairLen = 2;
    const std::uint16_t pairUnits[2] = {0xD83D, 0xDE00};
    std::memcpy(crossSurrogate + 0x10, &pairLen, sizeof(pairLen));
    std::memcpy(crossSurrogate + 0x14, pairUnits, sizeof(pairUnits));
    char* pairUtf8 = ScePsmMonoStringToUtf8(crossSurrogate);
    Require(pairUtf8 != nullptr && std::strcmp(pairUtf8, "\xF0\x9F\x98\x80") == 0, "surrogate pair crossing mapped page boundary");
    ScePsmMonoFree(pairUtf8);

    unsigned char* oddCross = mapped + pageSize - (0x14 + 3);
    const std::int32_t oddLen = 4;
    const std::uint16_t oddUnits[4] = {u'a', u'b', u'c', u'd'};
    std::memcpy(oddCross + 0x10, &oddLen, sizeof(oddLen));
    std::memcpy(oddCross + 0x14, oddUnits, sizeof(oddUnits));
    char* oddUtf8 = ScePsmMonoStringToUtf8(oddCross);
    Require(oddUtf8 != nullptr && std::strcmp(oddUtf8, "abcd") == 0, "odd-address string crossing mapped page boundary");
    ScePsmMonoFree(oddUtf8);
#ifdef _WIN32
    VirtualFree(mapped, 0, MEM_RELEASE);
#else
    munmap(mapped, pageSize * 2);
#endif

#ifdef _WIN32
    auto* pages = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 2, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
    Require(pages != nullptr, "allocate guarded pages");
    DWORD prevProt = 0;
    Require(VirtualProtect(pages + pageSize, pageSize, PAGE_NOACCESS, &prevProt) != 0, "protect trailing page");
#else
    auto* pages = static_cast<unsigned char*>(mmap(nullptr, pageSize * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    Require(pages != MAP_FAILED, "allocate guarded pages");
    Require(mprotect(pages + pageSize, pageSize, PROT_NONE) == 0, "protect trailing page");
#endif
    unsigned char* strTail = pages + pageSize - (0x14 + 4 * sizeof(std::uint16_t));
    const std::int32_t tailLen = 100;
    std::memcpy(strTail + 0x10, &tailLen, sizeof(tailLen));
    const std::uint16_t terminated[4] = {u'o', u'k', 0, u'!'};
    std::memcpy(strTail + 0x14, terminated, sizeof(terminated));
    char* tailUtf8 = ScePsmMonoStringToUtf8(strTail);
    Require(tailUtf8 != nullptr && std::strcmp(tailUtf8, "ok") == 0, "NUL before guarded page stops without faulting");
    ScePsmMonoFree(tailUtf8);
    const std::uint16_t unterminated[4] = {u'a', u'b', u'c', u'd'};
    std::memcpy(strTail + 0x14, unterminated, sizeof(unterminated));
    errno = 71;
    Require(throwsInvalidArgument([&] { ScePsmMonoStringToUtf8(strTail); }) && errno == 71, "StringToUtf8 spilling into guarded page throws");

    unsigned char* oddGuarded = pages + pageSize - (0x14 + 1);
    const std::int32_t oneLen = 1;
    std::memcpy(oddGuarded + 0x10, &oneLen, sizeof(oneLen));
    oddGuarded[0x14] = 'a';
    errno = 71;
    Require(throwsInvalidArgument([&] { ScePsmMonoStringToUtf8(oddGuarded); }) && errno == 71, "odd-address unit straddling guarded page throws");

    unsigned char* lonelyHigh = pages + pageSize - (0x14 + sizeof(std::uint16_t));
    const std::int32_t highLen = 10;
    const std::uint16_t highUnit = 0xD83D;
    std::memcpy(lonelyHigh + 0x10, &highLen, sizeof(highLen));
    std::memcpy(lonelyHigh + 0x14, &highUnit, sizeof(highUnit));
    errno = 71;
    Require(throwsInvalidArgument([&] { ScePsmMonoStringToUtf8(lonelyHigh); }) && errno == 71, "high surrogate at guarded page boundary throws");
#ifdef _WIN32
    VirtualFree(pages, 0, MEM_RELEASE);
#else
    munmap(pages, pageSize * 2);
#endif

    std::puts("Psm Mono object tests passed");
    return 0;
}
