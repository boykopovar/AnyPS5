#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <stdexcept>

namespace {

template <typename T>
T ReadField(const LibreSsl::Bio* bio, std::size_t offset, const char* function) {
    T value{};
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, reinterpret_cast<const unsigned char*>(bio) + offset, sizeof(value));
    errno = saved;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void WriteField(LibreSsl::Bio* bio, std::size_t offset, const T& value, const char* function) {
    const int saved = errno;
    const bool ok = GuestCopyOut_nid_no_patch(reinterpret_cast<unsigned char*>(bio) + offset, &value, sizeof(value));
    errno = saved;
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

}

extern "C" {

void* APS5_VABI BIO_get_data_nid_postfix(LibreSsl::Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO_get_data requires a BIO");
    return ReadField<void*>(bio, offsetof(LibreSsl::Bio, data), __func__);
}

void APS5_VABI BIO_set_data_nid_postfix(LibreSsl::Bio* bio, void* data) {
    if (!bio) throw std::invalid_argument("BIO_set_data requires a BIO");
    WriteField(bio, offsetof(LibreSsl::Bio, data), data, __func__);
}

void APS5_VABI BIO_set_init_nid_postfix(LibreSsl::Bio* bio, int init) {
    if (!bio) throw std::invalid_argument("BIO_set_init requires a BIO");
    WriteField(bio, offsetof(LibreSsl::Bio, initialized), init, __func__);
}

int APS5_VABI BIO_get_shutdown_nid_postfix(LibreSsl::Bio* bio) {
    if (!bio) throw std::invalid_argument("BIO_get_shutdown requires a BIO");
    return ReadField<int>(bio, offsetof(LibreSsl::Bio, shutdown), __func__);
}

void APS5_VABI BIO_set_shutdown_nid_postfix(LibreSsl::Bio* bio, int shutdown) {
    if (!bio) throw std::invalid_argument("BIO_set_shutdown requires a BIO");
    WriteField(bio, offsetof(LibreSsl::Bio, shutdown), shutdown, __func__);
}

void APS5_VABI BIO_set_flags_nid_postfix(LibreSsl::Bio* bio, int flags) {
    if (!bio) throw std::invalid_argument("BIO_set_flags requires a BIO");
    const int current = ReadField<int>(bio, offsetof(LibreSsl::Bio, flags), __func__);
    WriteField(bio, offsetof(LibreSsl::Bio, flags), current | flags, __func__);
}

void APS5_VABI BIO_clear_flags_nid_postfix(LibreSsl::Bio* bio, int flags) {
    if (!bio) throw std::invalid_argument("BIO_clear_flags requires a BIO");
    const int current = ReadField<int>(bio, offsetof(LibreSsl::Bio, flags), __func__);
    WriteField(bio, offsetof(LibreSsl::Bio, flags), current & ~flags, __func__);
}

}
