#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
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
#include <future>
#include <new>
#include <stdexcept>
#include <system_error>
#include <typeinfo>

struct TypeRecord { const void* vtable; const char* name; const TypeRecord* base; };

extern "C" {
extern const void* const _ZTVSt12future_error_nid_postfix[6];
extern const void* const _ZTVSt18bad_variant_access_nid_postfix[5];
extern const void* const _ZTVSt9bad_alloc_nid_postfix[5];
extern const void* const _ZTVSt10bad_typeid_nid_postfix[5];
extern const void* const _ZTVSt12system_error_nid_postfix[5];
}

namespace {

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
    std::fprintf(stderr, "libc internal exception virtuals check failed at line %d\n", line);
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

void CheckPlainDoraise(void (APS5_VABI *raise)(const LibcException::ExceptionObject*), const void* const* table, const char* name, const char* message) {
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

void CheckBadAllocDoraise() {
    CheckPlainDoraise(sceLibcInternalBadAllocDoraise_nid_no_patch, _ZTVSt9bad_alloc_nid_postfix, "St9bad_alloc", "bad allocation");
    bool caught = false;
    LibcException::ExceptionObject source {&_ZTVSt9bad_alloc_nid_postfix[2], nullptr};
    try { sceLibcInternalBadAllocDoraise_nid_no_patch(&source); }
    catch (const std::bad_alloc&) { caught = true; }
    REQUIRE(caught);
}

void CheckBadTypeidDoraise() {
    CheckPlainDoraise(sceLibcInternalBadTypeidDoraise_nid_no_patch, _ZTVSt10bad_typeid_nid_postfix, "St10bad_typeid", "std::bad_typeid");
    bool caught = false;
    LibcException::ExceptionObject source {&_ZTVSt10bad_typeid_nid_postfix[2], nullptr};
    try { sceLibcInternalBadTypeidDoraise_nid_no_patch(&source); }
    catch (const std::bad_typeid&) { caught = true; }
    REQUIRE(caught);
}

void CheckBadVariantAccessWhat() {
    LibcException::ExceptionObject object {&_ZTVSt18bad_variant_access_nid_postfix[2], nullptr};
    const char* text = sceLibcInternalBadVariantAccessWhat_nid_no_patch(&object);
    REQUIRE(text != nullptr && std::strcmp(text, "bad_variant_access") == 0);
    REQUIRE(text == What(&object));
}

LibcException::FutureErrorObject FutureError(int value) {
    LibcException::FutureErrorObject object {};
    object.vtable = &_ZTVSt12future_error_nid_postfix[2];
    object.message.capacity = 15;
    object.code = {0x12345678, value, _ZSt15future_categoryv_nid_postfix()};
    return object;
}

void CheckFutureErrorWhat() {
    struct Expected { int value; const char* text; };
    static constexpr Expected errors[] {
        {1, "broken promise"}, {2, "future already retrieved"}, {3, "promise already satisfied"}, {4, "no state"}
    };
    for (const auto& error : errors) {
        const auto object = FutureError(error.value);
        const char* text = sceLibcInternalFutureErrorWhat_nid_no_patch(&object);
        REQUIRE(text != nullptr && std::strcmp(text, error.text) == 0);
        REQUIRE(text == What(&object));
    }
    for (int value : {0, 5, 22, -1}) {
        const auto object = FutureError(value);
        REQUIRE(sceLibcInternalFutureErrorWhat_nid_no_patch(&object) == nullptr);
    }
}

void CheckFutureErrorDoraise() {
    auto source = FutureError(3);
    source.message.capacity = 31;
    source.message.size = 25;
    source.message.pointer = static_cast<char*>(_Znwm_nid_postfix(32));
    std::memcpy(source.message.pointer, "promise already satisfied", 26);
    bool caught = false;
    try { sceLibcInternalFutureErrorDoraise_nid_no_patch(&source); }
    catch (const std::future_error& error) {
        caught = true;
        const auto* object = reinterpret_cast<const LibcException::FutureErrorObject*>(&error);
        REQUIRE(object != &source);
        REQUIRE(object->vtable == &_ZTVSt12future_error_nid_postfix[2]);
        REQUIRE(std::strcmp(TypeName(object), "St12future_error") == 0);
        REQUIRE(object->code.reserved == source.code.reserved && object->code.value == 3 && object->code.category == source.code.category);
        REQUIRE(object->message.size == 25 && object->message.capacity == 31);
        REQUIRE(object->message.pointer != source.message.pointer);
        REQUIRE(std::strcmp(object->message.pointer, "promise already satisfied") == 0);
        REQUIRE(std::strcmp(What(object), "promise already satisfied") == 0);
    }
    REQUIRE(caught);
    REQUIRE(std::strcmp(source.message.pointer, "promise already satisfied") == 0);
    _ZdlPv_nid_postfix(source.message.pointer);

    auto inline_ = FutureError(4);
    inline_.message.size = 2;
    std::memcpy(inline_.message.buffer, "ns", 3);
    caught = false;
    try { sceLibcInternalFutureErrorDoraise_nid_no_patch(&inline_); }
    catch (const std::future_error& error) {
        caught = true;
        const auto* object = reinterpret_cast<const LibcException::FutureErrorObject*>(&error);
        REQUIRE(object->code.value == 4 && object->message.size == 2 && object->message.capacity == 15);
        REQUIRE(std::strcmp(object->message.buffer, "ns") == 0);
        REQUIRE(std::strcmp(What(object), "no state") == 0);
    }
    REQUIRE(caught);

    bool rejected = false;
    try { sceLibcInternalFutureErrorDoraise_nid_no_patch(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

void CheckMessage(const LibcException::CategoryObject* category, int value, const char* expected) {
    GuestLocale::String result {};
    std::memset(&result, 0xcd, sizeof(result));
    REQUIRE(sceLibcInternalFutureErrorCategoryMessage_nid_no_patch(&result, category, value) == &result);
    const auto length = std::strlen(expected);
    REQUIRE(result.reserved == 0 && result.size == length && (result.capacity & 15) == 15);
    const char* text = result.capacity <= 15 ? result.buffer : result.pointer;
    REQUIRE(std::memcmp(text, expected, length + 1) == 0);
    if (result.capacity > 15) _ZdlPv_nid_postfix(result.pointer);
}

void CheckFutureCategoryMessage() {
    const auto* future = _ZSt15future_categoryv_nid_postfix();
    CheckMessage(future, 1, "broken promise");
    CheckMessage(future, 2, "future already retrieved");
    CheckMessage(future, 3, "promise already satisfied");
    CheckMessage(future, 4, "no state");
    for (int value : {0, 5, 22, 99}) {
        const char* generic = _ZSt13_Syserror_mapi_nid_postfix(value);
        CheckMessage(future, value, generic ? generic : "unknown error");
    }
}

void CheckSystemErrorDoraise() {
    bool caught = false;
    try { _ZSt16_Throw_Cpp_errori_nid_postfix(4); }
    catch (const std::system_error& first) {
        const auto* source = reinterpret_cast<const LibcException::SystemErrorObject*>(&first);
        try { sceLibcInternalSystemErrorDoraise_nid_no_patch(source); }
        catch (const std::system_error& second) {
            caught = true;
            const auto* object = reinterpret_cast<const LibcException::SystemErrorObject*>(&second);
            REQUIRE(object != source);
            REQUIRE(object->base.vtable == &_ZTVSt12system_error_nid_postfix[2]);
            REQUIRE(std::strcmp(TypeName(object), "St12system_error") == 0);
            REQUIRE(object->base.message == source->base.message);
            REQUIRE(std::strcmp(What(object), What(source)) == 0);
            REQUIRE(object->code.value == source->code.value && object->code.category == source->code.category);
        }
        REQUIRE(What(source)[0] != 0);
    }
    REQUIRE(caught);

    LibcException::SystemErrorObject custom {};
    _ZNSt13runtime_errorC1EPKc_nid_postfix(&custom.base, "custom failure");
    custom.base.vtable = &_ZTVSt12system_error_nid_postfix[2];
    custom.code = {0, 22, _ZSt16generic_categoryv_nid_postfix()};
    caught = false;
    try { sceLibcInternalSystemErrorDoraise_nid_no_patch(&custom); }
    catch (const std::runtime_error& error) {
        caught = true;
        const auto* object = reinterpret_cast<const LibcException::SystemErrorObject*>(&error);
        REQUIRE(std::strcmp(TypeName(object), "St12system_error") == 0);
        REQUIRE(std::strcmp(What(object), "custom failure") == 0);
        REQUIRE(object->code.value == 22 && object->code.category == custom.code.category);
    }
    REQUIRE(caught);
    _ZNSt12system_errorD1Ev_nid_postfix(&custom.base);
    REQUIRE(custom.base.message == nullptr);

    bool rejected = false;
    try { sceLibcInternalSystemErrorDoraise_nid_no_patch(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    REQUIRE(rejected);
}

}

int main() {
    const std::array<void*, 10> api {
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    CheckBadAllocDoraise();
    CheckBadTypeidDoraise();
    CheckBadVariantAccessWhat();
    CheckFutureErrorWhat();
    CheckFutureErrorDoraise();
    CheckFutureCategoryMessage();
    CheckSystemErrorDoraise();

    std::puts("libc internal exception virtual member checks passed");
    return 0;
}
