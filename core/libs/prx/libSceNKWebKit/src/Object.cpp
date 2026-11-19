#include "prx/libSceNKWebKit/src/Object.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
std::uint32_t APS5_VABI WKGetTypeID(const void* object) {
    return NKWebKit::TypeID(NKWebKit::ToObject(object, __func__)->Type());
}

const void* APS5_VABI WKRetain(const void* object) {
    NKWebKit::ToObject(object, __func__)->Ref();
    return object;
}

void APS5_VABI WKRelease(const void* object) {
    NKWebKit::ToObject(object, __func__)->Deref();
}
}
