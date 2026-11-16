#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

const void* APS5_VABI WKPlugInInformationReplacementObscuredKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPlugInInformationReplacementObscuredKey"));
}

const void* APS5_VABI WKPluginInformationBundleIdentifierKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationBundleIdentifierKey"));
}

const void* APS5_VABI WKPluginInformationBundleShortVersionKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationBundleShortVersionKey"));
}

const void* APS5_VABI WKPluginInformationBundleVersionKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationBundleVersionKey"));
}

const void* APS5_VABI WKPluginInformationDefaultLoadPolicyKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationDefaultLoadPolicyKey"));
}

const void* APS5_VABI WKPluginInformationDisplayNameKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationDisplayNameKey"));
}

const void* APS5_VABI WKPluginInformationFrameURLKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationFrameURLKey"));
}

const void* APS5_VABI WKPluginInformationHasSandboxProfileKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationHasSandboxProfileKey"));
}

const void* APS5_VABI WKPluginInformationMIMETypeKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationMIMETypeKey"));
}

const void* APS5_VABI WKPluginInformationPageURLKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationPageURLKey"));
}

const void* APS5_VABI WKPluginInformationPathKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationPathKey"));
}

const void* APS5_VABI WKPluginInformationPluginURLKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationPluginURLKey"));
}

const void* APS5_VABI WKPluginInformationPluginspageAttributeURLKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationPluginspageAttributeURLKey"));
}

const void* APS5_VABI WKPluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey() {
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8("WKPluginInformationUpdatePastLastBlockedVersionIsKnownAvailableKey"));
}

}
