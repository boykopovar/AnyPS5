#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>
#include <stdexcept>

using Message = GuestLocale::String* (APS5_VABI *)(GuestLocale::String*, const LibcException::CategoryObject*, int);
using Name = const char* (APS5_VABI *)(const LibcException::CategoryObject*);
using Raise = void (APS5_VABI *)(const LibcException::FutureErrorObject*);

bool failAllocation = false;

void* APS5_VABI TestAllocate(std::size_t size) { return failAllocation ? nullptr : std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct ClassTypeRecord { const void* vtable; const char* name; };

extern "C" {
extern TypeRecord _ZTISt12future_error_nid_postfix;
extern TypeRecord _ZTISt22_Future_error_category_nid_postfix;
extern ClassTypeRecord _ZTISt14error_category_nid_postfix;
extern std::uintptr_t _ZTVSt12future_error_nid_postfix[6];
}

static void Require(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "future category check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

static void CheckMessage(Message message, const LibcException::CategoryObject* category, int value, const char* expected) {
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

static void CheckBaseCategory() {
    REQUIRE(sizeof(ClassTypeRecord) == 0x10);
    REQUIRE(std::strcmp(_ZTISt14error_category_nid_postfix.name, "St14error_category") == 0);
    const auto* future = _ZSt15future_categoryv_nid_postfix();
    const auto* generic = _ZSt16generic_categoryv_nid_postfix();
    REQUIRE(future != nullptr && generic != nullptr && future != generic);

    const LibcException::ErrorCode condition {0x7fffffff, 22, future};
    REQUIRE(_ZNKSt14error_category10equivalentEiRKSt15error_condition_nid_postfix(future, 22, &condition));
    REQUIRE(!_ZNKSt14error_category10equivalentEiRKSt15error_condition_nid_postfix(future, 23, &condition));
    const LibcException::ErrorCode otherCondition {0, 22, generic};
    REQUIRE(!_ZNKSt14error_category10equivalentEiRKSt15error_condition_nid_postfix(future, 22, &otherCondition));

    const LibcException::ErrorCode code {0x12345678, 22, future};
    REQUIRE(_ZNKSt14error_category10equivalentERKSt10error_codei_nid_postfix(future, &code, 22));
    REQUIRE(!_ZNKSt14error_category10equivalentERKSt10error_codei_nid_postfix(future, &code, 23));
    const LibcException::ErrorCode otherCode {0, 22, generic};
    REQUIRE(!_ZNKSt14error_category10equivalentERKSt10error_codei_nid_postfix(future, &otherCode, 22));

    const auto mapped = _ZNKSt14error_category23default_error_conditionEi_nid_postfix(future, 7);
    REQUIRE(mapped.reserved == 0 && mapped.value == 7 && mapped.category == future);
    _ZNSt14error_categoryD2Ev_nid_postfix(nullptr);
}

static void CheckFutureCategory() {
    const auto* future = _ZSt15future_categoryv_nid_postfix();
    const auto* type = static_cast<const TypeRecord*>(static_cast<const void* const*>(future->vtable)[-1]);
    REQUIRE(std::strcmp(type->name, "St22_Future_error_category") == 0);
    REQUIRE(type->base && std::strcmp(type->base->name, "St23_Generic_error_category") == 0);
    REQUIRE(std::strcmp(_ZTISt22_Future_error_category_nid_postfix.name, "St22_Future_error_category") == 0);
    REQUIRE(_ZTISt22_Future_error_category_nid_postfix.base && std::strcmp(_ZTISt22_Future_error_category_nid_postfix.base->name, "St23_Generic_error_category") == 0);

    const auto name = std::bit_cast<Name>(static_cast<const void* const*>(future->vtable)[2]);
    REQUIRE(std::strcmp(name(future), "future") == 0);

    const auto message = std::bit_cast<Message>(static_cast<const void* const*>(future->vtable)[3]);
    struct Expected { int value; const char* text; };
    static constexpr Expected errors[] {
        {1, "broken promise"}, {2, "future already retrieved"}, {3, "promise already satisfied"}, {4, "no state"}
    };
    for (const auto& error : errors) CheckMessage(message, future, error.value, error.text);

    for (int value : {0, 5, 22, 99}) {
        const char* generic = _ZSt13_Syserror_mapi_nid_postfix(value);
        CheckMessage(message, future, value, generic ? generic : "unknown error");
    }

    GuestLocale::String first {}, second {};
    REQUIRE(message(&first, future, 2) == &first && message(&second, future, 3) == &second);
    REQUIRE(first.pointer != second.pointer);
    _ZdlPv_nid_postfix(second.pointer);
    REQUIRE(std::strcmp(first.pointer, "future already retrieved") == 0);
    _ZdlPv_nid_postfix(first.pointer);

    bool rejected = false;
    try { message(nullptr, future, 2); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

static void CheckFutureError() {
    REQUIRE(sizeof(LibcException::FutureErrorObject) == 0x40);
    REQUIRE(offsetof(LibcException::FutureErrorObject, message) == 0x8);
    REQUIRE(offsetof(LibcException::FutureErrorObject, code) == 0x30);
    REQUIRE(std::strcmp(_ZTISt12future_error_nid_postfix.name, "St12future_error") == 0);
    REQUIRE(_ZTISt12future_error_nid_postfix.base && std::strcmp(_ZTISt12future_error_nid_postfix.base->name, "St11logic_error") == 0);

    REQUIRE(_ZTVSt12future_error_nid_postfix[0] == 0);
    REQUIRE(_ZTVSt12future_error_nid_postfix[1] == reinterpret_cast<std::uintptr_t>(&_ZTISt12future_error_nid_postfix));
    for (int slot = 2; slot < 6; ++slot) REQUIRE(_ZTVSt12future_error_nid_postfix[slot] != 0);

    const auto* future = _ZSt15future_categoryv_nid_postfix();
    LibcException::ErrorCode code {0x12345678, 2, future};
    bool caught = false;
    try { _ZSt19_Throw_future_errorRKSt10error_code_nid_postfix(&code); }
    catch (const std::exception& error) {
        caught = true;
        REQUIRE(error.what() != nullptr && std::strcmp(error.what(), "future already retrieved") == 0);
    }
    REQUIRE(caught);

    code.value = 99;
    caught = false;
    try { _ZSt19_Throw_future_errorRKSt10error_code_nid_postfix(&code); }
    catch (const std::exception& error) {
        caught = true;
        REQUIRE(error.what() == nullptr);
    }
    REQUIRE(caught);

    const auto raise = std::bit_cast<Raise>(_ZTVSt12future_error_nid_postfix[5]);
    LibcException::FutureErrorObject source {};
    source.message.capacity = 24;
    source.message.size = 24;
    source.message.pointer = static_cast<char*>(_Znwm_nid_postfix(25));
    std::memcpy(source.message.pointer, "future already retrieved", 25);
    source.code = {0x12345678, 2, future};
    failAllocation = true;
    bool allocationFailure = false;
    try { raise(&source); }
    catch (const std::bad_alloc&) { allocationFailure = true; }
    failAllocation = false;
    REQUIRE(allocationFailure);
    REQUIRE(std::strcmp(source.message.pointer, "future already retrieved") == 0);
    _ZdlPv_nid_postfix(source.message.pointer);

    LibcException::FutureErrorObject object {};
    object.message.capacity = 15;
    _ZNSt12future_errorD1Ev_nid_postfix(&object);
    REQUIRE(object.message.reserved == 0 && object.message.size == 0 && object.message.capacity == 15);
    REQUIRE(object.message.buffer[0] == 0);
}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());
    CheckBaseCategory();
    CheckFutureCategory();
    CheckFutureError();
    std::puts("guest future category, error_category base members and future_error layout: PASS");
    return 0;
}
