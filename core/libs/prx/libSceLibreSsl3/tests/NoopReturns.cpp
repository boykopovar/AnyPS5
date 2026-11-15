#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "prx/libSceLibreSsl3/include/Api.hpp"

namespace {

void Require(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "LibreSsl3 noop returns: %s\n", message);
        std::abort();
    }
}

}

int main() {
    unsigned char context[0x100];
    unsigned char ssl[0x100];
    std::memset(context, 0xa5, sizeof(context));
    std::memset(ssl, 0x5a, sizeof(ssl));
    unsigned char untouchedContext[sizeof(context)];
    unsigned char untouchedSsl[sizeof(ssl)];
    std::memcpy(untouchedContext, context, sizeof(context));
    std::memcpy(untouchedSsl, ssl, sizeof(ssl));

    Require(SSL_CTX_set_max_early_data_nid_postfix(context, 0x4000) == 1, "SSL_CTX_set_max_early_data must return 1");
    Require(SSL_CTX_set_max_early_data_nid_postfix(nullptr, 0) == 1, "SSL_CTX_set_max_early_data must return 1 for a null context");
    SSL_CTX_set_post_handshake_auth_nid_postfix(context, 1);
    Require(SSL_process_quic_post_handshake_nid_postfix(ssl) == 1, "SSL_process_quic_post_handshake must return 1");
    SSL_set_quic_use_legacy_codepoint_nid_postfix(ssl, 1);

    Require(std::memcmp(context, untouchedContext, sizeof(context)) == 0, "a call wrote the SSL_CTX");
    Require(std::memcmp(ssl, untouchedSsl, sizeof(ssl)) == 0, "a call wrote the SSL");
    return 0;
}
