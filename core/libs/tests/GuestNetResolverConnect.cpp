#include "prx/libSceNet/include/Net.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

namespace {

void Require(bool value) { if (!value) std::abort(); }

constexpr int kErrorBase = static_cast<int>(0x80410100);
constexpr int kResolverEnosupport = 227;

bool Failed(int result, int error) { return result == (kErrorBase | error) && *sceNetErrnoLoc() == error; }

bool StorageUntouched(const std::uint8_t* storage, std::size_t size) {
    for (std::size_t index = 0; index < size; ++index) {
        if (storage[index] != 0x5a) return false;
    }
    return true;
}

void CheckUnsupported() {
    alignas(16) std::uint8_t storage[64];
    NetResolverConnectOption option{1000, 2000, 3, 0};
    const NetResolverConnectOption before = option;
    for (const std::uint32_t flags : {0u, 1u, 0x80000000u}) {
        std::memset(storage, 0x5a, sizeof(storage));
        auto* connection = reinterpret_cast<NetResolverConnection*>(storage);
        Require(Failed(sceNetResolverConnectCreate("resolver", 1, connection, flags), kResolverEnosupport));
        Require(Failed(sceNetResolverConnectCreate(nullptr, 2, nullptr, flags), kResolverEnosupport));
        Require(Failed(sceNetResolverConnectCreate("", 3, nullptr, flags), kResolverEnosupport));
        Require(StorageUntouched(storage, sizeof(storage)));
        Require(Failed(sceNetResolverConnect(connection, "example.org", 53, &option), kResolverEnosupport));
        Require(Failed(sceNetResolverConnect(nullptr, nullptr, 0, nullptr), kResolverEnosupport));
        Require(Failed(sceNetResolverConnect(connection, "", 65535, &option), kResolverEnosupport));
        Require(Failed(sceNetResolverConnectAbort(connection, flags), kResolverEnosupport));
        Require(Failed(sceNetResolverConnectAbort(nullptr, 0), kResolverEnosupport));
        Require(Failed(sceNetResolverConnectDestroy(connection), kResolverEnosupport));
        Require(Failed(sceNetResolverConnectDestroy(nullptr), kResolverEnosupport));
        Require(StorageUntouched(storage, sizeof(storage)));
        Require(std::memcmp(&option, &before, sizeof(option)) == 0);
    }
}

}

int main() {
    CheckUnsupported();
    Require(sceNetInit_nid_postfix() == 0);
    CheckUnsupported();
    Require(sceNetTerm() == 0);
    CheckUnsupported();
    return 0;
}
