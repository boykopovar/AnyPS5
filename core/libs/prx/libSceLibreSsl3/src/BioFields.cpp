#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" {

void* APS5_VABI BIO_get_data_nid_postfix(LibreSsl::Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO_get_data requires a BIO");
    return bio->data;
}

}
