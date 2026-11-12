#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_CTX_set_quic_method check failed at line %u\n", location.line());
    std::abort();
}

template <typename Function>
void RequireThrows(Function function, std::source_location location = std::source_location::current()) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

}

int main() {
    LibreSsl3SslCtx context{};
    context.method = TLS_method_nid_postfix();
    LibreSsl3SslMethod sentinel{};
    Require(SSL_CTX_set_quic_method_nid_postfix(&context, &sentinel) == 1);
    Require(context.quicMethod == &sentinel);
    Require(context.method == TLS_method_nid_postfix());

    Require(SSL_CTX_set_quic_method_nid_postfix(&context, nullptr) == 1);
    Require(context.quicMethod == nullptr);

    LibreSsl3SslMethod dtls{1, 0, 0, 0, 0};
    context.method = &dtls;
    Require(SSL_CTX_set_quic_method_nid_postfix(&context, &sentinel) == 0);
    Require(context.quicMethod == nullptr);

    RequireThrows([] { SSL_CTX_set_quic_method_nid_postfix(nullptr, nullptr); });
    LibreSsl3SslCtx withoutMethod{};
    RequireThrows([&] { SSL_CTX_set_quic_method_nid_postfix(&withoutMethod, &sentinel); });
    std::puts("libSceLibreSsl3 SSL_CTX_set_quic_method tests passed");
}
