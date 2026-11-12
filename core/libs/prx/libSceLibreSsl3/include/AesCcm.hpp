#pragma once

#include "SceTypes.hpp"

struct LibreSslEvpCipher;

extern "C" {

const LibreSslEvpCipher* APS5_VABI EVP_aes_128_ccm_nid_postfix();

}
