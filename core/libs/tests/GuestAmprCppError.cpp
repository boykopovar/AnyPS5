#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };
struct Category { const void* const* vtable; };
struct ErrorCode { int reserved; int value; const Category* category; };
struct SystemError { const void* const* vtable; const char* message; ErrorCode code; };
static_assert(sizeof(SystemError) == 32 && offsetof(SystemError, code) == 16);

using GuestWhat = const char* (APS5_VABI *)(const SystemError*);
using CategoryDelete = void (APS5_VABI *)(const Category*);
using CategoryName = const char* (APS5_VABI *)(const Category*);
using CategoryMessage = void* (APS5_VABI *)(void*, const Category*, int);
using CategoryCondition = ErrorCode (APS5_VABI *)(const Category*, int);
using CategoryEquivalentCondition = bool (APS5_VABI *)(const Category*, int, const ErrorCode*);
using CategoryEquivalentCode = bool (APS5_VABI *)(const Category*, const ErrorCode*, int);

struct Expected { int value; const char* what; };
static constexpr Expected Errors[] {
    {16, "device or resource busy: device or resource busy"},
    {22, "invalid argument: invalid argument"},
    {3, "no such process: no such process"},
    {12, "not enough memory: not enough memory"},
    {1, "operation not permitted: operation not permitted"},
    {11, "resource deadlock would occur: resource deadlock would occur"},
    {35, "resource unavailable try again: operation would block"}
};

extern "C" {
extern const unsigned char _ZTISt9exception_nid_postfix[];
extern const unsigned char _ZTISt11logic_error_nid_postfix[];
extern const unsigned char _ZTISt13runtime_error_nid_postfix[];
extern const unsigned char _ZTISt12system_error_nid_postfix[];
[[noreturn]] void APS5_VABI _ZSt16_Throw_Cpp_errori_nid_postfix(int code);
void APS5_VABI _ZNSt12system_errorC1ERKS__nid_postfix(SystemError* self, const SystemError* source);
SystemError* APS5_VABI _ZNSt12system_erroraSERKS__nid_postfix(SystemError* self, const SystemError* source);
void APS5_VABI _ZNSt12system_errorD1Ev_nid_postfix(SystemError* self);
void APS5_VABI _ZdlPv_nid_postfix(void* pointer);
void APS5_VABI GuestCatch(int code);

const void* FixtureCatchType = nullptr;
const void* FixtureOtherType = nullptr;
const Expected* FixtureExpected = nullptr;
unsigned FixtureInspected = 0;

[[noreturn]] void APS5_VABI FixtureFail(unsigned code) {
    std::fprintf(stderr, "guest _Throw_Cpp_error failure %u\n", code);
    std::exit(static_cast<int>(code));
}
}

static void Require(bool value, unsigned code) {
    if (!value) FixtureFail(code);
}

template<class TFunction>
static TFunction Slot(const void* const* vtable, int index) {
    return reinterpret_cast<TFunction>(const_cast<void*>(vtable[index]));
}

static const TypeRecord* TypeOf(const void* const* vtable, int destroySlot) {
    return static_cast<const TypeRecord*>(vtable[destroySlot - 1]);
}

