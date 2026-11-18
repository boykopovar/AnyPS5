#include "prx/libScePsm/libScePsm.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

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

    std::puts("Psm Mono object tests passed");
    return 0;
}
