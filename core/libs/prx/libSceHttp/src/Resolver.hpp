#ifndef CORE_LIBS_PRX_LIBSCEHTTP_SRC_RESOLVER_HPP
#define CORE_LIBS_PRX_LIBSCEHTTP_SRC_RESOLVER_HPP

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <netdb.h>
#endif

#include <cstdint>

int ResolveHttpHost(const char* host, const char* service, const addrinfo* hints, std::uint32_t timeoutUsec,
    addrinfo** addresses);

namespace GuestHttpTest {
void SetResolverDelayForTests(std::uint32_t delayUsec) noexcept;
}

#endif
