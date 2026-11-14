#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_OCSPRESPONSE_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_OCSPRESPONSE_HPP

#include "prx/libSceLibreSsl/include/Api.hpp"
#include <cstddef>

extern "C" {

void LibreSslWipe_nid_no_patch(void* pointer, std::size_t size);
void LibreSslAsn1StringFree_nid_no_patch(Asn1String* string);
void LibreSslOcspResponseBytesFree_nid_no_patch(LibreSslOcspResponseBytes* bytes);

}

#endif
