#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <typeinfo>

struct TypeRecord { const void* const* vtable; const char* name; const TypeRecord* base; };
struct Vtable { std::ptrdiff_t offset; const TypeRecord* type; const void* destroy; const void* deleteObject; const char* (APS5_VABI *what)(const LibcException::ExceptionObject*); };

extern "C" {
extern const TypeRecord _ZTISt9bad_alloc_nid_postfix;
extern const TypeRecord _ZTISt20bad_array_new_length_nid_postfix;
extern const Vtable _ZTVSt20bad_array_new_length_nid_postfix;
}

static void Require(bool value) { if (!value) std::abort(); }

static const std::type_info* CaptureThrown(void (*thrower)()) {
    const std::type_info* caught = nullptr;
    try {
        thrower();
    } catch (...) {
        caught = __cxa_current_exception_type_nid_postfix();
    }
    return caught;
}

int main() {
    const auto& type = _ZTISt20bad_array_new_length_nid_postfix;
    Require(std::strcmp(type.name, "St20bad_array_new_length") == 0);
    Require(type.base == &_ZTISt9bad_alloc_nid_postfix);
    const auto& table = _ZTVSt20bad_array_new_length_nid_postfix;
    Require(table.offset == 0 && table.type == &type && table.destroy != nullptr && table.deleteObject != nullptr && table.what != nullptr);

    LibcException::ExceptionObject first{};
    _ZNSt20bad_array_new_lengthC1Ev_nid_postfix(&first);
    Require(first.vtable == &table.destroy);
    Require(std::strcmp(_ZNKSt20bad_array_new_length4whatEv_nid_postfix(&first), "bad allocation") == 0);
    Require(std::strcmp(table.what(&first), "bad allocation") == 0);
    _ZNSt20bad_array_new_lengthD1Ev_nid_postfix(&first);

    LibcException::ExceptionObject second{};
    _ZNSt20bad_array_new_lengthC2Ev_nid_postfix(&second);
    Require(second.vtable == &table.destroy);
    Require(std::strcmp(_ZNKSt20bad_array_new_length4whatEv_nid_postfix(&second), "bad allocation") == 0);
    _ZNSt20bad_array_new_lengthD2Ev_nid_postfix(&second);

    auto* allocated = static_cast<LibcException::ExceptionObject*>(std::malloc(sizeof(LibcException::ExceptionObject)));
    Require(allocated != nullptr);
    *allocated = {};
    _ZNSt20bad_array_new_lengthC1Ev_nid_postfix(allocated);
    Require(allocated->vtable == &table.destroy);
    _ZNSt20bad_array_new_lengthD0Ev_nid_postfix(allocated);

    const auto* caughtThrow = CaptureThrown([] { _ZSt6_ThrowRKSt20bad_array_new_length_nid_postfix(nullptr); });
    Require(caughtThrow != nullptr && caughtThrow->name() != nullptr && std::strcmp(caughtThrow->name(), type.name) == 0);

    const auto* caughtCxa = CaptureThrown([] { __cxa_throw_bad_array_new_length_nid_postfix(); });
    Require(caughtCxa != nullptr && caughtCxa->name() != nullptr && std::strcmp(caughtCxa->name(), type.name) == 0);
    return 0;
}
