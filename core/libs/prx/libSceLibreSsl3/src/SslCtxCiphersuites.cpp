#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::array<std::pair<std::string_view, std::uint64_t>, 5> CipherSuites{{
    {"TLS_AES_128_GCM_SHA256", 0x03001301u},
    {"TLS_AES_256_GCM_SHA384", 0x03001302u},
    {"TLS_CHACHA20_POLY1305_SHA256", 0x03001303u},
    {"TLS_AES_128_CCM_SHA256", 0x03001304u},
    {"TLS_AES_128_CCM_8_SHA256", 0x03001305u}
}};

std::uint64_t CipherSuiteId(std::string_view name) {
    for (const auto& [candidate, id] : CipherSuites)
        if (candidate == name) return id;
    return 0;
}

void* CipherIdPointer(std::uint64_t id) {
    return reinterpret_cast<void*>(static_cast<std::uintptr_t>(id));
}

std::string ReadGuestText(const char* text, const char* caller) {
    const int savedErrno = errno;
    std::string copied;
    int error = CopyGuestString_nid_no_patch(text, 65536, copied);
    if (error == 63) error = CopyGuestString_nid_no_patch(text, 1u << 20, copied);
    errno = savedErrno;
    if (error != 0) NotImplemented_nid_no_patch(caller);
    return copied;
}

}

extern "C" int APS5_VABI SSL_CTX_set_ciphersuites_nid_postfix(LibreSsl3SslCtx* context, const char* ciphersuites) {
    if (!context || !ciphersuites) return 0;
    const std::string guestText = ReadGuestText(ciphersuites, __func__);
    auto* const stack = sk_new_nid_postfix(nullptr);
    if (!stack) return 0;
    std::string_view remaining(guestText);
    std::size_t recognised = 0;
    for (;;) {
        const std::size_t separator = remaining.find(':');
        const std::uint64_t id = CipherSuiteId(remaining.substr(0, separator));
        if (id) {
            if (!sk_push_nid_postfix(stack, CipherIdPointer(id))) {
                sk_free_nid_postfix(stack);
                return 0;
            }
            ++recognised;
        }
        if (separator == std::string_view::npos) break;
        remaining.remove_prefix(separator + 1);
    }
    if (recognised == 0) {
        sk_free_nid_postfix(stack);
        return 0;
    }
    sk_free_nid_postfix(static_cast<LibreSslStack*>(context->cipherListTls13));
    context->cipherListTls13 = stack;
    return 1;
}
