#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cstdint>
#include <stdexcept>

namespace {

void FreeName(LibreSslX509Name*& name) {
    if (!name) return;
    if (name->entries || name->bytes) NotImplemented_nid_no_patch("X509_INFO_free: name entries");
    free_nid_postfix(name->canonEnc);
    free_nid_postfix(name);
}

void FreeCertificate(LibreSslX509* certificate) {
    if (!certificate) return;
    if (std::atomic_ref<std::int32_t>(certificate->references).fetch_sub(1) > 1) return;
    if (certificate->signatureAlgorithm) NotImplemented_nid_no_patch("X509_INFO_free: signature algorithm");
    auto* info = static_cast<LibreSslX509CertInfo*>(certificate->certificateInfo);
    if (info) {
        if (info->signature || info->validity || info->key || info->extensions)
            NotImplemented_nid_no_patch("X509_INFO_free: certificate data other than names, integers and the encoding");
        for (void** text : {&info->version, &info->serialNumber, &info->issuerUid, &info->subjectUid}) {
            auto* string = static_cast<Asn1String*>(*text);
            if (!string) continue;
            free_nid_postfix(string->data);
            free_nid_postfix(string);
        }
        auto* issuer = static_cast<LibreSslX509Name*>(info->issuer);
        FreeName(issuer);
        info->issuer = issuer;
        auto* subject = static_cast<LibreSslX509Name*>(info->subject);
        FreeName(subject);
        info->subject = subject;
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

}

extern "C" {

void APS5_VABI X509_INFO_free_nid_postfix(LibreSslX509Info* info) {
    if (!info) return;
    if (std::atomic_ref<std::int32_t>(info->references).fetch_sub(1) > 1) return;
    if (info->crl) NotImplemented_nid_no_patch("X509_INFO_free: CRL");
    if (info->privateKey) NotImplemented_nid_no_patch("X509_INFO_free: private key");
    FreeCertificate(info->certificate);
    free_nid_postfix(info->encData);
    free_nid_postfix(info);
}

}
