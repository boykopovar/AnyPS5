#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_CIPHERLIST_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_CIPHERLIST_HPP

#include <cstdint>
#include "prx/libSceLibreSsl/include/Api.hpp"

extern "C" {

int LibreSslCipherCount_nid_no_patch();
const void* LibreSslCipherAt_nid_no_patch(unsigned int index);
const void* LibreSslCipherById_nid_no_patch(std::uint64_t id);
LibreSslStack* LibreSslCreateCipherList_nid_no_patch(LibreSslStack** cipherList, LibreSslStack** cipherListById, const char* rules);

}

#endif
