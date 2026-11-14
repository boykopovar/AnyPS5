#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <atomic>
#include <climits>
#include <cinttypes>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>
#include "SceTypes.hpp"
#include "prx/libSceLibreSsl/include/Bio.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libSceLibreSsl/ErrorQueue.hpp"
#include "src/PemReadBio.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/Api.hpp"
#include "mbedtls/des.h"

namespace {

constexpr std::size_t DesScheduleBytes = 128;
constexpr int CryptoNumLocks = 41;
constexpr int Asn1Library = 13;
constexpr int SslLibrary = 20;
constexpr int ErrorFunctionUnknown = 0xfff;
constexpr int Asn1BadObjectHeader = 102;
constexpr int Asn1HeaderTooLong = 123;
constexpr int Asn1NestedError = 58;
constexpr int Asn1SequenceNotConstructed = 149;
constexpr int Asn1TooLong = 155;
constexpr int Asn1WrongTag = 168;
constexpr int ErrorAsn1Library = 13;

void PutAsn1Error(int reason) {
    LibreSslPutError_nid_no_patch(Asn1Library, ErrorFunctionUnknown, reason, __FILE__, __LINE__);
}

bool ParseX509OuterSequenceHeader(const unsigned char* data, int length) {
    if (length == 0) {
        PutAsn1Error(Asn1HeaderTooLong);
        PutAsn1Error(Asn1BadObjectHeader);
        PutAsn1Error(Asn1NestedError);
        return false;
    }

    std::size_t cursor = 0;
    const std::size_t available = static_cast<std::size_t>(length);
    const unsigned char first = data[cursor++];
    const bool constructed = (first & 0x20) != 0;
    const int objectClass = first & 0xc0;
    int tag = first & 0x1f;
    if (tag == 0x1f) {
        if (cursor == available) {
            PutAsn1Error(Asn1HeaderTooLong);
            PutAsn1Error(Asn1BadObjectHeader);
            PutAsn1Error(Asn1NestedError);
            return false;
        }
        std::uint64_t highTag = 0;
        unsigned char byte = data[cursor++];
        while ((byte & 0x80) != 0) {
            highTag = (highTag << 7) | (byte & 0x7f);
            if (cursor == available || highTag > (static_cast<std::uint64_t>(INT_MAX) >> 7)) {
                PutAsn1Error(Asn1HeaderTooLong);
                PutAsn1Error(Asn1BadObjectHeader);
                PutAsn1Error(Asn1NestedError);
                return false;
            }
            byte = data[cursor++];
        }
        highTag = (highTag << 7) | (byte & 0x7f);
        if (highTag > static_cast<std::uint64_t>(INT_MAX)) {
            PutAsn1Error(Asn1HeaderTooLong);
            PutAsn1Error(Asn1BadObjectHeader);
            PutAsn1Error(Asn1NestedError);
            return false;
        }
        tag = static_cast<int>(highTag);
    }

    if (cursor == available) {
        PutAsn1Error(Asn1HeaderTooLong);
        PutAsn1Error(Asn1BadObjectHeader);
        PutAsn1Error(Asn1NestedError);
        return false;
    }

    const unsigned char firstLength = data[cursor++];
    bool indefinite = false;
    std::uint64_t contentLength = 0;
    if (firstLength == 0x80) {
        indefinite = true;
        if (!constructed) {
            PutAsn1Error(Asn1HeaderTooLong);
            PutAsn1Error(Asn1BadObjectHeader);
            PutAsn1Error(Asn1NestedError);
            return false;
        }
    } else if ((firstLength & 0x80) == 0) {
        contentLength = firstLength;
    } else {
        std::size_t lengthBytes = firstLength & 0x7f;
        if (lengthBytes > available - cursor) {
            PutAsn1Error(Asn1HeaderTooLong);
            PutAsn1Error(Asn1BadObjectHeader);
            PutAsn1Error(Asn1NestedError);
            return false;
        }
        while (lengthBytes != 0 && data[cursor] == 0) {
            ++cursor;
            --lengthBytes;
        }
        if (lengthBytes > sizeof(std::int64_t)) {
            PutAsn1Error(Asn1HeaderTooLong);
            PutAsn1Error(Asn1BadObjectHeader);
            PutAsn1Error(Asn1NestedError);
            return false;
        }
        for (std::size_t index = 0; index < lengthBytes; ++index)
            contentLength = (contentLength << 8) | data[cursor++];
        if (contentLength > static_cast<std::uint64_t>(std::numeric_limits<std::int64_t>::max())) {
            PutAsn1Error(Asn1HeaderTooLong);
            PutAsn1Error(Asn1BadObjectHeader);
            PutAsn1Error(Asn1NestedError);
            return false;
        }
    }

    if (!indefinite && contentLength > available - cursor) {
        PutAsn1Error(Asn1TooLong);
        PutAsn1Error(Asn1BadObjectHeader);
        PutAsn1Error(Asn1NestedError);
        return false;
    }
    if (objectClass != 0 || tag != 16) {
        PutAsn1Error(Asn1WrongTag);
        PutAsn1Error(Asn1NestedError);
        return false;
    }
    if (!constructed) {
        PutAsn1Error(Asn1SequenceNotConstructed);
        return false;
    }
    return true;
}

static_assert(sizeof(CryptoThreadId) == 16);

std::atomic<void*> lockingCallback{nullptr};
std::atomic<void*> addLockCallback{nullptr};
std::atomic<void*> threadIdCallback{nullptr};

struct X509Extension {
    void* object;
    std::int32_t critical;
    void* value;
};

static_assert(sizeof(X509Extension) == 24);
static_assert(offsetof(X509Extension, object) == 0);
static_assert(offsetof(X509Extension, critical) == 8);
static_assert(offsetof(X509Extension, value) == 16);

} // namespace

