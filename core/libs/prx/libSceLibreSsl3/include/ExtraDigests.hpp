#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"

namespace LibreSsl3 {

const EvpMd* Sha512_224Digest();
const EvpMd* Ripemd160Digest();
const EvpMd* Md4Digest();
const EvpMd* Sm3Digest();
const EvpMd* Md5Sha1Digest();
const EvpMd* Sha3_224Digest();
const EvpMd* Sha3_256Digest();
const EvpMd* Sha3_384Digest();
const EvpMd* Sha3_512Digest();

} // namespace LibreSsl3
