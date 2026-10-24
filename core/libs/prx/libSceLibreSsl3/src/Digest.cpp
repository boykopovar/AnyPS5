#include <cstddef>
#include "prx/libc/include/General.hpp"

extern "C" {
int APS5_VABI MD5_Init_nid_postfix(void* context);
int APS5_VABI MD5_Update_nid_postfix(void* context, const void* data, std::size_t length);
int APS5_VABI MD5_Final_nid_postfix(unsigned char* digest, void* context);
int APS5_VABI SHA1_Init_nid_postfix(void* context);
int APS5_VABI SHA1_Update_nid_postfix(void* context, const void* data, std::size_t length);
int APS5_VABI SHA1_Final_nid_postfix(unsigned char* digest, void* context);

unsigned char* APS5_VABI MD5_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest) {
    if (!digest) NotImplemented_nid_no_patch("MD5: null digest buffer");
    alignas(8) unsigned char context[92];
    MD5_Init_nid_postfix(context);
    MD5_Update_nid_postfix(context, data, length);
    MD5_Final_nid_postfix(digest, context);
    return digest;
}

unsigned char* APS5_VABI SHA1_nid_postfix(const unsigned char* data, std::size_t length, unsigned char* digest) {
    if (!digest) NotImplemented_nid_no_patch("SHA1: null digest buffer");
    alignas(8) unsigned char context[96];
    SHA1_Init_nid_postfix(context);
    SHA1_Update_nid_postfix(context, data, length);
    SHA1_Final_nid_postfix(digest, context);
    return digest;
}

}
