#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"

namespace {

struct BioGuard {
    LibreSsl::Bio* bio;
    ~BioGuard() {
        BIO_free_nid_postfix(bio);
    }
};

}  // namespace

extern "C" LibreSslX509* PEM_read_X509_impl_nid_no_patch(void* stream, LibreSslX509** certificate,
                                                        LibreSslPemPasswordCallback passwordCallback,
                                                        void* userData) {
    auto* bio = BIO_new_nid_postfix(BIO_s_file_nid_postfix());
    if (!bio) {
        LibreSslPutError_nid_no_patch(9, 0xfff, 7);
        return nullptr;
    }
    BioGuard guard{bio};
    bio->shutdown = 0;
    bio->data = stream;
    bio->initialized = 1;
    return PEM_read_bio_X509_nid_postfix(bio, certificate, passwordCallback, userData);
}
