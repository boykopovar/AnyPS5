#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/libc.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>


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
    if (context->engine) NotImplemented_nid_no_patch(__func__);
    if (context->digest && context->digest->cleanup && !(context->flags & Cleaned)) context->digest->cleanup(context);
    if (context->engine) NotImplemented_nid_no_patch(__func__);
    if (context->keyContext && !(context->flags & KeepKeyContext)) NotImplemented_nid_no_patch(__func__);
    if (context->digest && context->digest->contextSize && context->data && !(context->flags & Reuse)) {
        if (context->digest->contextSize < 0) NotImplemented_nid_no_patch(__func__);
        Clear(context->data, static_cast<std::size_t>(context->digest->contextSize));
        free_nid_postfix(context->data);
    }
    Clear(context, sizeof(*context));
    free_nid_postfix(context);
}

int APS5_VABI EVP_DigestInit_nid_postfix(DigestContext* context, const DigestMethod* digest) {
    if (!context || !digest || digest->contextSize < 0 || !digest->init) NotImplemented_nid_no_patch(__func__);
    std::memset(context, 0, sizeof(*context));
    context->digest = digest;
    if (digest->contextSize) {
        context->update = digest->update;
        context->data = calloc_nid_postfix(1, static_cast<std::size_t>(digest->contextSize));
        if (!context->data) NotImplemented_nid_no_patch(__func__);
    }
    return digest->init(context);
}

}
