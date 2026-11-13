#include "prx/libSceSystemService/libSceSystemService.hpp"

#include <cstdlib>

int main() {
    for (int iter = 0; iter < 2; ++iter) {
        libSceSystemServiceUnknown_kDGjRMtUDxo_nid_no_patch();
        libSceSystemServiceUnknown_qdTPJWjquDc_nid_no_patch();
        if (libSceSystemServiceUnknown_OyTwhCKWXi4_nid_no_patch() != static_cast<int>(0x80F50013u)) std::abort();
        if (libSceSystemServiceUnknown_G3oPdw3lkps_nid_no_patch() != static_cast<int>(0x80F50013u)) std::abort();
    }
    libSceSystemServiceUnknown_qdTPJWjquDc_nid_no_patch();
    if (libSceSystemServiceUnknown_OyTwhCKWXi4_nid_no_patch() != static_cast<int>(0x80F50013u)) std::abort();
    if (libSceSystemServiceUnknown_G3oPdw3lkps_nid_no_patch() != static_cast<int>(0x80F50013u)) std::abort();
}
