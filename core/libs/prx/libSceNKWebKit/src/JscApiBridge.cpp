#include "prx/libSceJsc/include/ApiBridgeExports.hpp"
#include "prx/libSceNKWebKit/include/UrlParserExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/Jsc.hpp"
#include "prx/libSceNKWebKit/src/WtfTextAbi.hpp"

#include <utility>

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

void ResizeStringBuilder(Jsc::WtfAbi::StringBuilder* builder, unsigned newSize) {
    WtfAbi::Resize(*reinterpret_cast<WtfAbi::StringBuilder*>(builder), newSize);
}

static_assert(sizeof(Jsc::WtfAbi::URL) == sizeof(WtfAbi::URL));

void RemoveUrlFragmentIdentifier(Jsc::WtfAbi::URL* guestUrl) {
    auto& url = *reinterpret_cast<WtfAbi::URL*>(guestUrl);
    if (!(url.bits & 1) || !url.impl) return;
    WtfAbi::String left = _ZN3WTF10StringImpl9substringEjj(url.impl, 0, url.queryEnd);
    WtfAbi::Release(std::exchange(url.impl, std::exchange(left.impl, nullptr)));
}

void WriteTextStreamIndent(Jsc::WtfAbi::TextStream* stream, int indent) {
    auto* text = reinterpret_cast<WtfAbi::TextStream*>(stream);
    for (int i = 0; i < indent; ++i) _ZN3WTF10TextStreamlsEPKc(text, "  ");
}

void AppendNumberRespectingIntegers(Jsc::WtfAbi::TextStream* stream, double value) {
    WtfAbi::AppendRespectingIntegers(*reinterpret_cast<WtfAbi::TextStream*>(stream), value);
}

static_assert(sizeof(Jsc::WtfAbi::StringView) == sizeof(WtfAbi::StringView));

void ConstructUrlFromBase(Jsc::WtfAbi::URL* url, const Jsc::WtfAbi::URL* base, const Jsc::WtfAbi::String* relative, const void* encoding) {
    _ZN3WTF3URLC2ERKS0_RKNS_6StringEPKNS_15URLTextEncodingE(reinterpret_cast<WtfAbi::URL*>(url), reinterpret_cast<const WtfAbi::URL*>(base),
        reinterpret_cast<const WtfAbi::String*>(relative), encoding);
}

void SetPassword(Jsc::WtfAbi::URL* url, Jsc::WtfAbi::StringView password) {
    WtfAbi::SetUrlPassword(reinterpret_cast<WtfAbi::URL*>(url), WtfAbi::StringView{password.characters, password.length, password.is8Bit});
}

constexpr Jsc::ApiBridge bridge{&MakeString, &HasPropertyForKey, &ResizeStringBuilder, &RemoveUrlFragmentIdentifier,
    &WriteTextStreamIndent, &AppendNumberRespectingIntegers, &ConstructUrlFromBase, &SetPassword};
const bool bridgeRegistered = (RegisterApiBridge_nid_no_patch(&bridge), true);

}

}
