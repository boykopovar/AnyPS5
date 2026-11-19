#include "prx/libc/include/Api.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <typeinfo>

namespace {

void Require(bool value) { if (!value) std::abort(); }

}

int main() {
    Require(__cxa_current_exception_type_nid_postfix() == nullptr);
    Require(__cxa_current_primary_exception_nid_postfix() == nullptr);

    void* primary = nullptr;
    try { throw std::runtime_error("host failure"); }
    catch (const std::runtime_error&) {
        const std::type_info* type = __cxa_current_exception_type_nid_postfix();
        Require(type != nullptr && *type == typeid(std::runtime_error));
        primary = __cxa_current_primary_exception_nid_postfix();
    }
    Require(primary != nullptr);
    Require(__cxa_current_exception_type_nid_postfix() == nullptr);

    GuestExceptionPointer copy(nullptr);
    _ZNSt15__exception_ptr13exception_ptrC1EPv_nid_postfix(&copy, primary);
    __cxa_decrement_exception_refcount_nid_postfix(primary);
    const std::type_info* held = _ZNKSt15__exception_ptr13exception_ptr20__cxa_exception_typeEv_nid_postfix(&copy);
    Require(held != nullptr && *held == typeid(std::runtime_error));

    bool rethrown = false;
    try { _ZSt17rethrow_exceptionNSt15__exception_ptr13exception_ptrE_nid_postfix(copy); }
    catch (const std::runtime_error& error) { rethrown = std::strcmp(error.what(), "host failure") == 0; }
    Require(rethrown);

    _ZNSt15__exception_ptr13exception_ptr10_M_releaseEv_nid_postfix(&copy);
    Require(copy.object == nullptr);
    return 0;
}
