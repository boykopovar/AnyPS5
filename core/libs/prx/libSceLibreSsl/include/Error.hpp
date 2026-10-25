#ifndef CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_ERROR_HPP
#define CORE_LIBS_PRX_LIBSCELIBRESSL_INCLUDE_ERROR_HPP

extern "C" {

void LibreSslPutError_nid_no_patch(int library, int function, int reason);

}

#endif
