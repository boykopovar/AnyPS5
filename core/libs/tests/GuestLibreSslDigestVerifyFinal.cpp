#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>

namespace {

struct PkeyMethodPrefix {
    std::int32_t pkeyId;
    std::int32_t flags;
    void* beforeVerifyContext[16];
    void* verifyContext;
};

struct PkeyContextPrefix {
    PkeyMethodPrefix* method;
};

static_assert(sizeof(PkeyMethodPrefix) == 144);
static_assert(offsetof(PkeyMethodPrefix, verifyContext) == 136);
static_assert(sizeof(PkeyContextPrefix) == 8);
static_assert(sizeof(EvpMdContext) == 48);
static_assert(offsetof(EvpMdContext, pkeyContext) == 32);

constexpr std::uint64_t EvpInputNotInitialized = (6ull << 24) | (0xfffull << 12) | 111;
constexpr std::uint64_t EvpMessageDigestIsNull = (6ull << 24) | (0xfffull << 12) | 159;
constexpr std::uint64_t EvpExpectingRsaKey = (6ull << 24) | (0xfffull << 12) | 127;

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

template<typename TAction>
void RequireThrows(TAction action, const char* message) {
    try {
        action();
    } catch (const std::exception&) {
        return;
    }
    Require(false, message);
}

void RunUninitialized(bool hasVerifyContext) {
    PkeyMethodPrefix method{};
    method.verifyContext = hasVerifyContext ? reinterpret_cast<void*>(0x7777) : nullptr;
    PkeyContextPrefix pkeyContext{&method};
    EvpMdContext context{};
    context.flags = 0x35;
    context.pkeyContext = &pkeyContext;
    const auto contextBefore = context;
    const auto pkeyContextBefore = pkeyContext;
    const auto methodBefore = method;
    std::array<unsigned char, 19> signature{};
    signature.fill(0xa5);
    const auto signatureBefore = signature;

    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(6, 0xfff, 159, __FILE__, __LINE__);
    LibreSslPutError_nid_no_patch(6, 0xfff, 127, __FILE__, __LINE__);
    Require(EVP_DigestVerifyFinal_nid_postfix(&context, signature.data(), std::numeric_limits<std::size_t>::max()) == -1,
        "an uninitialized digest returns -1 after reading the verification method prefix");
    Require(std::memcmp(&context, &contextBefore, sizeof(context)) == 0 && std::memcmp(&pkeyContext, &pkeyContextBefore, sizeof(pkeyContext)) == 0
        && std::memcmp(&method, &methodBefore, sizeof(method)) == 0, "the context and verification method remain unchanged");
    Require(signature == signatureBefore, "signature bytes are untouched");
    Require(ERR_get_error_nid_postfix() == EvpMessageDigestIsNull && ERR_get_error_nid_postfix() == EvpExpectingRsaKey
        && ERR_get_error_nid_postfix() == EvpInputNotInitialized && ERR_get_error_nid_postfix() == 0,
        "the exact input-not-initialized error is appended behind existing errors");
    Require(EVP_DigestVerifyFinal_nid_postfix(&context, reinterpret_cast<unsigned char*>(std::uintptr_t{1}),
                std::numeric_limits<std::size_t>::max()) == -1, "the copy failure does not read signature memory");
    Require(ERR_get_error_nid_postfix() == EvpInputNotInitialized && ERR_get_error_nid_postfix() == 0,
            "the unread signature path queues only the copy error");
}

void MissingVerificationState() {
    EvpMdContext context{};
    ERR_clear_error_nid_postfix();
    RequireThrows([&] { EVP_DigestVerifyFinal_nid_postfix(&context, nullptr, 0); }, "a missing EVP_PKEY_CTX follows the source's unchecked read");
    Require(ERR_get_error_nid_postfix() == 0, "a missing EVP_PKEY_CTX does not queue the digest error first");

    PkeyContextPrefix pkeyContext{};
    context.pkeyContext = &pkeyContext;
    RequireThrows([&] { EVP_DigestVerifyFinal_nid_postfix(&context, nullptr, 0); }, "a missing EVP_PKEY_METHOD follows the source's unchecked read");
    Require(ERR_get_error_nid_postfix() == 0, "a missing EVP_PKEY_METHOD queues no synthetic digest error");

    pkeyContext.method = reinterpret_cast<PkeyMethodPrefix*>(std::uintptr_t{1});
    RequireThrows([&] { EVP_DigestVerifyFinal_nid_postfix(&context, nullptr, 0); }, "unreadable method state throws before the digest error");
    context.pkeyContext = reinterpret_cast<void*>(std::uintptr_t{1});
    RequireThrows([&] { EVP_DigestVerifyFinal_nid_postfix(&context, nullptr, 0); }, "unreadable key context throws before the digest error");
    RequireThrows([] { EVP_DigestVerifyFinal_nid_postfix(nullptr, nullptr, 0); }, "a null context follows the source's unchecked read");
    Require(ERR_get_error_nid_postfix() == 0, "a null context queues no error");
}

void InitializedDigestIsUnknown() {
    PkeyMethodPrefix method{};
    PkeyContextPrefix pkeyContext{&method};
    EvpMd digest{};
    EvpMdContext context{};
    context.digest = &digest;
    context.pkeyContext = &pkeyContext;
    std::array<unsigned char, 8> signature{};
    signature.fill(0x6d);
    const auto signatureBefore = signature;
    const auto contextBefore = context;

    ERR_clear_error_nid_postfix();
    LibreSslPutError_nid_no_patch(6, 0xfff, 159, __FILE__, __LINE__);
    RequireThrows([&] { EVP_DigestVerifyFinal_nid_postfix(&context, signature.data(), signature.size()); },
        "initialized digest verification requires the unported context-copy and key-verification path");
    Require(ERR_get_error_nid_postfix() == EvpMessageDigestIsNull && ERR_get_error_nid_postfix() == 0,
        "the unknown initialized path preserves existing errors and adds none");
    Require(std::memcmp(&context, &contextBefore, sizeof(context)) == 0 && signature == signatureBefore,
        "the unknown initialized path does not modify its arguments");
}

}

int main() {
    RunUninitialized(false);
    RunUninitialized(true);
    MissingVerificationState();
    InitializedDigestIsUnknown();
    return 0;
}
