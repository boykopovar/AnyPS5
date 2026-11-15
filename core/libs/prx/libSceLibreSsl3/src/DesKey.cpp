#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

extern "C" void APS5_VABI DES_set_key_unchecked_nid_postfix(const unsigned char* key, unsigned char* schedule) {
    DES_set_key_nid_postfix(key, schedule);
}
