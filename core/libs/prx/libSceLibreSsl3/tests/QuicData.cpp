#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/General.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <string>

namespace {

std::size_t checks = 0;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    ++checks;
    if (condition) return;
    std::fprintf(stderr, "QUIC data check failed at line %u\n", location.line());
    std::abort();
}

constexpr int EncryptionInitial = 0;
constexpr int EncryptionHandshake = 2;
constexpr int EncryptionOneRtt = 3;

char QuicMethod;

LibreSsl3Ssl MakeSsl(const void* quicMethod = &QuicMethod) {
    LibreSsl3Ssl ssl{};
    ssl.quicMethod = quicMethod;
    return ssl;
}

LibreSsl3QuicPendingData* Pending(LibreSsl3Ssl& ssl) {
    return static_cast<LibreSsl3QuicPendingData*>(ssl.tls13);
}

std::string Queued(const LibreSsl3Ssl& ssl) {
    const auto* pending = static_cast<const LibreSsl3QuicPendingData*>(ssl.tls13);
    if (!pending || !pending->data) return std::string();
    return std::string(reinterpret_cast<const char*>(pending->data), static_cast<std::size_t>(pending->length));
}

void Release(LibreSsl3Ssl& ssl) {
    auto* pending = Pending(ssl);
    if (!pending) return;
    free_nid_postfix(pending->data);
    free_nid_postfix(pending);
    ssl.tls13 = nullptr;
}

void TransportParams() {
    std::array<std::uint8_t, 4> params{{1, 2, 3, 4}};
    LibreSsl3Ssl ssl = MakeSsl();
    ssl.quicTransportParams = params.data();
    ssl.quicTransportParamsLength = params.size();

    const std::uint8_t* received = nullptr;
    std::size_t length = 99;
    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &received, &length) == 1);
    Require(received == params.data());
    Require(length == params.size());

    ssl.quicTransportParams = nullptr;
    ssl.quicTransportParamsLength = 0;
    received = params.data();
    length = 99;
    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &received, &length) == 1);
    Require(received == nullptr);
    Require(length == 0);

    ssl.quicTransportParams = nullptr;
    ssl.quicTransportParamsLength = 12345;
    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &received, &length) == 1);
    Require(received == nullptr);
    Require(length == 12345);

    ssl.quicTransportParams = reinterpret_cast<std::uint8_t*>(0x10);
    ssl.quicTransportParamsLength = 4;
    received = nullptr;
    length = 0;
    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &received, &length) == 1);
    Require(received == reinterpret_cast<const std::uint8_t*>(0x10));
    Require(length == 4);

    ssl.quicTransportParams = nullptr;
    ssl.quicTransportParamsLength = 0;
    received = params.data();
    length = 7;
    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, nullptr, &length) == 1);
    Require(received == params.data());
    Require(length == 0);
    length = 7;
    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &received, nullptr) == 1);
    Require(received == nullptr);
    Require(length == 7);

    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, nullptr, nullptr) == 1);

    received = params.data();
    length = 7;
    Require(SSL_get_peer_quic_transport_params_nid_postfix(nullptr, &received, &length) == 0);
    Require(received == params.data());
    Require(length == 7);

    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, reinterpret_cast<const std::uint8_t**>(0x10),
                                                           &length) == 0);
    Require(SSL_get_peer_quic_transport_params_nid_postfix(&ssl, &received,
                                                           reinterpret_cast<std::size_t*>(0x10)) == 0);
}

void Provide() {
    LibreSsl3Ssl ssl = MakeSsl();
    std::string expected;
    auto provide = [&](int level, const std::string& bytes) {
        Require(SSL_provide_quic_data_nid_postfix(&ssl, level, reinterpret_cast<const std::uint8_t*>(bytes.data()),
                                                  bytes.size()) == 1);
        expected += bytes;
        Require(Queued(ssl) == expected);
    };

    provide(EncryptionInitial, "abc");
    provide(EncryptionHandshake, "de");
    provide(EncryptionOneRtt, std::string(500, 'x'));
    provide(EncryptionInitial, std::string());
    Require(Pending(ssl)->length == expected.size());

    std::string source = "hi";
    Require(SSL_provide_quic_data_nid_postfix(&ssl, EncryptionInitial,
                                              reinterpret_cast<const std::uint8_t*>(source.data()),
                                              source.size()) == 1);
    expected += "hi";
    source.assign(source.size(), '?');
    Require(Queued(ssl) == expected);

    Release(ssl);
    Require(ssl.tls13 == nullptr);
}