static void CheckCategory(const SystemError& error) {
    const Category* category = error.code.category;
    Require(category != nullptr, 20);
    const TypeRecord* type = TypeOf(category->vtable, 0);
    Require(std::strcmp(type->name, "St23_Generic_error_category") == 0, 21);
    Require(type->base && std::strcmp(type->base->name, "St14error_category") == 0, 22);
    Require(std::strcmp(Slot<CategoryName>(category->vtable, 2)(category), "generic") == 0, 23);
    const ErrorCode condition = Slot<CategoryCondition>(category->vtable, 4)(category, error.code.value);
    Require(condition.value == error.code.value && condition.category == category, 24);
    const ErrorCode other {0, error.code.value + 1, category};
    const ErrorCode foreign {0, error.code.value, nullptr};
    auto equivalentCondition = Slot<CategoryEquivalentCondition>(category->vtable, 5);
    Require(equivalentCondition(category, error.code.value, &condition), 25);
    Require(!equivalentCondition(category, error.code.value, &other), 26);
    Require(!equivalentCondition(category, error.code.value, &foreign), 27);
    auto equivalentCode = Slot<CategoryEquivalentCode>(category->vtable, 6);
    Require(equivalentCode(category, &error.code, error.code.value), 28);
    Require(!equivalentCode(category, &error.code, error.code.value + 1), 29);
    Require(!equivalentCode(category, &foreign, error.code.value), 30);
    GuestLocale::String result {};
    Require(Slot<CategoryMessage>(category->vtable, 3)(&result, category, error.code.value) == &result, 33);
    const char* data = result.capacity <= 15 ? result.buffer : result.pointer;
    Require(std::strcmp(data, std::strstr(FixtureExpected->what, ": ") + 2) == 0, 34);
    if (result.capacity > 15) _ZdlPv_nid_postfix(result.pointer);
    bool deleteThrew = false;
    try {
        Slot<CategoryDelete>(category->vtable, 1)(category);
    } catch (const std::invalid_argument&) {
        deleteThrew = true;
    }
    Require(deleteThrew, 32);
}

static void CheckCopies(const SystemError& error) {
    SystemError copy {};
    _ZNSt12system_errorC1ERKS__nid_postfix(&copy, &error);
    Require(copy.vtable == error.vtable && copy.message == error.message, 40);
    Require(copy.code.value == error.code.value && copy.code.category == error.code.category, 41);
    SystemError assigned {};
    _ZNSt12system_errorC1ERKS__nid_postfix(&assigned, &error);
    assigned.code = {0, 0, nullptr};
    Require(_ZNSt12system_erroraSERKS__nid_postfix(&assigned, &error) == &assigned, 42);
    Require(assigned.code.value == error.code.value && assigned.code.category == error.code.category, 43);
    _ZNSt12system_errorD1Ev_nid_postfix(&assigned);
    _ZNSt12system_errorD1Ev_nid_postfix(&copy);
    Require(copy.message == nullptr && assigned.message == nullptr, 44);
}

extern "C" void APS5_VABI FixtureInspect(void* object) {
    const auto& error = *static_cast<const SystemError*>(object);
    Require(std::strcmp(TypeOf(error.vtable, 0)->name, "St12system_error") == 0, 10);
    const char* what = Slot<GuestWhat>(error.vtable, 2)(&error);
    Require(what && std::strcmp(what, FixtureExpected->what) == 0, 11);
    Require(error.code.value == FixtureExpected->value, 12);
    CheckCategory(error);
    CheckCopies(error);
    ++FixtureInspected;
}

static void Catch(const void* type, int code) {
    FixtureCatchType = type;
    FixtureExpected = &Errors[code];
    const auto before = FixtureInspected;
    GuestCatch(code);
    Require(FixtureInspected == before + 1, 2);
}

static void CheckOutOfRange(int code) {
    bool threw = false;
    try {
        _ZSt16_Throw_Cpp_errori_nid_postfix(code);
    } catch (const std::out_of_range& error) {
        threw = std::string(error.what()).find(std::to_string(code)) != std::string::npos;
    }
    Require(threw, 3);
}

int main() {
    const std::array<void*, 10> api {};
    ApplicationHeapRegister_nid_no_patch(api.data());
    FixtureOtherType = _ZTISt11logic_error_nid_postfix;
    for (int code = 0; code < static_cast<int>(std::size(Errors)); ++code) Catch(_ZTISt12system_error_nid_postfix, code);
    Catch(_ZTISt13runtime_error_nid_postfix, 5);
    Catch(_ZTISt9exception_nid_postfix, 6);
    CheckOutOfRange(-1);
    CheckOutOfRange(static_cast<int>(std::size(Errors)));
    std::puts("guest _Throw_Cpp_error system_error, generic category and range: PASS");
    return 0;
}
