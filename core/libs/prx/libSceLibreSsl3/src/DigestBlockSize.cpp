#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" int APS5_VABI EVP_MD_block_size_nid_postfix(const EvpMd* md) {
    if (!md) throw std::invalid_argument("EVP_MD_block_size requires a digest");
    return md->blockSize;
}
