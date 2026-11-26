#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <atomic>

static_assert(sizeof(LibreSslX509CertInfo) == 104);
static_assert(offsetof(LibreSslX509CertInfo, serialNumber) == 8);
static_assert(offsetof(LibreSslX509CertInfo, issuer) == 24);
static_assert(offsetof(LibreSslX509CertInfo, subject) == 40);
static_assert(offsetof(LibreSslX509, certificateInfo) == 0);

namespace {

const LibreSslX509CertInfo& CertInfo(const LibreSslX509* certificate, const char* function) {
    if (!certificate) throw std::invalid_argument(std::string(function) + ": null certificate");
    if (!certificate->certificateInfo) throw std::invalid_argument(std::string(function) + ": null certificate info");
    return *static_cast<const LibreSslX509CertInfo*>(certificate->certificateInfo);
}

}

extern "C" {

void* APS5_VABI X509_get_issuer_name_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).issuer;
}

void* APS5_VABI X509_get_subject_name_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).subject;
}

void APS5_VABI X509_free_nid_postfix(LibreSslX509* certificate) {
    if (!certificate) return;
    auto* info = static_cast<LibreSslX509CertInfo*>(certificate->certificateInfo);
    if (certificate->signatureAlgorithm) NotImplemented_nid_no_patch("X509_free: signature algorithm");
    if (info && (info->signature || info->validity || info->key || info->extensions))
        NotImplemented_nid_no_patch("X509_free: certificate data other than names, integers and the encoding");
    if (std::atomic_ref<std::int32_t>(certificate->references).fetch_sub(1) > 1) return;
    if (info) {
        for (void** text : {&info->version, &info->serialNumber, &info->issuerUid, &info->subjectUid}) {
            auto* string = static_cast<Asn1String*>(*text);
            if (!string) continue;
            free_nid_postfix(string->data);
            free_nid_postfix(string);
        }
        for (void** name : {&info->issuer, &info->subject}) {
            auto* value = static_cast<LibreSslX509Name*>(*name);
            if (!value) continue;
            if (value->entries || value->bytes) NotImplemented_nid_no_patch("X509_free: name entries");
            free_nid_postfix(value->canonEnc);
            free_nid_postfix(value);
        }
        free_nid_postfix(info->enc.enc);
        free_nid_postfix(info);
    }
    auto* signature = static_cast<Asn1String*>(certificate->signature);
    if (signature) {
        free_nid_postfix(signature->data);
        free_nid_postfix(signature);
    }
    free_nid_postfix(certificate);
}

void* APS5_VABI X509_get_serialNumber_nid_postfix(const LibreSslX509* certificate) {
    return CertInfo(certificate, __func__).serialNumber;
}

}
