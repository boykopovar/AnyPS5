#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libSceNet/include/Net.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include <array>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {

void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "failed: %s\n", message);
    std::abort();
}

int Receive(LibreSsl::Bio* bio, char* output) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    for (;;) {
        const int result = bio->method->read(bio, output, 1);
        if (result >= 0) return result;
        Require((bio->flags & 0xf) == 0x9, "a nonblocking read advertises retry-read");
        Require(std::chrono::steady_clock::now() < deadline, "loopback datagram arrives before the deadline");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

} // namespace

int main() {
    std::array<void*, 10> heap{};
    ApplicationHeapRegister_nid_no_patch(heap.data());
    const int descriptor = sceNetSocket("bio-retry", 2, 2, 0);
    Require(descriptor >= 0, "create UDP socket");
    std::array<unsigned char, 16> address{16, 2, 0, 0, 127, 0, 0, 1};
    Require(sceNetBind_nid_postfix(descriptor, address.data(), address.size()) == 0, "bind loopback");
    std::uint32_t addressLength = address.size();
    Require(sceNetGetsockname(descriptor, address.data(), &addressLength) == 0, "read bound port");
    Require(sceNetConnect(descriptor, address.data(), addressLength) == 0, "connect loopback socket to itself");
    const int enabled = 1;
    Require(sceNetSetsockopt(descriptor, 0xffff, 0x1200, &enabled, sizeof(enabled)) == 0, "enable nonblocking I/O");
    auto* bio = BIO_new_socket_nid_postfix(descriptor, 0);
    Require(bio != nullptr, "create socket BIO");
    char output = 0;
    bio->flags = 0x400a;
    Require(bio->method->read(bio, &output, 1) == -1 && *sceNetErrnoLoc() == 35, "empty socket returns guest EAGAIN");
    Require(bio->flags == 0x4009, "read retry replaces write retry and preserves unrelated flags");
    Require(bio->method->write(bio, "x", 1) == 1 && bio->flags == 0x4000, "successful write clears read retry");
    bio->flags |= 0xa;
    Require(Receive(bio, &output) == 1 && output == 'x' && bio->flags == 0x4000, "successful read clears write retry");
    Require(bio->method->write(bio, "", 0) == 0, "send empty datagram");
    *sceNetErrnoLoc() = 35;
    bio->flags |= 0xa;
    Require(Receive(bio, &output) == 0 && bio->flags == 0x4000, "zero-byte receive ignores stale EAGAIN");
    Require(sceNetSocketClose(descriptor) == 0, "close underlying descriptor");
    bio->flags |= 0x9;
    Require(bio->method->write(bio, "x", 1) == -1 && bio->flags == 0x4000, "terminal write failure clears retry");
    bio->flags |= 0xa;
    Require(bio->method->read(bio, &output, 1) == -1 && bio->flags == 0x4000, "terminal read failure clears retry");
    Require(BIO_free_nid_postfix(bio) == 1, "free borrowed socket BIO");
}
