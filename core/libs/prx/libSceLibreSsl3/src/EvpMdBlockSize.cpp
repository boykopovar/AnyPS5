#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

extern "C" int APS5_VABI EVP_MD_block_size_nid_postfix(const LibreSsl3::DigestMethod* digest) {
    return digest ? digest->blockSize : 0;
}
