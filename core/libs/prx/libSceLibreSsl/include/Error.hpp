#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_ERROR_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_ERROR_HPP

#include <cstdint>

extern "C" {

void LibreSslPutError_nid_no_patch(int library, int function, int reason, const char* file = nullptr, int line = 0);
std::uint64_t LibreSslPopError_nid_no_patch(const char** file, int* line);

}

#endif
