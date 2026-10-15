#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>

struct Category { void* const* vtable; };
struct ErrorCode { int value; const Category* category; };
using Message = GuestLocale::String* (APS5_VABI *)(GuestLocale::String*, const Category*, int);
using Name = const char* (APS5_VABI *)(const Category*);
using Condition = ErrorCode (APS5_VABI *)(const Category*, int);

extern "C" {
const char* APS5_VABI _ZSt13_Syserror_mapi_nid_postfix(int value);
const Category* APS5_VABI _ZSt16generic_categoryv_nid_postfix();
GuestLocale::String* APS5_VABI _ZNKSt23_Generic_error_category7messageEi_nid_postfix(
    GuestLocale::String* result, const Category* self, int value);
int* APS5_VABI __error_nid_postfix();
void APS5_VABI _ZdlPv_nid_postfix(void* pointer);
}

static void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "generic category check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

static void CheckMessage(Message message, const Category* category, int value, const char* expected) {
    constexpr std::uint64_t marker = 0xA55AA55AA55AA55Aull;
    struct Guarded { std::uint64_t before; GuestLocale::String result; std::uint64_t after; };
    Guarded guarded {marker, {}, marker};
    std::memset(&guarded.result, 0xcd, sizeof(guarded.result));
    *__error_nid_postfix() = 61;
    REQUIRE(message(&guarded.result, category, value) == &guarded.result);
    REQUIRE(*__error_nid_postfix() == 61);
    REQUIRE(guarded.before == marker && guarded.after == marker);
    const auto length = std::strlen(expected);
    REQUIRE(guarded.result.reserved == 0 && guarded.result.size == length);
    REQUIRE(guarded.result.capacity >= length && (guarded.result.capacity & 15) == 15);
    const char* text = guarded.result.capacity <= 15 ? guarded.result.buffer : guarded.result.pointer;
    REQUIRE(std::memcmp(text, expected, length + 1) == 0);
    REQUIRE((guarded.result.capacity <= 15) == (length <= 15));
    if (length > 15) _ZdlPv_nid_postfix(guarded.result.pointer);
}

int main() {
    const std::array<void*, 10> api {};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const Category* category = _ZSt16generic_categoryv_nid_postfix();
    REQUIRE(category && category == _ZSt16generic_categoryv_nid_postfix());
    REQUIRE(std::strcmp(reinterpret_cast<Name>(category->vtable[2])(category), "generic") == 0);
    const auto message = reinterpret_cast<Message>(category->vtable[3]);
    const auto condition = reinterpret_cast<Condition>(category->vtable[4])(category, 35);
    REQUIRE(condition.value == 35 && condition.category == category);

    struct Expected { int value; const char* text; };
    const Expected expected[] {
        {3, "no such process"}, {14, "bad address"}, {13, "permission denied"},
        {25, "inappropriate io control operation"}, {35, "operation would block"},
        {45, "not supported"}, {47, "address family not supported"}, {60, "timed out"},
        {78, "function not supported"}, {95, "state not recoverable"}, {96, "owner dead"}
    };
    for (const auto& error : expected) {
        *__error_nid_postfix() = 61;
        const char* mapped = _ZSt13_Syserror_mapi_nid_postfix(error.value);
        REQUIRE(mapped && std::strcmp(mapped, error.text) == 0 && *__error_nid_postfix() == 61);
        CheckMessage(message, category, error.value, error.text);
        CheckMessage(_ZNKSt23_Generic_error_category7messageEi_nid_postfix, category, error.value, error.text);
    }

    unsigned known = 0;
    for (int value = 1; value <= 96; ++value) {
        const char* mapped = _ZSt13_Syserror_mapi_nid_postfix(value);
        if (mapped) ++known;
        CheckMessage(message, category, value, mapped ? mapped : "unknown error");
    }
    REQUIRE(known == 72);
    for (int value : {std::numeric_limits<int>::min(), -1, 0, 15, 44, 97, std::numeric_limits<int>::max()}) {
        *__error_nid_postfix() = 61;
        REQUIRE(_ZSt13_Syserror_mapi_nid_postfix(value) == nullptr && *__error_nid_postfix() == 61);
        CheckMessage(message, category, value, "unknown error");
    }
    GuestLocale::String first {}, second {};
    REQUIRE(message(&first, category, 25) == &first && message(&second, category, 11) == &second);
    REQUIRE(first.pointer != second.pointer);
    _ZdlPv_nid_postfix(second.pointer);
    REQUIRE(std::strcmp(first.pointer, "inappropriate io control operation") == 0);
    _ZdlPv_nid_postfix(first.pointer);
    bool rejected = false;
    try { message(nullptr, category, 22); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
    std::puts("guest generic category, FreeBSD error map and message ownership: PASS");
}
