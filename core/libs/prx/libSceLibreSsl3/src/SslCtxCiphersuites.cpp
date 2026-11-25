#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl/include/Error.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr int SslLibrary = 20;
constexpr int AnyFunction = 0xfff;
constexpr int NoCipherMatch = 185;
constexpr std::uint64_t Unsupported = 0;

constexpr std::array<std::pair<std::string_view, std::uint64_t>, 10> CipherSuites{{
    {"TLS_AES_128_GCM_SHA256", 0x03001301u},
    {"AEAD-AES128-GCM-SHA256", 0x03001301u},
    {"TLS_AES_256_GCM_SHA384", 0x03001302u},
    {"AEAD-AES256-GCM-SHA384", 0x03001302u},
    {"TLS_CHACHA20_POLY1305_SHA256", 0x03001303u},
    {"AEAD-CHACHA20-POLY1305-SHA256", 0x03001303u},
    {"TLS_AES_128_CCM_SHA256", Unsupported},
    {"AEAD-AES128-CCM-SHA256", Unsupported},
    {"TLS_AES_128_CCM_8_SHA256", Unsupported},
    {"AEAD-AES128-CCM-8-SHA256", Unsupported}
}};

const std::uint64_t* FindCipherSuite(std::string_view name) {
    for (const auto& suite : CipherSuites)
        if (suite.first == name) return &suite.second;
    return nullptr;
}

int NoMatch(LibreSslStack* stack) {
    sk_free_nid_postfix(stack);
    LibreSslPutError_nid_no_patch(SslLibrary, AnyFunction, NoCipherMatch, __FILE__, __LINE__);
    return 0;
}

void* CipherIdPointer(std::uint64_t id) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));
}

std::string ReadGuestText(const char* text, const char* caller) {
    std::string copied;
    int error = CopyGuestString_nid_no_patch(text, 65536, copied);
    if (error == 63) error = CopyGuestString_nid_no_patch(text, 1u << 20, copied);
    if (error != 0) NotImplemented_nid_no_patch(caller);
    return copied;
}

LibreSsl3SslCtx ValidateWritableContext(LibreSsl3SslCtx* context, const char* caller) {
    LibreSsl3SslCtx copy{};
    const bool valid = GuestCopyIn_nid_no_patch(&copy, context, sizeof(copy)) &&
        GuestWritable_nid_no_patch(&context->cipherListTls13, sizeof(context->cipherListTls13));
    if (!valid) NotImplemented_nid_no_patch(caller);
    return copy;
}

void WriteCipherList(LibreSsl3SslCtx* context, LibreSslStack* stack, const char* caller) {
    const bool written = GuestCopyOut_nid_no_patch(&context->cipherListTls13, &stack, sizeof(stack));
    if (!written) {
        sk_free_nid_postfix(stack);
        NotImplemented_nid_no_patch(caller);
    }
}

}

extern "C" int APS5_VABI SSL_CTX_set_ciphersuites_nid_postfix(LibreSsl3SslCtx* context, const char* ciphersuites) {
    if (!context || !ciphersuites) return 0;
    const LibreSsl3SslCtx guestContext = ValidateWritableContext(context, __func__);
    const std::string guestText = ReadGuestText(ciphersuites, __func__);
    auto* const stack = sk_new_nid_postfix(nullptr);
    if (!stack) return NoMatch(nullptr);
    if (!guestText.empty()) {
        std::string_view remaining(guestText);
        for (;;) {
            const std::size_t separator = remaining.find(':');
            const std::uint64_t* const id = FindCipherSuite(remaining.substr(0, separator));
            if (!id) return NoMatch(stack);
            if (*id != Unsupported && !sk_push_nid_postfix(stack, CipherIdPointer(*id))) return NoMatch(stack);
            if (separator == std::string_view::npos) break;
            remaining.remove_prefix(separator + 1);
        }
    }
    WriteCipherList(context, stack, __func__);
    sk_free_nid_postfix(static_cast<LibreSslStack*>(guestContext.cipherListTls13));
    return 1;
}
