#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int PemLibrary = 9;
constexpr int AnyFunction = 0xfff;
constexpr int BufferError = 7;
constexpr int MallocFailure = 65;
constexpr std::size_t PemBufferSize = 1024;

void PemError(int reason) {
    LibreSslPutError_nid_no_patch(PemLibrary, AnyFunction, reason, __FILE__, __LINE__);
}

void Wipe(void* pointer, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(pointer);
    while (size--) *bytes++ = 0;
}

int Write(LibreSsl::Bio* bio, const char* bytes, int size) {
    return BIO_write_nid_postfix(bio, bytes, size);
}

int WriteEmptyPem(LibreSsl::Bio* bio) {
    static constexpr char begin[] = "-----BEGIN CERTIFICATE-----\n";
    static constexpr char end[] = "-----END CERTIFICATE-----\n";
    if (Write(bio, begin, 11) != 11 || Write(bio, begin + 11, 11) != 11 || Write(bio, begin + 22, 6) != 6) {
        PemError(BufferError);
        return 0;
    }
    auto* buffer = static_cast<unsigned char*>(reallocarray_nid_postfix(nullptr, PemBufferSize, 8));
    if (!buffer) {
        PemError(MallocFailure);
        return 0;
    }
    Wipe(buffer, PemBufferSize * 8);
    free_nid_postfix(buffer);
    if (Write(bio, end, 9) != 9 || Write(bio, end + 9, 11) != 11 || Write(bio, end + 20, 6) != 6) {
        PemError(BufferError);
        return 0;
    }
    return 0;
}

}

int APS5_VABI PEM_write_bio_X509_nid_postfix(LibreSsl::Bio* bio, LibreSslX509* certificate) {
    if (certificate) NotImplemented_nid_no_patch(__func__);
    auto* data = static_cast<unsigned char*>(malloc_nid_postfix(20));
    if (!data) {
        PemError(MallocFailure);
        return 0;
    }
    int result;
    try {
        result = WriteEmptyPem(bio);
    } catch (...) {
        free_nid_postfix(data);
        throw;
    }
    free_nid_postfix(data);
    return result;
}
