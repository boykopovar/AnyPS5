#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include <cmath>
#include <limits>
#include <string>
#include <utility>

extern "C" {

const void* APS5_VABI WKSerializedScriptValueCreate(NKWebKit::JSContextRef context, NKWebKit::JSValueRef value, NKWebKit::JSValueRef* exception) {
    if (!context || !value || !value->alive) return nullptr;
    if (JSValueIsUndefined(context, value)) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SerializedScriptValue>(std::string{}, NKWebKit::SerializedScriptValueKind::Undefined));
    }
    if (JSValueIsNumber(context, value)) {
        const double number = JSValueToNumber(context, value, exception);
        if (std::isnan(number)) {
            return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SerializedScriptValue>(std::string{}, NKWebKit::SerializedScriptValueKind::NaN));
        }
        if (std::isinf(number)) {
            const auto kind = (number > 0) ? NKWebKit::SerializedScriptValueKind::PositiveInfinity : NKWebKit::SerializedScriptValueKind::NegativeInfinity;
            return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SerializedScriptValue>(std::string{}, kind));
        }
    }
    NKWebKit::JSStringRef jsonString = JSValueCreateJSONString(context, value, 0, exception);
    if (!jsonString) return nullptr;
    const std::string utf8 = NKWebKit::JsStringToUtf8(jsonString);
    JSStringRelease_nid_postfix(jsonString);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SerializedScriptValue>(utf8, NKWebKit::SerializedScriptValueKind::Json));
}

NKWebKit::JSValueRef APS5_VABI WKSerializedScriptValueDeserialize(const void* serializedScriptValueRef, NKWebKit::JSContextRef context, NKWebKit::JSValueRef* exception) {
    if (!serializedScriptValueRef || !context) return nullptr;
    const auto* serialized = NKWebKit::Cast<NKWebKit::SerializedScriptValue>(serializedScriptValueRef, __func__);
    switch (serialized->kind) {
    case NKWebKit::SerializedScriptValueKind::Undefined:
        return JSValueMakeUndefined(context);
    case NKWebKit::SerializedScriptValueKind::NaN:
        return JSValueMakeNumber(context, std::numeric_limits<double>::quiet_NaN());
    case NKWebKit::SerializedScriptValueKind::PositiveInfinity:
        return JSValueMakeNumber(context, std::numeric_limits<double>::infinity());
    case NKWebKit::SerializedScriptValueKind::NegativeInfinity:
        return JSValueMakeNumber(context, -std::numeric_limits<double>::infinity());
    case NKWebKit::SerializedScriptValueKind::Json:
        break;
    }
    NKWebKit::JSStringRef jsonString = JSStringCreateWithUTF8CString_nid_postfix(serialized->data.c_str());
    if (!jsonString) {
        if (exception) {
            *exception = JSObjectMakeError(context, 0, nullptr, nullptr);
        }
        return nullptr;
    }
    NKWebKit::JSValueRef result = JSValueMakeFromJSONString(context, jsonString);
    JSStringRelease_nid_postfix(jsonString);
    if (!result && exception) {
        *exception = JSObjectMakeError(context, 0, nullptr, nullptr);
    }
    return result;
}

}
