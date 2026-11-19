#include "prx/libSceNKWebKit/include/WebCoreClientDestructorsExports.hpp"

#include "prx/libc/include/Api.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <stdexcept>

extern "C" {

void APS5_VABI _ZN7WebCore15XPathNSResolverD0Ev(void* self) {
    if (!self) throw std::invalid_argument("WebCore::XPathNSResolver deleting destructor requires an object");
    _ZN3WTF8fastFreeEPv(self);
}

void APS5_VABI _ZN7WebCore27ContentSecurityPolicyClientD1Ev(void* self) {
    if (!self) throw std::invalid_argument("WebCore::ContentSecurityPolicyClient destructor requires an object");
}

void APS5_VABI _ZN7WebCore27ContentSecurityPolicyClientD0Ev(void* self) {
    _ZN7WebCore27ContentSecurityPolicyClientD1Ev(self);
    _ZdlPv_nid_postfix(self);
}

}
