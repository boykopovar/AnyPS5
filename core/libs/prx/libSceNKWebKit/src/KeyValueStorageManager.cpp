#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKKeyValueStorageManagerGetCreationTimeKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKKeyValueStorageManagerCreationTimeKey"));
}

const void* APS5_VABI WKKeyValueStorageManagerGetModificationTimeKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKKeyValueStorageManagerModificationTimeKey"));
}

const void* APS5_VABI WKKeyValueStorageManagerGetOriginKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKKeyValueStorageManagerOriginKey"));
}

}
