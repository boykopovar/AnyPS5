#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include <array>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <new>

extern "C" {
void APS5_VABI sceLibcInternalUnknown6_112_6yplvTHbxpE_nid_no_patch(void* self);
extern std::uintptr_t _ZTVSt12future_error_nid_postfix[6];
}

namespace {

unsigned releases = 0;
void* lastReleased = nullptr;
std::array<void*, 8> released {};

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "libc internal deleting destructors: %s\n", message);
    std::abort();
}

void* APS5_VABI TestAllocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI TestFree(void* pointer) {
    if (releases < released.size()) released[releases] = pointer;
    ++releases;
    lastReleased = pointer;
    std::free(pointer);
}
void* APS5_VABI TestCalloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI TestReallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI TestAlign(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI TestRealign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI TestPosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

using ExceptionDestructor = void (APS5_VABI*)(LibcException::ExceptionObject*);

void CheckExceptionDeletes() {
    struct Entry { ExceptionDestructor deleting; ExceptionDestructor complete; ExceptionDestructor base; bool message; };
    const Entry entries[] = {
        {_ZNSt9exceptionD0Ev_nid_postfix, _ZNSt9exceptionD1Ev_nid_postfix, _ZNSt9exceptionD2Ev_nid_postfix, false},
        {_ZNSt8bad_castD0Ev_nid_postfix, _ZNSt8bad_castD1Ev_nid_postfix, _ZNSt8bad_castD2Ev_nid_postfix, false},
        {_ZNSt10bad_typeidD0Ev_nid_postfix, _ZNSt10bad_typeidD1Ev_nid_postfix, _ZNSt10bad_typeidD2Ev_nid_postfix, false},
        {_ZNSt9bad_allocD0Ev_nid_postfix, _ZNSt9bad_allocD1Ev_nid_postfix, _ZNSt9bad_allocD2Ev_nid_postfix, false},
        {_ZNSt20bad_array_new_lengthD0Ev_nid_postfix, _ZNSt20bad_array_new_lengthD1Ev_nid_postfix, _ZNSt20bad_array_new_lengthD2Ev_nid_postfix, false},
        {_ZNSt17bad_function_callD0Ev_nid_postfix, _ZNSt17bad_function_callD1Ev_nid_postfix, _ZNSt17bad_function_callD2Ev_nid_postfix, false},
        {_ZNSt12bad_weak_ptrD0Ev_nid_postfix, _ZNSt12bad_weak_ptrD1Ev_nid_postfix, _ZNSt12bad_weak_ptrD2Ev_nid_postfix, false},
        {_ZNSt19bad_optional_accessD0Ev_nid_postfix, _ZNSt19bad_optional_accessD1Ev_nid_postfix, _ZNSt19bad_optional_accessD2Ev_nid_postfix, false},
        {_ZNSt11logic_errorD0Ev_nid_postfix, _ZNSt11logic_errorD1Ev_nid_postfix, _ZNSt11logic_errorD2Ev_nid_postfix, true},
        {_ZNSt13runtime_errorD0Ev_nid_postfix, _ZNSt13runtime_errorD1Ev_nid_postfix, _ZNSt13runtime_errorD2Ev_nid_postfix, true},
        {_ZNSt16invalid_argumentD0Ev_nid_postfix, _ZNSt16invalid_argumentD1Ev_nid_postfix, _ZNSt16invalid_argumentD2Ev_nid_postfix, true},
        {_ZNSt12out_of_rangeD0Ev_nid_postfix, _ZNSt12out_of_rangeD1Ev_nid_postfix, _ZNSt12out_of_rangeD2Ev_nid_postfix, true},
        {_ZNSt12domain_errorD0Ev_nid_postfix, _ZNSt12domain_errorD1Ev_nid_postfix, _ZNSt12domain_errorD2Ev_nid_postfix, true},
        {_ZNSt12length_errorD0Ev_nid_postfix, _ZNSt12length_errorD1Ev_nid_postfix, _ZNSt12length_errorD2Ev_nid_postfix, true},
        {_ZNSt12system_errorD0Ev_nid_postfix, _ZNSt12system_errorD1Ev_nid_postfix, _ZNSt12system_errorD2Ev_nid_postfix, true},
        {_ZNSt11regex_errorD0Ev_nid_postfix, _ZNSt11regex_errorD1Ev_nid_postfix, _ZNSt11regex_errorD2Ev_nid_postfix, true},
        {_ZNSt11range_errorD0Ev_nid_postfix, _ZNSt11range_errorD1Ev_nid_postfix, _ZNSt11range_errorD2Ev_nid_postfix, true},
        {_ZNSt8ios_base7failureD0Ev_nid_postfix, _ZNSt8ios_base7failureD1Ev_nid_postfix, _ZNSt8ios_base7failureD2Ev_nid_postfix, true},
        {_ZNSt13__sce_cxxvers16_Error_exceptionD0Ev_nid_postfix, _ZNSt13__sce_cxxvers16_Error_exceptionD1Ev_nid_postfix, _ZNSt13__sce_cxxvers16_Error_exceptionD2Ev_nid_postfix, true},
    };
    for (const auto& entry : entries) {
        auto* object = static_cast<LibcException::ExceptionObject*>(_Znwm_nid_postfix(0x40));
        std::memset(object, 0, 0x40);
        if (entry.message) _ZNSt13runtime_errorC1EPKc_nid_postfix(object, "shared guest exception message");
        else _ZNSt9exceptionC1Ev_nid_postfix(object);
        releases = 0;
        entry.complete(object);
        entry.base(object);
        Require(releases == 0, "nondeleting exception destructors keep the object");
        LibcException::ExceptionObject retained {};
        if (entry.message) {
            _ZNSt13runtime_errorC1EPKc_nid_postfix(object, "shared guest exception message");
            _ZNSt13runtime_errorC1ERKS__nid_postfix(&retained, object);
        }
        releases = 0;
        entry.deleting(object);
        Require(releases == 1 && lastReleased == object, "direct D0 uses guest operator delete");
        if (entry.message) {
            Require(std::strcmp(retained.message, "shared guest exception message") == 0, "deleting a copy retains the shared host message");
            _ZNSt13runtime_errorD1Ev_nid_postfix(&retained);
            Require(releases == 1, "shared host message release does not use the guest heap");
        }
    }
    for (const bool message : {false, true}) {
        auto* object = static_cast<LibcException::ExceptionObject*>(_Znwm_nid_postfix(sizeof(LibcException::ExceptionObject)));
        if (message) _ZNSt13runtime_errorC1EPKc_nid_postfix(object, "virtual guest exception");
        else _ZNSt9exceptionC1Ev_nid_postfix(object);
        const auto* table = static_cast<const void* const*>(object->vtable);
        const auto deleting = reinterpret_cast<ExceptionDestructor>(const_cast<void*>(table[1]));
        releases = 0;
        deleting(object);
        Require(releases == 1 && lastReleased == object, "guest virtual D0 uses guest operator delete");
    }
    const auto before = releases;
    bool caught = false;
    try { _ZSt14_Xout_of_rangePKc_nid_postfix("thrown guest exception"); }
    catch (...) {
        const auto* type = __cxa_current_exception_type_nid_postfix();
        caught = type && std::strcmp(type->name(), "St12out_of_range") == 0;
    }
    Require(caught && releases == before, "runtime-owned exceptions use nondeleting cleanup");
}

void CheckFutureDelete() {
    auto* object = new (_Znwm_nid_postfix(sizeof(LibcException::FutureErrorObject))) LibcException::FutureErrorObject{};
    object->message.capacity = 24;
    object->message.size = 24;
    object->message.pointer = static_cast<char*>(_Znwm_nid_postfix(25));
    std::memcpy(object->message.pointer, "future already retrieved", 25);
    auto* message = object->message.pointer;
    using Delete = void (APS5_VABI*)(LibcException::FutureErrorObject*);
    const auto deleting = reinterpret_cast<Delete>(_ZTVSt12future_error_nid_postfix[3]);
    releases = 0;
    deleting(object);
    Require(releases == 2 && released[0] == message && released[1] == object, "future_error releases guest message then guest object");
}

void CheckFilesystemDeletes() {
    struct FilesystemObject {
        LibcException::SystemErrorObject base;
        unsigned char reserved[0x10];
        LibcException::ErrorCode code;
        GuestLocale::String path1;
        GuestLocale::String path2;
    };
    static_assert(sizeof(FilesystemObject) == 0x90);
    for (const bool versioned : {false, true}) {
        const void* table = nullptr;
        const auto before = releases;
        try {
            const LibcException::ErrorCode code {0, 0, _ZSt16generic_categoryv_nid_postfix()};
            if (versioned) _ZNSt8__sce_v210filesystem18_Xfilesystem_errorEPKcNS_10error_codeE_nid_postfix("delete", code);
            else _ZNSt10filesystem18_Xfilesystem_errorEPKcSt10error_code_nid_postfix("delete", code);
        } catch (const std::exception& error) {
            table = reinterpret_cast<const LibcException::ExceptionObject*>(&error)->vtable;
        }
        Require(table != nullptr && releases == before, "thrown filesystem_error uses nondeleting host exception cleanup");
        auto* object = new (_Znwm_nid_postfix(sizeof(FilesystemObject))) FilesystemObject{};
        _ZNSt13runtime_errorC1EPKc_nid_postfix(&object->base.base, "filesystem deleting copy");
        object->base.base.vtable = table;
        object->path1.capacity = object->path2.capacity = 31;
        object->path1.size = object->path2.size = 21;
        object->path1.pointer = static_cast<char*>(_Znwm_nid_postfix(32));
        object->path2.pointer = static_cast<char*>(_Znwm_nid_postfix(32));
        std::memcpy(object->path1.pointer, "first path for delete", 22);
        std::memcpy(object->path2.pointer, "other path for delete", 22);
        auto* first = object->path1.pointer;
        auto* second = object->path2.pointer;
        const auto* slots = static_cast<const void* const*>(table);
        const auto deleting = reinterpret_cast<ExceptionDestructor>(const_cast<void*>(slots[1]));
        releases = 0;
        deleting(&object->base.base);
        Require(releases == 3 && released[0] == second && released[1] == first && released[2] == object,
            "filesystem_error releases paths then the guest object");
    }
}

}

