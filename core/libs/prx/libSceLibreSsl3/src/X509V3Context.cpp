#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" void APS5_VABI X509V3_set_ctx_nid_postfix(LibreSsl3X509V3Context* context, LibreSslX509* issuer, LibreSslX509* subject,
                                                   void* request, void* crl, int flags) {
    if (!context) throw std::invalid_argument("X509V3_set_ctx requires a context");
    context->issuerCertificate = issuer;
    context->subjectCertificate = subject;
    context->crl = crl;
    context->subjectRequest = request;
    context->flags = flags;
}
