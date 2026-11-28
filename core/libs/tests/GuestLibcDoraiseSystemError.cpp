#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <bit>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <system_error>
#include <typeinfo>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

extern "C" {
extern const void* const _ZTVSt9exception_nid_postfix[5];
extern const void* const _ZTVSt8bad_cast_nid_postfix[5];
extern const void* const _ZTVSt11logic_error_nid_postfix[5];
extern const void* const _ZTVSt12system_error_nid_postfix[5];
extern const void* const _ZTVNSt8ios_base7failureE_nid_postfix[5];
extern const void* const _ZTISt9exception_nid_postfix[];
extern const void* const _ZTISt8bad_cast_nid_postfix[];
extern const void* const _ZTISt11logic_error_nid_postfix[];
extern const void* const _ZTISt12system_error_nid_postfix[];
extern const void* const _ZTINSt8ios_base7failureE_nid_postfix[];
}

using GuestWhat = const char* (APS5_VABI *)(const void*);

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) { std::free(pointer); }
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void Require(bool condition, int line) {
    if (condition) return;
    std::fprintf(stderr, "guest libc doraise system_error check failed at line %d\n", line);
    std::exit(1);
}
#define REQUIRE(value) Require((value), __LINE__)

const void* VtableOf(const void* object) { return *static_cast<const void* const*>(object); }

const char* TypeName(const void* object) {
    return static_cast<const TypeRecord*>(static_cast<const void* const*>(VtableOf(object))[-1])->name;
}

const char* What(const void* object) {
    return std::bit_cast<GuestWhat>(static_cast<const void* const*>(VtableOf(object))[2])(object);
}

void CheckPlainDoraise(const char* name, const void* const* table, const char* message,
                       void (APS5_VABI *raise)(const LibcException::ExceptionObject*)) {
    LibcException::ExceptionObject source {&table[2], nullptr};
    bool caught = false;
    try { raise(&source); }
    catch (const std::exception& error) {
        caught = true;
        REQUIRE(&error != reinterpret_cast<const std::exception*>(&source));
        REQUIRE(VtableOf(&error) == &table[2]);
        REQUIRE(std::strcmp(TypeName(&error), name) == 0);
        REQUIRE(std::strcmp(What(&error), message) == 0);
    }
    REQUIRE(caught);

    bool rejected = false;
    try { raise(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

void CheckStdExceptionDoraise() {
    CheckPlainDoraise("St9exception", _ZTVSt9exception_nid_postfix, "std::exception",
                      _ZNKSt9exception8_DoraiseEv_nid_postfix);
}

void CheckBadCastDoraise() {
    CheckPlainDoraise("St8bad_cast", _ZTVSt8bad_cast_nid_postfix, "std::bad_cast",
                      _ZNKSt8bad_cast8_DoraiseEv_nid_postfix);
}

void CheckLogicErrorDoraise() {
    LibcException::ExceptionObject source {&_ZTVSt11logic_error_nid_postfix[2], nullptr};
    _ZNSt11logic_errorC1EPKc_nid_postfix(&source, "logic-msg");

    bool caught = false;
    try { _ZNKSt11logic_error8_DoraiseEv_nid_postfix(&source); }
    catch (const std::logic_error& error) {
        caught = true;
        REQUIRE(&error != reinterpret_cast<const std::logic_error*>(&source));
        REQUIRE(VtableOf(&error) == &_ZTVSt11logic_error_nid_postfix[2]);
        REQUIRE(std::strcmp(TypeName(&error), "St11logic_error") == 0);
        REQUIRE(std::strcmp(What(&error), "logic-msg") == 0);
    }
    REQUIRE(caught);

    bool rejected = false;
    try { _ZNKSt11logic_error8_DoraiseEv_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);

    _ZNSt11logic_errorD1Ev_nid_postfix(&source);
    REQUIRE(source.message == nullptr);
}

void CheckSystemErrorDoraise() {
    LibcException::SystemErrorObject source {};
    LibcException::ErrorCode code {0, 22, _ZSt16generic_categoryv_nid_postfix()};
    _ZNSt12system_errorC2ESt10error_codePKc_nid_postfix(&source, code, "system-msg");

    bool caught = false;
    try { _ZNKSt8ios_base7failure8_DoraiseEv_nid_postfix(&source); }
    catch (const std::system_error& error) {
        caught = true;
        const auto* object = reinterpret_cast<const LibcException::SystemErrorObject*>(&error);
        REQUIRE(object != &source);
        REQUIRE(object->base.vtable == &_ZTVNSt8ios_base7failureE_nid_postfix[2]);
        REQUIRE(std::strcmp(TypeName(object), "NSt8ios_base7failureE") == 0);
        REQUIRE(object->base.message == source.base.message);
        REQUIRE(std::strcmp(What(object), "system-msg: invalid argument") == 0);
        REQUIRE(object->code.value == source.code.value && object->code.category == source.code.category);
    }
    REQUIRE(caught);

    bool rejected = false;
    try { _ZNKSt8ios_base7failure8_DoraiseEv_nid_postfix(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);

    _ZNSt12system_errorD1Ev_nid_postfix(&source.base);
    REQUIRE(source.base.message == nullptr);
}

void CheckSystemErrorCopyConstruct() {
    LibcException::SystemErrorObject source {};
    LibcException::ErrorCode code {0, 22, _ZSt16generic_categoryv_nid_postfix()};
    _ZNSt12system_errorC2ESt10error_codePKc_nid_postfix(&source, code, "system-msg");

    LibcException::SystemErrorObject copy {};
    _ZNSt12system_errorC2ERKS__nid_postfix(&copy, &source);

    REQUIRE(copy.base.vtable == source.base.vtable && copy.base.message == source.base.message);
    REQUIRE(copy.code.value == source.code.value && copy.code.category == source.code.category);
    REQUIRE(std::strcmp(_ZNKSt12system_error4whatEv_nid_postfix(&copy.base), "system-msg: invalid argument") == 0);

    _ZNSt12system_errorD1Ev_nid_postfix(&source.base);
    std::memset(&source, 0xcd, sizeof(source));

    REQUIRE(copy.base.vtable == _ZTVSt12system_error_nid_postfix + 2);
    REQUIRE(copy.code.value == 22 && copy.code.category == _ZSt16generic_categoryv_nid_postfix());
    REQUIRE(std::strcmp(_ZNKSt12system_error4whatEv_nid_postfix(&copy.base), "system-msg: invalid argument") == 0);

    _ZNSt12system_errorD1Ev_nid_postfix(&copy.base);
    REQUIRE(copy.base.message == nullptr);
}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckStdExceptionDoraise();
    CheckBadCastDoraise();
    CheckLogicErrorDoraise();
    CheckSystemErrorDoraise();
    CheckSystemErrorCopyConstruct();

    std::puts("guest libc doraise system_error tests passed");
    return 0;
}
