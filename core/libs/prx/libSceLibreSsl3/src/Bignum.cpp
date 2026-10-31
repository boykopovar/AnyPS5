#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" {

int APS5_VABI BN_is_negative_nid_postfix(const LibreSsl3Bignum* number) {
    if (!number)
        throw std::invalid_argument("BN_is_negative requires a number");
    return number->neg != 0;
}

}
