#include "prx/libSceNet/include/Net.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>

namespace {
void Require(bool condition, const char* message) {
    if (condition) return;
    std::fprintf(stderr, "guest net dns override: %s\n", message);
    std::abort();
}
}

int main() {
#ifdef _WIN32
    Require(_putenv_s("ANYPS5_DNS_OVERRIDE", "guest-sce-net.dns-override=192.0.2.44") == 0, "putenv failed");
#else
    Require(setenv("ANYPS5_DNS_OVERRIDE", "guest-sce-net.dns-override=192.0.2.44", 1) == 0, "setenv failed");
#endif
    const int resolver = sceNetResolverCreate("dns-override", 0, 0);
    Require(resolver >= 1024 && resolver < 0x4000, "resolver id out of range");
    std::uint32_t address = 0;
    Require(sceNetResolverStartNtoa(resolver, "GUEST-SCE-NET.dns-override", &address, 0, 0, 0) == 0, "override lookup failed");
    Require(address == 0x2c0200c0u, "override address changed");
    Require(sceNetResolverDestroy(resolver) == 0, "resolver destroy failed");
    return 0;
}