void Rejections() {
    const std::array<std::uint8_t, 2> bytes{{1, 2}};
    const auto* data = bytes.data();

    Require(SSL_provide_quic_data_nid_postfix(nullptr, EncryptionInitial, data, bytes.size()) == 0);

    LibreSsl3Ssl methodless = MakeSsl(nullptr);
    Require(SSL_provide_quic_data_nid_postfix(&methodless, EncryptionInitial, data, bytes.size()) == 0);
    Require(methodless.tls13 == nullptr);

    LibreSsl3Ssl ssl = MakeSsl();
    for (const int level : {-1, EncryptionOneRtt + 1, 99}) {
        Require(SSL_provide_quic_data_nid_postfix(&ssl, level, data, bytes.size()) == 0);
        Require(ssl.tls13 == nullptr);
    }
    for (const int level : {EncryptionInitial, 1, EncryptionHandshake, EncryptionOneRtt}) {
        Require(SSL_provide_quic_data_nid_postfix(&ssl, level, data, bytes.size()) == 1);
    }
    Require(Queued(ssl) == "\x01\x02\x01\x02\x01\x02\x01\x02");

    Require(SSL_provide_quic_data_nid_postfix(&ssl, EncryptionInitial, nullptr, 1) == 0);
    Require(SSL_provide_quic_data_nid_postfix(&ssl, EncryptionInitial,
                                              reinterpret_cast<const std::uint8_t*>(0x10), 4) == 0);
    Require(Queued(ssl) == "\x01\x02\x01\x02\x01\x02\x01\x02");

    Require(SSL_provide_quic_data_nid_postfix(&ssl, EncryptionHandshake, nullptr, 0) == 1);
    Require(Queued(ssl) == "\x01\x02\x01\x02\x01\x02\x01\x02");

    void* const pending = ssl.tls13;
    Require(SSL_provide_quic_data_nid_postfix(&ssl, EncryptionOneRtt + 1, data, bytes.size()) == 0);
    Require(SSL_provide_quic_data_nid_postfix(&ssl, EncryptionInitial, nullptr, bytes.size()) == 0);
    Require(ssl.tls13 == pending);
    Require(Queued(ssl) == "\x01\x02\x01\x02\x01\x02\x01\x02");

    Release(ssl);
}

void IndependentConnections() {
    LibreSsl3Ssl first = MakeSsl();
    LibreSsl3Ssl second = MakeSsl();
    const std::string a = "one";
    const std::string b = "two";
    Require(SSL_provide_quic_data_nid_postfix(&first, EncryptionInitial,
                                              reinterpret_cast<const std::uint8_t*>(a.data()), a.size()) == 1);
    Require(SSL_provide_quic_data_nid_postfix(&second, EncryptionInitial,
                                              reinterpret_cast<const std::uint8_t*>(b.data()), b.size()) == 1);
    Require(Queued(first) == "one");
    Require(Queued(second) == "two");
    Require(first.tls13 != second.tls13);
    Release(first);
    Release(second);
}

}

void* APS5_VABI Allocate_nid_no_patch(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free_nid_no_patch(void* pointer) { std::free(pointer); }
void* APS5_VABI Callocate_nid_no_patch(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate_nid_no_patch(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI UnexpectedAlign_nid_no_patch(std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned allocation"); }
void* APS5_VABI UnexpectedRealign_nid_no_patch(void*, std::size_t, std::size_t) { throw std::runtime_error("unexpected aligned reallocation"); }
int APS5_VABI UnexpectedPosixAlign_nid_no_patch(void**, std::size_t, std::size_t) { throw std::runtime_error("unexpected posix_memalign"); }

int main() {
    std::array<void*, 10> heapApi{};
    heapApi[0] = reinterpret_cast<void*>(Allocate_nid_no_patch);
    heapApi[1] = reinterpret_cast<void*>(Free_nid_no_patch);
    heapApi[2] = reinterpret_cast<void*>(Callocate_nid_no_patch);
    heapApi[3] = reinterpret_cast<void*>(Reallocate_nid_no_patch);
    heapApi[4] = reinterpret_cast<void*>(UnexpectedAlign_nid_no_patch);
    heapApi[5] = reinterpret_cast<void*>(UnexpectedRealign_nid_no_patch);
    heapApi[6] = reinterpret_cast<void*>(UnexpectedPosixAlign_nid_no_patch);
    ApplicationHeapRegister_nid_no_patch(heapApi.data());
    TransportParams();
    Provide();
    Rejections();
    IndependentConnections();
    std::printf("QUIC data: %zu checks\n", checks);
}
