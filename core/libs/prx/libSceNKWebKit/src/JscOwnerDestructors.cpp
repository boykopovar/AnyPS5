#include "prx/libSceNKWebKit/include/JscOwnerDestructorsExports.hpp"

#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

extern "C" {

void APS5_VABI _ZN3JSC15WeakHandleOwnerD0Ev(void* self) {
    if (!self) throw std::invalid_argument("JSC::WeakHandleOwner deleting destructor requires an object");
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI _ZN3JSC20TypedArrayControllerD0Ev(void* self) {
    if (!self) throw std::invalid_argument("JSC::TypedArrayController deleting destructor requires an object");
    _ZN3WTF8fastFreeEPv(self);
}

}
