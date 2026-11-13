#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace {

void Require(bool condition, int line = __builtin_LINE()) {
    if (condition) return;
    std::fprintf(stderr, "SSL_set_quic_transport_params check failed at line %d\n", line);
    std::abort();
}

bool failAllocation;

void* APS5_VABI Allocate(std::size_t bytes) {
    if (failAllocation) {
        failAllocation = false;
        return nullptr;
    }
    return std::malloc(bytes ? bytes : 1);
}

void APS5_VABI Release(void* block) {
    std::free(block);
}

void* APS5_VABI AllocateZeroed(std::size_t count, std::size_t bytes) {
    return std::calloc(count ? count : 1, bytes ? bytes : 1);
}

void* APS5_VABI Reallocate(void*, std::size_t) { return nullptr; }
void* APS5_VABI Align(std::size_t, std::size_t) { return nullptr; }
void* APS5_VABI Realign(void*, std::size_t, std::size_t) { return nullptr; }
int APS5_VABI PosixAlign(void**, std::size_t, std::size_t) { return 12; }

void InitializeHeap() {
    void* const api[10] {
        reinterpret_cast<void*>(&Allocate), reinterpret_cast<void*>(&Release), reinterpret_cast<void*>(&AllocateZeroed),
        reinterpret_cast<void*>(&Reallocate), reinterpret_cast<void*>(&Align), reinterpret_cast<void*>(&Realign),
        reinterpret_cast<void*>(&PosixAlign),
    };
    ApplicationHeapRegister_nid_no_patch(api);
}

}

int main() {
    InitializeHeap();
    LibreSsl3Ssl ssl{};
    const std::array<std::uint8_t, 3> first{0x01, 0x02, 0x03};
    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, first.data(), first.size()) == 1);
    Require(ssl.quicTransportParams != nullptr && ssl.quicTransportParamsLength == 3);
    Require(std::memcmp(ssl.quicTransportParams, first.data(), first.size()) == 0);

    const std::array<std::uint8_t, 1> second{0xff};
    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, second.data(), second.size()) == 1);
    Require(ssl.quicTransportParamsLength == 1 && ssl.quicTransportParams[0] == 0xff);

    failAllocation = true;
    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, first.data(), first.size()) == 0);
    Require(ssl.quicTransportParams == nullptr && ssl.quicTransportParamsLength == 0);

    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, second.data(), second.size()) == 1);
    Require(ssl.quicTransportParamsLength == 1 && ssl.quicTransportParams[0] == 0xff);

    Require(SSL_set_quic_transport_params_nid_postfix(&ssl, nullptr, 0) == 1);
    Require(ssl.quicTransportParams != nullptr && ssl.quicTransportParamsLength == 0);
    free_nid_postfix(ssl.quicTransportParams);

    bool threw = false;
    try { SSL_set_quic_transport_params_nid_postfix(nullptr, first.data(), first.size()); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);
    threw = false;
    try { SSL_set_quic_transport_params_nid_postfix(&ssl, nullptr, 1); } catch (const std::invalid_argument&) { threw = true; }
    Require(threw);

    return 0;
}
