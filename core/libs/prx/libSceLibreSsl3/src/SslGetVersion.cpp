#include "prx/libSceLibreSsl3/include/Api.hpp"
#include <stdexcept>

extern "C" const char* APS5_VABI SSL_get_version_nid_postfix(const LibreSsl3Ssl* ssl) {
    if (!ssl) throw std::invalid_argument("SSL_get_version: null SSL");
    switch (ssl->version) {
    case 0x0300: return "SSLv3";
    case 0x0301: return "TLSv1";
    case 0x0302: return "TLSv1.1";
    case 0x0303: return "TLSv1.2";
    case 0x0304: return "TLSv1.3";
    case 0xfeff: return "DTLSv1";
    case 0xfefd: return "DTLSv1.2";
    default: return "unknown";
    }
}
