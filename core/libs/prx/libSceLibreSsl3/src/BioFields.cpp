#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <stdexcept>

extern "C" {

void* APS5_VABI BIO_get_data_nid_postfix(LibreSsl::Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO_get_data requires a BIO");
    return bio->data;
}

void APS5_VABI BIO_set_data_nid_postfix(LibreSsl::Bio* bio, void* data) {
    if (!bio) throw std::invalid_argument("BIO_set_data requires a BIO");
    bio->data = data;
}

void APS5_VABI BIO_set_init_nid_postfix(LibreSsl::Bio* bio, int init) {
    if (!bio) throw std::invalid_argument("BIO_set_init requires a BIO");
    bio->initialized = init;
}

int APS5_VABI BIO_get_shutdown_nid_postfix(LibreSsl::Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO_get_shutdown requires a BIO");
    return bio->shutdown;
}

void APS5_VABI BIO_set_shutdown_nid_postfix(LibreSsl::Bio* bio, int shutdown) {
    if (!bio) throw std::invalid_argument("BIO_set_shutdown requires a BIO");
    bio->shutdown = shutdown;
}

void APS5_VABI BIO_set_flags_nid_postfix(LibreSsl::Bio* bio, int flags) {
    if (!bio) throw std::invalid_argument("BIO_set_flags requires a BIO");
    bio->flags |= flags;
}

void APS5_VABI BIO_clear_flags_nid_postfix(LibreSsl::Bio* bio, int flags) {
    if (!bio) throw std::invalid_argument("BIO_clear_flags requires a BIO");
    bio->flags &= ~flags;
}

}
