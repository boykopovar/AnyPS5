#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKUserScriptCreateWithSource(const void* source, std::uint32_t injectionTime, bool forMainFrameOnly) {
    std::u16string sourceString;
    if (source) {
        sourceString = NKWebKit::Cast<NKWebKit::String>(source, __func__)->characters;
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::UserScript>(std::move(sourceString), injectionTime, forMainFrameOnly));
}

const void* APS5_VABI WKUserScriptCopySource(const void* userScript) {
    const auto* script = NKWebKit::Cast<NKWebKit::UserScript>(userScript, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(script->source));
}

std::uint32_t APS5_VABI WKUserScriptGetInjectionTime(const void* userScript) {
    return NKWebKit::Cast<NKWebKit::UserScript>(userScript, __func__)->injectionTime;
}

bool APS5_VABI WKUserScriptGetMainFrameOnly(const void* userScript) {
    return NKWebKit::Cast<NKWebKit::UserScript>(userScript, __func__)->forMainFrameOnly;
}

}
