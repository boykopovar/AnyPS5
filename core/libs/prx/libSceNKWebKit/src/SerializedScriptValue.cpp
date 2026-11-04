#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include <string>
#include <utility>

extern "C" {

const void* APS5_VABI WKSerializedScriptValueCreate(NKWebKit::JSContextRef context, NKWebKit::JSValueRef value, NKWebKit::JSValueRef* exception) {
    if (!context || !value || !value->alive) return nullptr;
    if (JSValueIsUndefined(context, value)) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SerializedScriptValue>(std::string{}, true));
    }
    NKWebKit::JSStringRef jsonString = JSValueCreateJSONString(context, value, 0, exception);
    if (!jsonString) return nullptr;
    const std::string utf8 = NKWebKit::JsStringToUtf8(jsonString);
    JSStringRelease_nid_postfix(jsonString);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SerializedScriptValue>(utf8, false));
}

NKWebKit::JSValueRef APS5_VABI WKSerializedScriptValueDeserialize(const void* serializedScriptValueRef, NKWebKit::JSContextRef context, NKWebKit::JSValueRef* exception) {
    (void)exception;
    if (!serializedScriptValueRef || !context) return nullptr;
    const auto* serialized = NKWebKit::Cast<NKWebKit::SerializedScriptValue>(serializedScriptValueRef, __func__);
    if (serialized->isUndefined) {
        return JSValueMakeUndefined(context);
    }
    NKWebKit::JSStringRef jsonString = JSStringCreateWithUTF8CString_nid_postfix(serialized->data.c_str());
    if (!jsonString) return nullptr;
    NKWebKit::JSValueRef result = JSValueMakeFromJSONString(context, jsonString);
    JSStringRelease_nid_postfix(jsonString);
    return result;
}

}
