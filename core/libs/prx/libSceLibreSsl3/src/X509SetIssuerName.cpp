#include "prx/libSceLibreSsl3/include/Api.hpp"

extern "C" int APS5_VABI X509_set_issuer_name_nid_postfix(LibreSsl3Certificate* certificate, void* name) {
    if (!certificate || !certificate->certInfo) return 0;
    const_cast<LibreSsl3CertificateInfo*>(certificate->certInfo)->issuer = name;
    return 1;
}
