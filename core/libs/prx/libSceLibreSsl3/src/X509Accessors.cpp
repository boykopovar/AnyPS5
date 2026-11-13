#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl3/include/Objects.hpp"
#include "prx/libc/include/General.hpp"

#include <limits>
#include <stdexcept>
#include <string>

namespace {

const LibreSsl3CertificateInfo* CertificateInfo(const LibreSsl3Certificate* certificate, const char* function) {
    if (!certificate) throw std::invalid_argument(std::string(function) + ": null certificate");
    if (!certificate->certInfo) throw std::invalid_argument(std::string(function) + ": null certificate info");
    return certificate->certInfo;
}

} // namespace

extern "C" {

LibreSsl3X509Pubkey* APS5_VABI X509_get_X509_PUBKEY_nid_postfix(const LibreSsl3Certificate* certificate) {
    return CertificateInfo(certificate, __func__)->key;
}

int APS5_VABI X509_get_signature_nid_nid_postfix(const LibreSsl3Certificate* certificate) {
    if (!certificate) throw std::invalid_argument(std::string(__func__) + ": null certificate");
    if (!certificate->sigAlg) throw std::invalid_argument(std::string(__func__) + ": null signature algorithm");
    return LibreSsl3::ObjectNid(static_cast<const LibreSslAsn1Object*>(certificate->sigAlg->algorithm));
}

int APS5_VABI X509_get_ext_by_NID_nid_postfix(const LibreSsl3Certificate* certificate, int nid, int lastpos) {
    const LibreSsl3Stack* extensions = CertificateInfo(certificate, __func__)->extensions;
    const LibreSslAsn1Object* object = LibreSsl3::ObjectByNid(nid);
    if (!object) return -2;
    if (lastpos == std::numeric_limits<int>::max()) throw std::overflow_error(std::string(__func__) + ": last position overflows");
    int position = lastpos + 1;
    if (position < 0) position = 0;
    for (; extensions && position < extensions->num; ++position) {
        if (!extensions->data) throw std::invalid_argument(std::string(__func__) + ": null extension array");
        const auto* extension = reinterpret_cast<const LibreSsl3X509Extension*>(extensions->data[position]);
        if (!extension) throw std::invalid_argument(std::string(__func__) + ": null extension");
        if (LibreSsl3::CompareObjects(extension->object, object) == 0) return position;
    }
    return -1;
}

}
