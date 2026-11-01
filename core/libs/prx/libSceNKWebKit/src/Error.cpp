#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKErrorCopyWKErrorDomain() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WebKitErrorDomain"));
}

}
