// Derived from LibreSSL 4.0.0 src/lib/libcrypto/bio/bio_lib.c: https://github.com/libressl/openbsd/blob/ce488b78e0bd994780569df718596b9776ff681a/src/lib/libcrypto/bio/bio_lib.c

#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"

extern "C" {

void APS5_VABI BIO_vfree_nid_postfix(LibreSsl::Bio* bio) {
    static_cast<void>(BIO_free_nid_postfix(bio));
}

}
