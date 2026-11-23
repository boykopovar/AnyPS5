#include "prx/libc/include/Api.hpp"

#include <string>

extern "C" {

int APS5_VABI _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE4syncEv_nid_postfix(void*) {
    return 0;
}

void* APS5_VABI _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE6setbufEPcl_nid_postfix(void* stream, char*, std::int64_t) {
    return stream;
}

int APS5_VABI _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE8overflowEi_nid_postfix(void*, int) {
    return std::char_traits<char>::eof();
}

int APS5_VABI _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9pbackfailEi_nid_postfix(void*, int) {
    return std::char_traits<char>::eof();
}

std::int64_t APS5_VABI _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9showmanycEv_nid_postfix(void*) {
    return 0;
}

int APS5_VABI _ZNSt3__115basic_streambufIcNS_11char_traitsIcEEE9underflowEv_nid_postfix(void*) {
    return std::char_traits<char>::eof();
}

}