int main() {
    const std::array<void*, 10> api{
        reinterpret_cast<void*>(&TestAllocate), reinterpret_cast<void*>(&TestFree),
        reinterpret_cast<void*>(&TestCalloc), reinterpret_cast<void*>(&TestReallocate),
        reinterpret_cast<void*>(&TestAlign), reinterpret_cast<void*>(&TestRealign),
        reinterpret_cast<void*>(&TestPosixAlign), nullptr, nullptr, nullptr};
    ApplicationHeapRegister_nid_no_patch(api.data());

    using Destructor = void (APS5_VABI*)(void*);
    const Destructor destructors[] = {
        sceLibcInternalUnknown6_112_6yplvTHbxpE_nid_no_patch,
        _ZNSt7codecvtIwc9_MbstatetED0Ev_nid_postfix,
        _ZNSt8messagesIwED0Ev_nid_postfix,
        _ZNSt24_Iostream_error_categoryD0Ev_nid_postfix,
        _ZNSt8messagesIcED0Ev_nid_postfix,
        _ZNSt9money_putIwSt19ostreambuf_iteratorIwSt11char_traitsIwEEED0Ev_nid_postfix,
    };

    for (Destructor destructor : destructors) {
        void* object = _Znwm_nid_postfix(0x40);
        Require(object != nullptr, "the guest heap allocates the object");
        releases = 0;
        lastReleased = nullptr;
        destructor(object);
        Require(releases == 1 && lastReleased == object, "the deleting destructor releases the object");
    }

    CheckExceptionDeletes();
    CheckFutureDelete();
    CheckFilesystemDeletes();

    std::puts("libc internal deleting destructor checks passed");
    return 0;
}
