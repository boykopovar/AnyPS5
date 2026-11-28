#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

struct ContextPointers {
    LibreSslX509* issuerCertificate;
    LibreSslX509* subjectCertificate;
    void* subjectRequest;
    void* crl;
};

static_assert(offsetof(LibreSsl3X509V3Context, issuerCertificate) == 8);
static_assert(offsetof(LibreSsl3X509V3Context, subjectCertificate) == 16);
static_assert(offsetof(LibreSsl3X509V3Context, subjectRequest) == 24);
static_assert(offsetof(LibreSsl3X509V3Context, crl) == 32);
static_assert(offsetof(LibreSsl3X509V3Context, database) == 40);
static_assert(sizeof(ContextPointers) == 32);

class PreservedErrno {
public:
    PreservedErrno() : saved(errno) {}
    ~PreservedErrno() { errno = saved; }

private:
    int saved;
};

void EnsureWritablePreservingErrno(void* guest, std::size_t size, const char* caller) {
    PreservedErrno guard;
    if (!GuestWritable_nid_no_patch(guest, size))
        NotImplemented_nid_no_patch(caller);
}

template <typename T>
void CopyOutPreservingErrno(void* guest, const T& value, const char* caller) {
    PreservedErrno guard;
    if (!GuestCopyOut_nid_no_patch(guest, &value, sizeof(value)))
        NotImplemented_nid_no_patch(caller);
}

}

extern "C" void APS5_VABI X509V3_set_ctx_nid_postfix(LibreSsl3X509V3Context* context, LibreSslX509* issuer, LibreSslX509* subject,
                                                   void* request, void* crl, int flags) {
    if (!context) throw std::invalid_argument("X509V3_set_ctx requires a context");
    EnsureWritablePreservingErrno(context, offsetof(LibreSsl3X509V3Context, database), __func__);
    auto* const guest = reinterpret_cast<unsigned char*>(context);
    const std::int32_t contextFlags = flags;
    const ContextPointers pointers{issuer, subject, request, crl};
    CopyOutPreservingErrno(guest + offsetof(LibreSsl3X509V3Context, flags), contextFlags, __func__);
    CopyOutPreservingErrno(guest + offsetof(LibreSsl3X509V3Context, issuerCertificate), pointers, __func__);
}
