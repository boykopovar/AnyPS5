#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <stdexcept>
#include <string>

namespace {

constexpr int RsaType = 6;
constexpr int RsaPssType = 912;
constexpr int EcType = 408;
constexpr int EvpLibrary = 6;
constexpr int AnyFunction = 0xfff;
constexpr int ExpectingRsaKey = 127;

void PutError(int reason) {
    LibreSslPutError_nid_no_patch(EvpLibrary, AnyFunction, reason);
}

LibreSsl3EvpPkey ReadKey_nid_no_patch(const LibreSsl3EvpPkey* key, const char* function) {
    if (!key)
        throw std::invalid_argument(std::string(function) + ": null EVP_PKEY");
    LibreSsl3EvpPkey guestKey{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&guestKey, key, sizeof(guestKey));
    errno = savedErrno;
    if (!ok)
        throw std::invalid_argument(std::string(function) + ": unreadable EVP_PKEY");
    return guestKey;
}

void EnsureWritableKey_nid_no_patch(LibreSsl3EvpPkey* key, const char* function) {
    const int savedErrno = errno;
    const bool writable = GuestWritable_nid_no_patch(key, sizeof(*key));
    errno = savedErrno;
    if (!writable)
        throw std::invalid_argument(std::string(function) + ": unwritable EVP_PKEY");
}

void WriteKey_nid_no_patch(LibreSsl3EvpPkey* key, const LibreSsl3EvpPkey& value, const char* function) {
    const int savedErrno = errno;
    const bool ok = GuestCopyOut_nid_no_patch(key, &value, sizeof(value));
    errno = savedErrno;
    if (!ok)
        throw std::invalid_argument(std::string(function) + ": unwritable EVP_PKEY");
}

bool PrepareAssign_nid_no_patch(LibreSsl3EvpPkey* key, void* data, LibreSsl3EvpPkey& value, const char* function) {
    value = ReadKey_nid_no_patch(key, function);
    if (!data)
        return false;
    EnsureWritableKey_nid_no_patch(key, function);
    if (value.method || value.key)
        NotImplemented_nid_no_patch((std::string(function) + " of existing native key data").c_str());
    return true;
}

}

extern "C" {

LibreSsl3Rsa* APS5_VABI EVP_PKEY_get1_RSA_nid_disambig1_nid_postfix(LibreSsl3EvpPkey* key) {
    const LibreSsl3EvpPkey value = ReadKey_nid_no_patch(key, __func__);
    if (value.type != RsaType && value.type != RsaPssType) {
        PutError(ExpectingRsaKey);
        return nullptr;
    }
    auto* rsa = static_cast<LibreSsl3Rsa*>(value.key);
    if (!rsa)
        throw std::invalid_argument(std::string(__func__) + ": RSA key is null");
    RSA_up_ref_nid_postfix(rsa);
    return rsa;
}

int APS5_VABI EVP_PKEY_set1_EC_KEY_nid_postfix(LibreSsl3EvpPkey* key, LibreSsl3EcKey* ec) {
    LibreSsl3EvpPkey value{};
    if (!PrepareAssign_nid_no_patch(key, ec, value, __func__))
        return 0;
    EC_KEY_up_ref_nid_postfix(ec);
    value.type = EcType;
    value.key = ec;
    WriteKey_nid_no_patch(key, value, __func__);
    return 1;
}

int APS5_VABI EVP_PKEY_assign_nid_postfix(LibreSsl3EvpPkey* key, int type, void* data) {
    LibreSsl3EvpPkey value{};
    if (!PrepareAssign_nid_no_patch(key, data, value, __func__))
        return 0;
    value.type = type;
    value.key = data;
    WriteKey_nid_no_patch(key, value, __func__);
    return 1;
}

}
