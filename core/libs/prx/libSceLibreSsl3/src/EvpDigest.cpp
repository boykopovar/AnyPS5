#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/PkeyContext.hpp"
#include "prx/libSceLibreSsl3/include/PkeyMethod.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>


namespace {

using LibreSsl3::DigestContext;
using LibreSsl3::DigestMethod;

constexpr std::uint64_t Cleaned = 2;
constexpr std::uint64_t Reuse = 4;
constexpr std::uint64_t KeepKeyContext = 0x400;

void* RequireData(DigestContext* context, const char* function) {
    if (!context || !context->data) NotImplemented_nid_no_patch(function);
    return context->data;
}

int APS5_VABI Sha1Init(DigestContext* context) {
    return SHA1_Init_nid_postfix(static_cast<Sha1Context*>(RequireData(context, __func__)));
}

int APS5_VABI Sha1Update(DigestContext* context, const void* data, std::size_t length) {
    if (context && !length) return 1;
    return SHA1_Update_nid_postfix(static_cast<Sha1Context*>(RequireData(context, __func__)), data, length);
}

int APS5_VABI Sha1Final(DigestContext* context, unsigned char* digest) {
    return SHA1_Final_nid_postfix(digest, static_cast<Sha1Context*>(RequireData(context, __func__)));
}

int APS5_VABI Sha224Init(DigestContext* context) {
    return SHA224_Init_nid_postfix(static_cast<Sha256Context*>(RequireData(context, __func__)));
}

int APS5_VABI Sha224Update(DigestContext* context, const void* data, std::size_t length) {
    if (context && !length) return 1;
    return SHA256_Update_nid_postfix(static_cast<Sha256Context*>(RequireData(context, __func__)), data, length);
}

int APS5_VABI Sha224Final(DigestContext* context, unsigned char* digest) {
    return SHA224_Final_nid_postfix(digest, static_cast<Sha256Context*>(RequireData(context, __func__)));
}

constexpr DigestMethod Sha1{64, 65, 20, 8, Sha1Init, Sha1Update, Sha1Final, nullptr, nullptr, nullptr, nullptr, {6, 19, 0, 0, 0}, 64, 104, nullptr};
constexpr DigestMethod Sha224{675, 671, 28, 8, Sha224Init, Sha224Update, Sha224Final, nullptr, nullptr, nullptr, nullptr, {6, 19, 0, 0, 0}, 64, 120, nullptr};

void Clear(void* memory, std::size_t size) {
    auto* bytes = static_cast<volatile unsigned char*>(memory);
    for (std::size_t index = 0; index < size; ++index) bytes[index] = 0;
}

template <typename T>
T CopyIn(const T* source, const char* function) {
    T value{};
    const int savedErrno = errno;
    const bool readable = GuestCopyIn_nid_no_patch(&value, source, sizeof(T));
    errno = savedErrno;
    if (!readable) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

template <typename T>
void CopyOut(T* destination, const T& value, const char* function) {
    const int savedErrno = errno;
    const bool writable = GuestCopyOut_nid_no_patch(destination, &value, sizeof(T));
    errno = savedErrno;
    if (!writable) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

void RequireWritable(void* destination, std::size_t size, const char* function) {
    if (!GuestWritable_nid_no_patch(destination, size)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

} // namespace

extern "C" {

const DigestMethod* APS5_VABI EVP_sha1_nid_postfix() {
    return &Sha1;
}

const DigestMethod* APS5_VABI EVP_sha224_nid_postfix() {
    return &Sha224;
}

DigestContext* APS5_VABI EVP_MD_CTX_new_nid_postfix() {
    return static_cast<DigestContext*>(calloc_nid_postfix(1, sizeof(DigestContext)));
}

void APS5_VABI EVP_MD_CTX_free_nid_postfix(DigestContext* context) {
    if (!context) return;
    const DigestContext initial = CopyIn(context, __func__);
    if (initial.engine) NotImplemented_nid_no_patch(__func__);
    RequireWritable(context, sizeof(DigestContext), __func__);
    if (initial.digest && !(initial.flags & Cleaned)) {
        const DigestMethod digest = CopyIn(initial.digest, __func__);
        if (digest.cleanup) digest.cleanup(context);
    }
    const DigestContext current = CopyIn(context, __func__);
    if (current.engine) NotImplemented_nid_no_patch(__func__);
    const DigestMethod digest = current.digest ? CopyIn(current.digest, __func__) : DigestMethod{};
    if (current.digest && digest.contextSize && current.data && !(current.flags & Reuse)) {
        if (digest.contextSize < 0) NotImplemented_nid_no_patch(__func__);
        RequireWritable(current.data, static_cast<std::size_t>(digest.contextSize), __func__);
        Clear(current.data, static_cast<std::size_t>(digest.contextSize));
        free_nid_postfix(current.data);
    }
    if (current.keyContext && !(current.flags & KeepKeyContext)) {
        const auto pctx = CopyIn(static_cast<const LibreSsl3PkeyContext*>(current.keyContext), __func__);
        const auto* method = static_cast<const LibreSsl3PkeyMethod*>(pctx.method);
        if (method && FindNativePkeyMethod_nid_no_patch(method->keyType) == method) {
            EVP_PKEY_CTX_free_nid_postfix(static_cast<LibreSsl3PkeyContext*>(current.keyContext));
        } else {
            NotImplemented_nid_no_patch(__func__);
        }
    }
    CopyOut(context, DigestContext{}, __func__);
    free_nid_postfix(context);
}

int APS5_VABI EVP_DigestInit_nid_postfix(DigestContext* context, const DigestMethod* digest) {
    if (!context || !digest) NotImplemented_nid_no_patch(__func__);
    const DigestMethod method = CopyIn(digest, __func__);
    if (method.contextSize < 0 || !method.init) NotImplemented_nid_no_patch(__func__);
    RequireWritable(context, sizeof(DigestContext), __func__);
    DigestContext initialized{};
    initialized.digest = digest;
    if (method.contextSize) {
        initialized.update = method.update;
        initialized.data = calloc_nid_postfix(1, static_cast<std::size_t>(method.contextSize));
        if (!initialized.data) {
            CopyOut(context, initialized, __func__);
            NotImplemented_nid_no_patch(__func__);
        }
    }
    CopyOut(context, initialized, __func__);
    return method.init(context);
}

}
