#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/include/WebKitConfigExports.hpp"

extern "C" {
const void* APS5_VABI WKCredentialCreate(const void* username, const void* password, std::uint32_t persistence) {
    std::u16string userString;
    std::u16string passwordString;
    if (username) {
        userString = NKWebKit::Cast<NKWebKit::String>(username, __func__)->characters;
    }
    if (password) {
        passwordString = NKWebKit::Cast<NKWebKit::String>(password, __func__)->characters;
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Credential>(std::move(userString), std::move(passwordString), persistence));
}

const void* APS5_VABI WKCredentialCreateWithCertificateInfo(const void* certificateInfo) {
    (void)certificateInfo;
    return nullptr;
}

const void* APS5_VABI WKCredentialCopyUser(const void* credential) {
    const auto* cred = NKWebKit::Cast<NKWebKit::Credential>(credential, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(cred->user));
}

}