extern "C" {

APS5_DUMMY_FUN
int APS5_VABI BIO_printf_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI BIO_write_nid_postfix(LibreSsl::Bio* bio, const void* source, int size) {
    return LibreSslBioWrite_nid_no_patch(bio, source, size);
}

void APS5_VABI CONF_modules_free_nid_postfix(void) {}

int APS5_VABI CRYPTO_THREADID_set_callback_nid_postfix(void* callback) {
    void* expected = nullptr;
    return threadIdCallback.compare_exchange_strong(expected, callback) ? 1 : 0;
}

void APS5_VABI CRYPTO_THREADID_set_numeric_nid_postfix(CryptoThreadId* id, std::uint64_t value) {
    if (id == nullptr) APS5_INVALID_ARG_EX;
    *id = {};
    id->val = value;
}

int APS5_VABI CRYPTO_num_locks_nid_postfix(void) {
    return CryptoNumLocks;
}

void APS5_VABI CRYPTO_set_add_lock_callback_nid_postfix(void* callback) {
    addLockCallback.store(callback);
}

void APS5_VABI CRYPTO_set_locking_callback_nid_postfix(void* callback) {
    lockingCallback.store(callback);
}

void APS5_VABI DES_ecb_encrypt_nid_postfix(const unsigned char* input, unsigned char* output, const unsigned char* schedule, int enc) {
    if (input == nullptr || output == nullptr || schedule == nullptr) APS5_INVALID_ARG_EX;
    mbedtls_des_context context;
    mbedtls_des_init(&context);
    unsigned char block[MBEDTLS_DES_KEY_SIZE];
    std::memcpy(block, input, sizeof(block));
    int status = enc ? mbedtls_des_setkey_enc(&context, schedule) : mbedtls_des_setkey_dec(&context, schedule);
    if (status == 0) status = mbedtls_des_crypt_ecb(&context, block, output);
    mbedtls_des_free(&context);
    if (status != 0) throw std::runtime_error("DES_ecb_encrypt: mbedtls failed");
}

int APS5_VABI DES_set_key_nid_postfix(const unsigned char* key, unsigned char* schedule) {
    if (key == nullptr || schedule == nullptr) APS5_INVALID_ARG_EX;
    std::memset(schedule, 0, DesScheduleBytes);
    std::memcpy(schedule, key, MBEDTLS_DES_KEY_SIZE);
    return 0;
}

void APS5_VABI DES_set_odd_parity_nid_postfix(unsigned char* key) {
    if (key == nullptr) APS5_INVALID_ARG_EX;
    mbedtls_des_key_set_parity(key);
}

int APS5_VABI DTLSv1_client_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI DTLSv1_server_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_get0_private_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI EC_KEY_set_private_key_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_get_id_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI ENGINE_set_default_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


void APS5_VABI OCSP_BASICRESP_free_nid_postfix(void* response) {
    if (response != nullptr) NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI OCSP_basic_verify_nid_postfix(LibreSslOcspBasicResponse* response, LibreSslStack* certificates, LibreSslX509Store* store, std::uint64_t flags) {
    constexpr int responderIdName = 0;
    constexpr int sha1DigestLength = 20;
    constexpr std::uint64_t noIntern = 0x2;
    static_cast<void>(store);
    if (!response) NotImplemented_nid_no_patch(__func__);
    const auto* const responseData = response->tbsResponseData;
    if (!responseData) NotImplemented_nid_no_patch(__func__);
    const auto* const responderId = responseData->responderId;
    if (!responderId) NotImplemented_nid_no_patch(__func__);

    const auto findKnownMissing = [responderId](const LibreSslStack* stack) {
        if (responderId->type == responderIdName) {
            static_cast<void>(responderId->value.byName);
        } else {
            const auto* const keyHash = responderId->value.byKey;
            if (!keyHash) return false;
            if (keyHash->length != sha1DigestLength) return true;
            static_cast<void>(keyHash->data);
        }
        return !stack || stack->num <= 0;
    };

    if (!findKnownMissing(certificates)) NotImplemented_nid_no_patch(__func__);
    if ((flags & noIntern) == 0 && !findKnownMissing(response->certs)) NotImplemented_nid_no_patch(__func__);
    LibreSslPutError_nid_no_patch(39, 0xfff, 118);
    return 0;
}

void* APS5_VABI OCSP_response_get1_basic_nid_postfix(LibreSslOcspResponse* response) {
    if (!response) throw std::invalid_argument(std::string(__func__) + ": null response");
    if (!response->responseBytes) {
        LibreSslPutError_nid_no_patch(39, 0xfff, 108);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI OPENSSL_add_all_algorithms_noconf_nid_postfix(void) {}

void APS5_VABI OPENSSL_load_builtin_modules_nid_postfix(void) {}

int APS5_VABI PEM_X509_INFO_read_bio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

extern "C" LibreSslX509* PEM_read_X509_impl_nid_no_patch(void* stream, LibreSslX509** certificate,
                                                         LibreSslPemPasswordCallback passwordCallback,
                                                         void* userData);

LibreSslX509* APS5_VABI PEM_read_X509_nid_postfix(void* stream, LibreSslX509** certificate,
                                                 LibreSslPemPasswordCallback passwordCallback, void* userData) {
    return PEM_read_X509_impl_nid_no_patch(stream, certificate, passwordCallback, userData);
}

LibreSslX509* APS5_VABI PEM_read_bio_X509_AUX_nid_postfix(LibreSsl::Bio* bio, LibreSslX509** certificate,
                                                          LibreSslPemPasswordCallback callback, void* userData) {
    return LibreSsl::ReadPemRecord(bio, certificate, callback, userData, "TRUSTED CERTIFICATE", __func__);
}

int APS5_VABI PEM_write_bio_X509_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI PKCS12_PBE_add_nid_postfix(void) {}

void APS5_VABI PKCS12_free_nid_postfix(void* pkcs12) {
    if (pkcs12 == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI PKCS12_parse_nid_postfix(void* p12, const char*, void**, void**, void**) {
    if (!p12) {
        LibreSslPutError_nid_no_patch(35, 0xfff, 105, __FILE__, __LINE__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI RSA_generate_key_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_load_verify_mem_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set1_groups_list_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_CTX_set_alpn_protos_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}



int APS5_VABI SSL_CTX_use_PrivateKey_nid_postfix(LibreSslCtx*, void* pkey) {
    if (!pkey) {
        LibreSslPutError_nid_no_patch(20, 0xfff, 67);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_use_PrivateKey_file_nid_postfix(LibreSslCtx* ctx, const char* file, int type) {
    constexpr int ErrorLibrarySsl = 20;
    constexpr int ErrorFunctionUnknown = 0xfff;
    constexpr int ErrorReasonBuffer = 7;
    constexpr int ErrorReasonSystem = 2;
    constexpr int SslFileTypePem = 1;
    constexpr int SslFileTypeAsn1 = 2;
    constexpr int BioControlSetFilename = 108;
    constexpr int BioClose = 0x01;
    constexpr int BioFileRead = 0x02;

    LibreSsl::Bio* input = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    if (input == nullptr) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, ErrorReasonBuffer, __FILE__, __LINE__);
        return 0;
    }
    struct BioGuard {
        LibreSsl::Bio* bio;
        ~BioGuard() { BIO_free_nid_postfix(bio); }
    } guard{input};

    if (BIO_ctrl_nid_postfix(input, BioControlSetFilename, BioClose | BioFileRead, const_cast<char*>(file)) <= 0) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, ErrorReasonSystem, __FILE__, __LINE__);
        return 0;
    }
    if (type != SslFileTypeAsn1 && type != SslFileTypePem) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, 124, __FILE__, __LINE__);
        return 0;
    }
    static_cast<void>(ctx);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_use_certificate_ASN1_nid_postfix(LibreSslCtx* context, int length, const unsigned char* data) {
    static_cast<void>(context);
    if (length < 0) {
        if (length == INT_MIN || data == nullptr || (data[0] & 0x1f) == 0x1f) NotImplemented_nid_no_patch(__func__);
        PutAsn1Error(Asn1HeaderTooLong);
        PutAsn1Error(Asn1BadObjectHeader);
        PutAsn1Error(Asn1NestedError);
        LibreSslPutError_nid_no_patch(SslLibrary, ErrorFunctionUnknown, ErrorAsn1Library, __FILE__, __LINE__);
        return 0;
    }
    if (length > 0 && data == nullptr) NotImplemented_nid_no_patch(__func__);
    if (!ParseX509OuterSequenceHeader(data, length)) {
        LibreSslPutError_nid_no_patch(SslLibrary, ErrorFunctionUnknown, ErrorAsn1Library, __FILE__, __LINE__);
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_use_certificate_chain_file_nid_postfix(LibreSslCtx* ctx, const char* file) {
    constexpr int ErrorLibrarySsl = 20;
    constexpr int ErrorFunctionUnknown = 0xfff;
    constexpr int ErrorReasonBuffer = 7;
    constexpr int ErrorReasonSystem = 2;
    constexpr int BioControlSetFilename = 108;
    constexpr int BioClose = 0x01;
    constexpr int BioFileRead = 0x02;

    LibreSsl::Bio* input = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    if (input == nullptr) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, ErrorReasonBuffer, __FILE__, __LINE__);
        return 0;
    }
    struct BioGuard {
        LibreSsl::Bio* bio;
        ~BioGuard() { BIO_free_nid_postfix(bio); }
    } guard{input};

    if (BIO_ctrl_nid_postfix(input, BioControlSetFilename, BioClose | BioFileRead, const_cast<char*>(file)) <= 0) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, ErrorReasonSystem, __FILE__, __LINE__);
        return 0;
    }
    ERR_clear_error_nid_postfix();
    static_cast<void>(ctx);
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_CTX_use_certificate_file_nid_postfix(LibreSslCtx* ctx, const char* file, int type) {
    constexpr int ErrorLibrarySsl = 20;
    constexpr int ErrorFunctionUnknown = 0xfff;
    constexpr int ErrorReasonBuffer = 7;
    constexpr int ErrorReasonSystem = 2;
    constexpr int SslFileTypePem = 1;
    constexpr int SslFileTypeAsn1 = 2;
    constexpr int BioControlSetFilename = 108;
    constexpr int BioClose = 0x01;
    constexpr int BioFileRead = 0x02;

    LibreSsl::Bio* input = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    if (input == nullptr) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, ErrorReasonBuffer, __FILE__, __LINE__);
        return 0;
    }
    struct BioGuard {
        LibreSsl::Bio* bio;
        ~BioGuard() { BIO_free_nid_postfix(bio); }
    } guard{input};

    if (BIO_ctrl_nid_postfix(input, BioControlSetFilename, BioClose | BioFileRead, const_cast<char*>(file)) <= 0) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, ErrorReasonSystem, __FILE__, __LINE__);
        return 0;
    }
    if (type != SslFileTypeAsn1 && type != SslFileTypePem) {
        LibreSslPutError_nid_no_patch(ErrorLibrarySsl, ErrorFunctionUnknown, 124, __FILE__, __LINE__);
        return 0;
    }
    static_cast<void>(ctx);
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI SSL_SESSION_free_nid_postfix(void* session) {
    if (session == nullptr) return;
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI SSL_accept_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_connect_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get0_param_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void* APS5_VABI SSL_get1_session_nid_postfix(LibreSslSsl* ssl) {
    if (!ssl) NotImplemented_nid_no_patch(__func__);
    if (!ssl->session) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI SSL_get_error_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_ex_data_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_get_fd_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

LibreSslStack* APS5_VABI SSL_get_peer_cert_chain_nid_postfix(const LibreSslSsl* ssl) {
    if (!ssl || !ssl->session) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}

LibreSslX509* APS5_VABI SSL_get_peer_certificate_nid_postfix(const LibreSslSsl* ssl) {
    if (!ssl || !ssl->session) return nullptr;
    NotImplemented_nid_no_patch(__func__);
}


int APS5_VABI SSL_get_servername_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI SSL_library_init_nid_postfix(void) {
    return 1;
}

void APS5_VABI SSL_load_error_strings_nid_postfix(void) {}

int APS5_VABI SSL_new_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_peek_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_pending_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_read_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_SSL_CTX_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_bio_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_fd_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_set_session_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_shutdown_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_use_PrivateKey_ASN1_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_use_certificate_ASN1_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSL_write_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

std::uint64_t APS5_VABI SSLeay_nid_postfix(void) {
    return 0x20000000;
}

int APS5_VABI SSLv23_client_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI SSLv23_server_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI TLS_client_method_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI UI_OpenSSL_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509V3_EXT_print_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_EXTENSION_get_critical_nid_postfix(const void* extension) {
    if (!extension) return 0;
    std::int32_t critical;
    std::memcpy(&critical, static_cast<const unsigned char*>(extension) + offsetof(X509Extension, critical), sizeof(critical));
    return critical > 0;
}

void* APS5_VABI X509_EXTENSION_get_data_nid_postfix(const void* extension) {
    if (!extension) return nullptr;
    void* value;
    std::memcpy(&value, static_cast<const unsigned char*>(extension) + offsetof(X509Extension, value), sizeof(value));
    return value;
}

void* APS5_VABI X509_EXTENSION_get_object_nid_postfix(const void* extension) {
    if (!extension) return nullptr;
    void* object;
    std::memcpy(&object, static_cast<const unsigned char*>(extension) + offsetof(X509Extension, object), sizeof(object));
    return object;
}

int APS5_VABI X509_INFO_free_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_NAME_print_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_STORE_add_crl_nid_postfix(LibreSslX509Store*, void* crl) {
    if (!crl) return 0;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_check_issued_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_get_ext_d2i_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_get_pubkey_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_load_crl_file_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_print_ex_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI X509_verify_cert_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

const char* APS5_VABI X509_verify_cert_error_string_nid_postfix(std::int64_t error) {
    static constexpr const char* messages[] {
        "ok",
        nullptr,
        "unable to get issuer certificate",
        "unable to get certificate CRL",
        "unable to decrypt certificate's signature",
        "unable to decrypt CRL's signature",
        "unable to decode issuer public key",
        "certificate signature failure",
        "CRL signature failure",
        "certificate is not yet valid",
        "certificate has expired",
        "CRL is not yet valid",
        "CRL has expired",
        "format error in certificate's notBefore field",
        "format error in certificate's notAfter field",
        "format error in CRL's lastUpdate field",
        "format error in CRL's nextUpdate field",
        "out of memory",
        "self signed certificate",
        "self signed certificate in certificate chain",
        "unable to get local issuer certificate",
        "unable to verify the first certificate",
        "certificate chain too long",
        "certificate revoked",
        "invalid CA certificate",
        "path length constraint exceeded",
        "unsupported certificate purpose",
        "certificate not trusted",
        "certificate rejected",
        "subject issuer mismatch",
        "authority and subject key identifier mismatch",
        "authority and issuer serial number mismatch",
        "key usage does not include certificate signing",
        "unable to get CRL issuer certificate",
        "unhandled critical extension",
        "key usage does not include CRL signing",
        "unhandled critical CRL extension",
        "invalid non-CA certificate (has CA markings)",
        "proxy path length constraint exceeded",
        "key usage does not include digital signature",
        "proxy certificates not allowed, please set the appropriate flag",
        "invalid or inconsistent certificate extension",
        "invalid or inconsistent certificate policy extension",
        "no explicit policy",
        "Different CRL scope",
        "Unsupported extension feature",
        "RFC 3779 resource not subset of parent's resources",
        "permitted subtree violation",
        "excluded subtree violation",
        "name constraints minimum and maximum not supported",
        "application verification failure",
        "unsupported name constraint type",
        "unsupported or invalid name constraint syntax",
        "unsupported or invalid name syntax",
        "CRL path validation error",
    };
    const auto code = static_cast<std::uint32_t>(error);
    if (code < sizeof(messages) / sizeof(messages[0]) && messages[code]) return messages[code];
    static char unknown[100];
    std::snprintf(unknown, sizeof(unknown), "error number %" PRId64, error);
    return unknown;
}

void* APS5_VABI d2i_PKCS12_bio_nid_postfix(LibreSsl::Bio* bio, void** output) {
    ERR_clear_error_nid_postfix();
    constexpr int HeaderSize = 8;
    char header[HeaderSize]{};
    int count = 0;
    if (!bio || !bio->method || !bio->method->read) {
        LibreSslPutError_nid_no_patch(32, 0xfff, 121, __FILE__, __LINE__);
        count = -2;
    } else {
        const auto callback = bio->callback;
        count = callback ? static_cast<int>(callback(bio, 2, header, HeaderSize, 0, 1)) : 1;
        if (count > 0) {
            if (!bio->initialized) {
                LibreSslPutError_nid_no_patch(32, 0xfff, 120, __FILE__, __LINE__);
                count = -2;
            } else {
                count = bio->method->read(bio, header, HeaderSize);
                if (count > 0) bio->bytesRead += static_cast<std::uint64_t>(count);
                if (callback) count = static_cast<int>(callback(bio, 0x82, header, HeaderSize, 0, count));
            }
        }
    }
    if (count < 0) {
        LibreSslPutError_nid_no_patch(13, 0xfff, 142, __FILE__, __LINE__);
        return nullptr;
    }
    if (count == 0) {
        LibreSslPutError_nid_no_patch(13, 0xfff, 123, __FILE__, __LINE__);
        return nullptr;
    }
    static_cast<void>(output);
    NotImplemented_nid_no_patch(__func__);
}

void* APS5_VABI d2i_PKCS12_fp_nid_postfix(FileStream* stream, void**) {
    if (!stream) throw std::invalid_argument("d2i_PKCS12_fp: null FILE");
    ERR_clear_error_nid_postfix();
    unsigned char header[8]{};
    const auto count = fread_nid_postfix(header, 1, sizeof(header), stream);
    if (count == 0) {
        if (ferror_nid_postfix(stream)) NotImplemented_nid_no_patch(__func__);
        LibreSslPutError_nid_no_patch(13, 0xfff, 123);
        return nullptr;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI i2d_X509_PUBKEY_nid_postfix(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslGetMemoryPoolStats(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslInit(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslInitEx(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslInitEx2(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceLibreSslTerm(void) {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
