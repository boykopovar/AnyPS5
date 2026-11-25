#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/include/JscExports.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include <string>

namespace NKWebKit {

BundleRangeHandle::BundleRangeHandle() = default;

BundleRangeHandle::BundleRangeHandle(JSContextRef ctx, JSValueRef val)
    : context(ctx), value(val) {
    if (context) {
        auto globalCtx = JSContextGetGlobalContext(context);
        if (globalCtx) {
            JSGlobalContextRetain(globalCtx);
        }
    }
    if (context && value) {
        JSValueProtect(context, value);
    }
}

BundleRangeHandle::~BundleRangeHandle() {
    auto globalCtx = context ? JSContextGetGlobalContext(context) : nullptr;
    if (context && value) {
        JSValueUnprotect(context, value);
    }
    if (globalCtx) {
        JSGlobalContextRelease(globalCtx);
    }
}

namespace {

bool IsRangeObject(JSContextRef context, JSValueRef value) {
    if (!context || !value || !value->alive) {
        return false;
    }
    if (!JSValueIsObject(context, value)) {
        return false;
    }

    auto* obj = const_cast<OpaqueJSValue*>(value);

    if (obj->isObjectHandle && obj->group) {
        const auto* data = obj->group->FindCallbackData(obj);
        if (data && data->jsClass) {
            for (const auto* cls = data->jsClass; cls; cls = cls->parentClass) {
                if (cls->className == "Range") {
                    return true;
                }
            }
        }
    }

    return false;
}

RectValue ExtractBoundingRect(JSContextRef context, JSObjectRef obj) {
    RectValue result{{0.0, 0.0}, {0.0, 0.0}};
    if (!context || !obj) {
        return result;
    }

    auto readNumber = [&](JSObjectRef target, const char* propName, double defaultVal = 0.0) -> double {
        JSValueRef exception = nullptr;
        auto* p = JSStringCreateWithUTF8CString_nid_postfix(propName);
        JSValueRef v = JSObjectGetProperty(context, target, p, &exception);
        JSStringRelease_nid_postfix(p);
        if (!exception && v && JSValueIsNumber(context, v)) {
            double num = JSValueToNumber(context, v, &exception);
            if (!exception) {
                return num;
            }
        }
        return defaultVal;
    };

    auto* getRectProp = JSStringCreateWithUTF8CString_nid_postfix("getBoundingClientRect");
    if (JSObjectHasProperty(context, obj, getRectProp)) {
        JSValueRef exception = nullptr;
        JSValueRef fnVal = JSObjectGetProperty(context, obj, getRectProp, &exception);
        if (!exception && fnVal && JSValueIsObject(context, fnVal) && JSObjectIsFunction(context, const_cast<OpaqueJSValue*>(fnVal))) {
            JSValueRef rectVal = JSObjectCallAsFunction(context, const_cast<OpaqueJSValue*>(fnVal), obj, 0, nullptr, &exception);
            if (!exception && rectVal && JSValueIsObject(context, rectVal)) {
                auto* rectObj = const_cast<OpaqueJSValue*>(rectVal);
                double x = readNumber(rectObj, "x", readNumber(rectObj, "left", 0.0));
                double y = readNumber(rectObj, "y", readNumber(rectObj, "top", 0.0));
                double w = readNumber(rectObj, "width", 0.0);
                double h = readNumber(rectObj, "height", 0.0);
                JSStringRelease_nid_postfix(getRectProp);
                return RectValue{{x, y}, {w, h}};
            }
        }
    }
    JSStringRelease_nid_postfix(getRectProp);

    auto* boundProp = JSStringCreateWithUTF8CString_nid_postfix("boundingRect");
    if (JSObjectHasProperty(context, obj, boundProp)) {
        JSValueRef exception = nullptr;
        JSValueRef rectVal = JSObjectGetProperty(context, obj, boundProp, &exception);
        if (!exception && rectVal && JSValueIsObject(context, rectVal)) {
            auto* rectObj = const_cast<OpaqueJSValue*>(rectVal);
            double x = readNumber(rectObj, "x", readNumber(rectObj, "left", 0.0));
            double y = readNumber(rectObj, "y", readNumber(rectObj, "top", 0.0));
            double w = readNumber(rectObj, "width", 0.0);
            double h = readNumber(rectObj, "height", 0.0);
            JSStringRelease_nid_postfix(boundProp);
            return RectValue{{x, y}, {w, h}};
        }
    }
    JSStringRelease_nid_postfix(boundProp);

    return result;
}

}

}

extern "C" {

const void* APS5_VABI WKBundleRangeHandleCreate(NKWebKit::JSContextRef context, NKWebKit::JSValueRef value) {
    if (!context || !value || !value->alive) {
        return nullptr;
    }
    if (!NKWebKit::IsRangeObject(context, value)) {
        return nullptr;
    }
    auto handle = NKWebKit::Make<NKWebKit::BundleRangeHandle>(context, value);
    auto* obj = const_cast<NKWebKit::OpaqueJSValue*>(value);
    handle->boundingRectInWindowCoordinates = NKWebKit::ExtractBoundingRect(context, obj);
    return NKWebKit::ToRef(handle);
}

NKWebKit::RectValue APS5_VABI WKBundleRangeHandleGetBoundingRectInWindowCoordinates(const void* rangeHandleRef) {
    const auto* handle = NKWebKit::Cast<NKWebKit::BundleRangeHandle>(rangeHandleRef, __func__);
    return handle->boundingRectInWindowCoordinates;
}

}
