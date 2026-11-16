#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"

namespace NKWebKit {

namespace {

OpaqueJSContext* Context(const void* context) {
    return const_cast<OpaqueJSContext*>(static_cast<const OpaqueJSContext*>(context));
}

const void* MakeString(const void* context, const GuestJSString* string) {
    auto* ctx = Context(context);
    if (!ctx->group) return nullptr;
    JSValue value = JsStringToJsValue(ctx->qctx, string);
    if (JS_IsException(value)) {
        JS_FreeValue(ctx->qctx, JS_GetException(ctx->qctx));
        return nullptr;
    }
    return ctx->group->ToHandle(ctx, value);
}

bool HasPropertyForKey(const void* context, const void* objectRef, const void* keyRef, const void** exceptionRef) {
    auto* ctx = Context(context);
    const auto* object = static_cast<const OpaqueJSValue*>(objectRef);
    const auto* key = static_cast<const OpaqueJSValue*>(keyRef);
    if (!object || !key || !object->alive || !key->alive) return false;
    auto* exception = reinterpret_cast<JSValueRef*>(exceptionRef);
    JSAtom atom = JS_ValueToAtom(ctx->qctx, key->qval);
    if (atom == JS_ATOM_NULL) {
        CapturePendingException(ctx, exception);
        return false;
    }
    const int result = JS_HasProperty(ctx->qctx, object->qval, atom);
    JS_FreeAtom(ctx->qctx, atom);
    if (result < 0) {
        CapturePendingException(ctx, exception);
        return false;
    }
    return result != 0;
}

constexpr Jsc::ApiBridge bridge{&MakeString, &HasPropertyForKey};
const bool bridgeRegistered = (RegisterApiBridge_nid_no_patch(&bridge), true);

}

}
