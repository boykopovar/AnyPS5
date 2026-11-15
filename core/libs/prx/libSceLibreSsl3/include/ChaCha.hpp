#pragma once

#include "SceTypes.hpp"

struct LibreSslEvpCipher;

extern "C" {

const LibreSslEvpCipher* APS5_VABI EVP_chacha20_nid_postfix();
const LibreSslEvpCipher* APS5_VABI EVP_chacha20_poly1305_nid_postfix();

}
