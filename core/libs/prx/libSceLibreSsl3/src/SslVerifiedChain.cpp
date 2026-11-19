#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Ssl.hpp"

extern "C" {

const LibreSsl3Stack* APS5_VABI SSL_get0_verified_chain_nid_postfix(const LibreSsl3Ssl* ssl) {
    if (!ssl || !ssl->s3) return nullptr;
    return ssl->s3->hs.verifiedChain;
}

}
