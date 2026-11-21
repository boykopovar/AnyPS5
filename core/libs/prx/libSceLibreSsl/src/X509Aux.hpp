#pragma once

#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>
#include <cstdint>

namespace LibreSsl {

LibreSslX509* X509AuxRead_nid_no_patch(LibreSslX509** certificate, const unsigned char** input, std::int64_t length);
void X509AuxiliaryFree(void* auxiliary);

}
