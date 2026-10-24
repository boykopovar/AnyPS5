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
struct ErrorCode { int reserved; int value; const Category* category; };
using Message = GuestLocale::String* (APS5_VABI *)(GuestLocale::String*, const Category*, int);
using Name = const char* (APS5_VABI *)(const Category*);
using Condition = ErrorCode (APS5_VABI *)(const Category*, int);
using EquivalentCondition = bool (APS5_VABI *)(const Category*, int, const ErrorCode*);
using EquivalentCode = bool (APS5_VABI *)(const Category*, const ErrorCode*, int);
using Delete = void (APS5_VABI *)(const Category*);
struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

extern "C" {
const char* APS5_VABI _ZSt13_Syserror_mapi_nid_postfix(int value);
const Category* APS5_VABI _ZSt16generic_categoryv_nid_postfix();
const Category* APS5_VABI _ZSt15system_categoryv_nid_postfix();
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

static void CheckSystemCategory(const Category* generic) {
    const Category* system = _ZSt15system_categoryv_nid_postfix();
    REQUIRE(system && system != generic && system == _ZSt15system_categoryv_nid_postfix());
    const auto* type = static_cast<const TypeRecord*>(system->vtable[-1]);
    REQUIRE(std::strcmp(type->name, "St22_System_error_category") == 0);
    REQUIRE(type->base && std::strcmp(type->base->name, "St14error_category") == 0);
    REQUIRE(std::strcmp(reinterpret_cast<Name>(system->vtable[2])(system), "system") == 0);
    const auto condition = reinterpret_cast<Condition>(system->vtable[4]);
    const ErrorCode known = condition(system, 2);
    std::uint64_t head = 0;
    std::memcpy(&head, &known, sizeof(head));
    REQUIRE(head == std::uint64_t{2} << 32 && known.value == 2 && known.category == generic);
    const ErrorCode unknown = condition(system, 44);
    REQUIRE(unknown.reserved == 0 && unknown.value == 44 && unknown.category == system);
    const auto equivalentCondition = reinterpret_cast<EquivalentCondition>(system->vtable[5]);
    REQUIRE(equivalentCondition(system, 2, &known));
    const ErrorCode sameCategory {0, 2, system};
    REQUIRE(!equivalentCondition(system, 2, &sameCategory));
    REQUIRE(equivalentCondition(system, 44, &unknown));
    const auto equivalentCode = reinterpret_cast<EquivalentCode>(system->vtable[6]);
    REQUIRE(equivalentCode(system, &sameCategory, 2) && !equivalentCode(system, &known, 2));
    const auto message = reinterpret_cast<Message>(system->vtable[3]);
    CheckMessage(message, system, 2, "no such file or directory");
    CheckMessage(message, system, 44, "unknown error");
    bool rejected = false;
    try { reinterpret_cast<Delete>(system->vtable[1])(system); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

int main() {
    const std::array<void*, 10> api {};
    ApplicationHeapRegister_nid_no_patch(api.data());
    const Category* category = _ZSt16generic_categoryv_nid_postfix();
    REQUIRE(category && category == _ZSt16generic_categoryv_nid_postfix());
    REQUIRE(std::strcmp(reinterpret_cast<Name>(category->vtable[2])(category), "generic") == 0);
    const auto message = reinterpret_cast<Message>(category->vtable[3]);
    const auto condition = reinterpret_cast<Condition>(category->vtable[4])(category, 35);
    REQUIRE(condition.reserved == 0 && condition.value == 35 && condition.category == category);
    const auto equivalent = reinterpret_cast<EquivalentCondition>(category->vtable[5]);
    REQUIRE(equivalent(category, 35, &condition));
    CheckSystemCategory(category);

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
    std::puts("guest generic and system categories, FreeBSD error map and message ownership: PASS");
}
