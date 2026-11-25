#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

extern "C" {

const void* APS5_VABI WKBundleCreateWKDataFromUInt8Array(const void* bundleRef, NKWebKit::JSContextRef contextRef, NKWebKit::JSValueRef valueRef) {
    NKWebKit::Cast<NKWebKit::Bundle>(bundleRef, __func__);
    if (!contextRef) {
        throw std::invalid_argument("contextRef must not be null");
    }
    if (!valueRef || !valueRef->alive) {
        throw std::invalid_argument("valueRef must not be null");
    }
    const auto type = JSValueGetTypedArrayType(contextRef, valueRef, nullptr);
    if (type != NKWebKit::kJSTypedArrayTypeUint8Array) {
        throw std::invalid_argument("valueRef must be a Uint8Array");
    }
    auto* obj = const_cast<NKWebKit::OpaqueJSValue*>(valueRef);
    const auto* ptr = static_cast<const std::uint8_t*>(JSObjectGetTypedArrayBytesPtr(contextRef, obj, nullptr));
    const std::size_t offset = JSObjectGetTypedArrayByteOffset(contextRef, obj, nullptr);
    const std::size_t length = JSObjectGetTypedArrayByteLength(contextRef, obj, nullptr);
    std::vector<std::uint8_t> bytes;
    if (ptr && length > 0) {
        bytes.assign(ptr + offset, ptr + offset + length);
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Data>(std::move(bytes)));
}

}
