#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKPlugInInformationReplacementObscuredKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"ReplacementObscured")));
}

const void* APS5_VABI WKPluginInformationBundleIdentifierKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"BundleIdentifier")));
}

const void* APS5_VABI WKPluginInformationBundleShortVersionKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"BundleShortVersion")));
}

const void* APS5_VABI WKPluginInformationBundleVersionKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"BundleVersion")));
}

const void* APS5_VABI WKPluginInformationDefaultLoadPolicyKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"DefaultLoadPolicy")));
}

const void* APS5_VABI WKPluginInformationDisplayNameKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"DisplayName")));
}

const void* APS5_VABI WKPluginInformationFrameURLKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"FrameURL")));
}

const void* APS5_VABI WKPluginInformationHasSandboxProfileKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"HasSandboxProfile")));
}

const void* APS5_VABI WKPluginInformationMIMETypeKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"MIMEType")));
}

const void* APS5_VABI WKPluginInformationPageURLKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"PageURL")));
}

const void* APS5_VABI WKPluginInformationPathKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"Path")));
}

const void* APS5_VABI WKPluginInformationPluginURLKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"PluginURL")));
}

const void* APS5_VABI WKPluginInformationPluginspageAttributeURLKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"PluginspageAttributeURL")));
}

const void* APS5_VABI WKPluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey() {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"UpdatePastLastBlockedVersionIsKnownAvailable")));
}

}
