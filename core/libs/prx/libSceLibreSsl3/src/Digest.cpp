#include "prx/libSceLibreSsl3/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <stdexcept>
#include "prx/libc/include/General.hpp"

extern "C" {

unsigned char* APS5_VABI MD5_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest) {
    if (!digest) throw std::invalid_argument("MD5: null digest buffer");
    Md5Context context;
    MD5_Init_nid_postfix(&context);
    MD5_Update_nid_postfix(&context, data, length);
    MD5_Final_nid_postfix(digest, &context);

    return digest;
}

unsigned char* APS5_VABI SHA1_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest) {
    if (!digest) throw std::invalid_argument("SHA1: null digest buffer");
    Sha1Context context;
    SHA1_Init_nid_postfix(&context);
    SHA1_Update_nid_postfix(&context, data, length);
    SHA1_Final_nid_postfix(digest, &context);

    return digest;
}

}
