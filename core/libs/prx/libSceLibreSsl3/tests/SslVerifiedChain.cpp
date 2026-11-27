#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libSceLibreSsl3/include/Ssl.hpp"
#include "prx/libSceLibreSsl3/include/X509.hpp"

#include <cerrno>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "LibreSSL3 SSL_get0_verified_chain check failed at line %u\n", location.line());
    std::abort();
}

void TestNullArguments() {
    Require(SSL_get0_verified_chain_nid_postfix(nullptr) == nullptr);

    LibreSsl3Ssl ssl{};
    Require(SSL_get0_verified_chain_nid_postfix(&ssl) == nullptr);

    LibreSsl3Ssl3State state{};
    ssl.s3 = &state;
    Require(SSL_get0_verified_chain_nid_postfix(&ssl) == nullptr);
}

void TestVerifiedChainReturned() {
    LibreSsl3Ssl ssl{};
    LibreSsl3Ssl3State state{};
    LibreSsl3Stack stack{};
    LibreSsl3X509 cert{};
    cert.references = 4;
    char* certData[1] = {reinterpret_cast<char*>(&cert)};
    stack.num = 1;
    stack.data = certData;

    state.hs.verifiedChain = &stack;
    ssl.s3 = &state;

    const LibreSsl3Stack* result = SSL_get0_verified_chain_nid_postfix(&ssl);
    Require(result == &stack);
    Require(result->num == 1);
    Require(result->data[0] == reinterpret_cast<char*>(&cert));
    Require(cert.references == 4);

    state.hs.verifiedChain = nullptr;
    Require(SSL_get0_verified_chain_nid_postfix(&ssl) == nullptr);
}

template <typename TCall>
void RejectsPreservingErrno(TCall call, std::source_location location = std::source_location::current()) {
    errno = 71;
    bool rejected = false;
    try {
        call();
    } catch (const std::exception&) {
        rejected = errno == 71;
    }
    Require(rejected, location);
}

void TestGuestPointers() {
    const auto* unmappedSsl = reinterpret_cast<const LibreSsl3Ssl*>(std::uintptr_t{0x10});
    RejectsPreservingErrno([&] { SSL_get0_verified_chain_nid_postfix(unmappedSsl); });

    LibreSsl3Ssl ssl{};
    ssl.s3 = reinterpret_cast<LibreSsl3Ssl3State*>(std::uintptr_t{0x10});
    RejectsPreservingErrno([&] { SSL_get0_verified_chain_nid_postfix(&ssl); });
}

}

int main() {
    TestNullArguments();
    TestVerifiedChainReturned();
    TestGuestPointers();

    std::puts("libSceLibreSsl3 SSL_get0_verified_chain tests passed");
    return 0;
}
