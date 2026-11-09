#include "prx/libSceLibreSsl3/include/Api.hpp"

extern "C" int APS5_VABI SSL_is_quic_nid_postfix(const LibreSsl3Ssl* ssl) {
    return ssl && ssl->quicMethod ? 1 : 0;
}
