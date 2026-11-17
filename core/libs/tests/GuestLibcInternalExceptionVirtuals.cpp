#include "prx/libSceLibcInternal/libSceLibcInternal.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "Testing.hpp"
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
        Require(&error != reinterpret_cast<const std::exception*>(&source));
        Require(VtableOf(&error) == &table[2]);
        Require(std::strcmp(TypeName(&error), name) == 0);
        Require(std::strcmp(What(&error), message) == 0);
    }
    Require(caught);

    bool rejected = false;
    try { raise(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
}

void CheckBadAllocDoraise() {
    CheckPlainDoraise(sceLibcInternalBadAllocDoraise_nid_no_patch, _ZTVSt9bad_alloc_nid_postfix, "St9bad_alloc", "bad allocation");
    bool caught = false;
    LibcException::ExceptionObject source {&_ZTVSt9bad_alloc_nid_postfix[2], nullptr};
    try { sceLibcInternalBadAllocDoraise_nid_no_patch(&source); }
    catch (const std::bad_alloc&) { caught = true; }
    Require(caught);
}

void CheckBadTypeidDoraise() {
    CheckPlainDoraise(sceLibcInternalBadTypeidDoraise_nid_no_patch, _ZTVSt10bad_typeid_nid_postfix, "St10bad_typeid", "std::bad_typeid");
    bool caught = false;
    LibcException::ExceptionObject source {&_ZTVSt10bad_typeid_nid_postfix[2], nullptr};
    try { sceLibcInternalBadTypeidDoraise_nid_no_patch(&source); }
    catch (const std::bad_typeid&) { caught = true; }
    Require(caught);
}

void CheckBadVariantAccessWhat() {
    LibcException::ExceptionObject object {&_ZTVSt18bad_variant_access_nid_postfix[2], nullptr};
    const char* text = sceLibcInternalBadVariantAccessWhat_nid_no_patch(&object);
    Require(text != nullptr && std::strcmp(text, "bad_variant_access") == 0);
    Require(text == What(&object));
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
        Require(text != nullptr && std::strcmp(text, error.text) == 0);
        Require(text == What(&object));
    }
    for (int value : {0, 5, 22, -1}) {
        const auto object = FutureError(value);
        Require(sceLibcInternalFutureErrorWhat_nid_no_patch(&object) == nullptr);
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
        Require(object != &source);
        Require(object->vtable == &_ZTVSt12future_error_nid_postfix[2]);
        Require(std::strcmp(TypeName(object), "St12future_error") == 0);
        Require(object->code.reserved == source.code.reserved && object->code.value == 3 && object->code.category == source.code.category);
        Require(object->message.size == 25 && object->message.capacity == 31);
        Require(object->message.pointer != source.message.pointer);
        Require(std::strcmp(object->message.pointer, "promise already satisfied") == 0);
        Require(std::strcmp(What(object), "promise already satisfied") == 0);
    }
    Require(caught);
    Require(std::strcmp(source.message.pointer, "promise already satisfied") == 0);
    _ZdlPv_nid_postfix(source.message.pointer);

    auto inline_ = FutureError(4);
    inline_.message.size = 2;
    std::memcpy(inline_.message.buffer, "ns", 3);
    caught = false;
    try { sceLibcInternalFutureErrorDoraise_nid_no_patch(&inline_); }
    catch (const std::future_error& error) {
        caught = true;
        const auto* object = reinterpret_cast<const LibcException::FutureErrorObject*>(&error);
        Require(object->code.value == 4 && object->message.size == 2 && object->message.capacity == 15);
        Require(std::strcmp(object->message.buffer, "ns") == 0);
        Require(std::strcmp(What(object), "no state") == 0);
    }
    Require(caught);

    bool rejected = false;
    try { sceLibcInternalFutureErrorDoraise_nid_no_patch(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
}

void CheckMessage(const LibcException::CategoryObject* category, int value, const char* expected) {
    GuestLocale::String result {};
    std::memset(&result, 0xcd, sizeof(result));
    Require(sceLibcInternalFutureErrorCategoryMessage_nid_no_patch(&result, category, value) == &result);
    const auto length = std::strlen(expected);
    Require(result.reserved == 0 && result.size == length && (result.capacity & 15) == 15);
    const char* text = result.capacity <= 15 ? result.buffer : result.pointer;
    Require(std::memcmp(text, expected, length + 1) == 0);
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
            Require(object != source);
            Require(object->base.vtable == &_ZTVSt12system_error_nid_postfix[2]);
            Require(std::strcmp(TypeName(object), "St12system_error") == 0);
            Require(object->base.message == source->base.message);
            Require(std::strcmp(What(object), What(source)) == 0);
            Require(object->code.value == source->code.value && object->code.category == source->code.category);
        }
        Require(What(source)[0] != 0);
    }
    Require(caught);

    LibcException::SystemErrorObject custom {};
    _ZNSt13runtime_errorC1EPKc_nid_postfix(&custom.base, "custom failure");
    custom.base.vtable = &_ZTVSt12system_error_nid_postfix[2];
    custom.code = {0, 22, _ZSt16generic_categoryv_nid_postfix()};
    caught = false;
    try { sceLibcInternalSystemErrorDoraise_nid_no_patch(&custom); }
    catch (const std::runtime_error& error) {
        caught = true;
        const auto* object = reinterpret_cast<const LibcException::SystemErrorObject*>(&error);
        Require(std::strcmp(TypeName(object), "St12system_error") == 0);
        Require(std::strcmp(What(object), "custom failure") == 0);
        Require(object->code.value == 22 && object->code.category == custom.code.category);
    }
    Require(caught);
    _ZNSt12system_errorD1Ev_nid_postfix(&custom.base);
    Require(custom.base.message == nullptr);

    bool rejected = false;
    try { sceLibcInternalSystemErrorDoraise_nid_no_patch(nullptr); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected);
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
