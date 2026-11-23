#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::ProxyAbi {

inline constexpr std::int32_t ProxyTypeDirect = 0;
inline constexpr std::int32_t ProxyTypeHTTP = 1;
inline constexpr std::int32_t ProxyTypeHTTPS = 2;
inline constexpr std::int32_t ProxyTypeSOCKS = 3;

struct ProxyServer {
    std::int32_t type;
    WtfAbi::StringImpl* hostName;
    std::int32_t port;
};

struct ProxyServerVector {
    ProxyServer* buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(sizeof(ProxyServer) == 24);
static_assert(offsetof(ProxyServer, type) == 0);
static_assert(offsetof(ProxyServer, hostName) == 8);
static_assert(offsetof(ProxyServer, port) == 16);
static_assert(sizeof(ProxyServerVector) == 16);
static_assert(offsetof(ProxyServerVector, capacity) == 8);
static_assert(offsetof(ProxyServerVector, size) == 12);

}

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const NKWebKit::ProxyAbi::ProxyServerVector* proxyServers);
NKWebKit::ProxyAbi::ProxyServerVector* APS5_VABI _ZN7WebCore18proxyServersForURLERKN3WTF3URLE(NKWebKit::ProxyAbi::ProxyServerVector* result, const NKWebKit::WtfAbi::URL* url);

}
