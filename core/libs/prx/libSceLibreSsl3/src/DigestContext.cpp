#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstring>

extern "C" {

const void* APS5_VABI EVP_MD_CTX_md_nid_postfix(const void* context) {
    if (!context)
        return nullptr;
    const void* digest;
    std::memcpy(&digest, context, sizeof(digest));
    return digest;
}

}
