#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"

#include <atomic>
#include <cerrno>
#include <cstddef>
#include <cstdint>

static_assert(offsetof(LibreSslCtx, extraCerts) == 40);

namespace {

void APS5_VABI ReleaseCertificate(void* pointer) {
    if (!pointer) return;
    auto* const certificate = static_cast<LibreSsl3X509*>(pointer);
    std::int32_t currentRefs = 0;
    const int savedErrno = errno;
    const bool valid = GuestCopyIn_nid_no_patch(&currentRefs, &certificate->references, sizeof(currentRefs)) &&
                       GuestWritable_nid_no_patch(&certificate->references, sizeof(certificate->references));
    errno = savedErrno;
    if (!valid) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    std::atomic_ref<std::int32_t> count(certificate->references);
    std::int32_t expected = count.load(std::memory_order_relaxed);
    while (expected > 0) {
        if (count.compare_exchange_weak(expected, expected - 1, std::memory_order_acq_rel, std::memory_order_relaxed)) {
            if (expected == 1) {
                FreeCertificate_nid_no_patch(certificate);
            }
            break;
        }
    }
}

}

extern "C" {

int APS5_VABI SSL_CTX_add0_chain_cert_nid_postfix(LibreSslCtx* context, LibreSsl3X509* certificate) {
    if (!context || !certificate) return 0;
    LibreSslCtx guestCtx{};
    LibreSsl3X509 guestCert{};
    int savedErrno = errno;
    const bool valid = GuestCopyIn_nid_no_patch(&guestCtx, context, sizeof(guestCtx)) &&
                       GuestWritable_nid_no_patch(&context->extraCerts, sizeof(context->extraCerts)) &&
                       GuestCopyIn_nid_no_patch(&guestCert, certificate, sizeof(guestCert));
    errno = savedErrno;
    if (!valid) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);

    auto* stack = static_cast<LibreSslStack*>(guestCtx.extraCerts);
    const bool createdStack = (stack == nullptr);
    if (!stack) {
        stack = sk_new_nid_postfix(nullptr);
        if (!stack) return 0;
        savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(&context->extraCerts, &stack, sizeof(stack));
        errno = savedErrno;
        if (!written) {
            sk_free_nid_postfix(stack);
            NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
        }
    }
    bool pushed = false;
    try {
        pushed = (sk_push_nid_postfix(stack, certificate) > 0);
    } catch (...) {
        pushed = false;
    }
    if (pushed) return 1;
    if (createdStack) {
        sk_free_nid_postfix(stack);
        void* nullExtra = nullptr;
        savedErrno = errno;
        GuestCopyOut_nid_no_patch(&context->extraCerts, &nullExtra, sizeof(nullExtra));
        errno = savedErrno;
    }
    return 0;
}

int APS5_VABI SSL_CTX_clear_chain_certs_nid_postfix(LibreSslCtx* context) {
    if (!context) return 0;
    LibreSslCtx guestCtx{};
    int savedErrno = errno;
    const bool valid = GuestCopyIn_nid_no_patch(&guestCtx, context, sizeof(guestCtx)) &&
                       GuestWritable_nid_no_patch(&context->extraCerts, sizeof(context->extraCerts));
    errno = savedErrno;
    if (!valid) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);

    auto* const stack = static_cast<LibreSslStack*>(guestCtx.extraCerts);
    if (stack) {
        LibreSslStack stackCopy{};
        savedErrno = errno;
        const bool validStack = GuestCopyIn_nid_no_patch(&stackCopy, stack, sizeof(stackCopy));
        errno = savedErrno;
        if (!validStack) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);

        if (stackCopy.num < 0 || stackCopy.numAlloc < 0 || stackCopy.num > stackCopy.numAlloc || (stackCopy.num > 0 && !stackCopy.data)) {
            return 0;
        }
        void* nullExtra = nullptr;
        savedErrno = errno;
        const bool written = GuestCopyOut_nid_no_patch(&context->extraCerts, &nullExtra, sizeof(nullExtra));
        errno = savedErrno;
        if (!written) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);

        struct StackGuard {
            LibreSslStack* ptr;
            ~StackGuard() {
                if (ptr) sk_free_nid_postfix(ptr);
            }
        } guard{stack};

        if (stackCopy.data && stackCopy.num > 0) {
            for (int index = 0; index < stackCopy.num; ++index) {
                char* rawCert = nullptr;
                savedErrno = errno;
                const bool validElem = GuestCopyIn_nid_no_patch(&rawCert, &stackCopy.data[index], sizeof(rawCert));
                errno = savedErrno;
                if (!validElem) NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
                ReleaseCertificate(rawCert);
            }
        }
    }
    return 1;
}

}
