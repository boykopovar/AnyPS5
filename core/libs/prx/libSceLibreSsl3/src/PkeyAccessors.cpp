#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <stdexcept>
#include <string>

namespace {

constexpr int RsaType = 6;
constexpr int EcType = 408;
constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int ExpectingRsaKey = 127;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, reason);
}

LibreSsl3EvpPkey& Check_nid_no_patch(LibreSsl3EvpPkey* key, const char* function) {
    if (!key) throw std::invalid_argument(std::string(function) + ": null EVP_PKEY");
    return *key;
}

int Assign_nid_no_patch(LibreSsl3EvpPkey* key, int type, void* data, const char* function) {
    auto& value = Check_nid_no_patch(key, function);
    if (!data) return 0;
    if (value.method || value.key) NotImplemented_nid_no_patch((std::string(function) + " of existing native key data").c_str());
    value.type = type;
    value.key = data;
    return 1;
}

}

extern "C" {

LibreSsl3Rsa* APS5_VABI EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* key) {
    auto& value = Check_nid_no_patch(key, __func__);
    if (value.type != RsaType) {
        PutError(ExpectingRsaKey);
        return nullptr;
    }
    auto* rsa = static_cast<LibreSsl3Rsa*>(value.key);
    if (!rsa) throw std::invalid_argument(std::string(__func__) + ": RSA key is null");
    RSA_up_ref_nid_postfix(rsa);
    return rsa;
}

int APS5_VABI EVP_PKEY_set1_EC_KEY_nid_postfix(LibreSsl3EvpPkey* key, LibreSsl3EcKey* ec) {
    const int result = Assign_nid_no_patch(key, EcType, ec, __func__);
    if (result) EC_KEY_up_ref_nid_postfix(ec);
    return result;
}

int APS5_VABI EVP_PKEY_assign_nid_postfix(LibreSsl3EvpPkey* key, int type, void* data) {
    return Assign_nid_no_patch(key, type, data, __func__);
}

}
