#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>

namespace WebCore {

struct ProxyServer {
    enum Type : int { Direct, HTTP, HTTPS, SOCKS };

    Type m_type{Direct};
    NKWebKit::WtfAbi::String m_hostName;
    int m_port{-1};
};

static_assert(offsetof(ProxyServer, m_type) == 0);
static_assert(offsetof(ProxyServer, m_hostName) == 8);
static_assert(offsetof(ProxyServer, m_port) == 16);
static_assert(sizeof(ProxyServer) == 24);
static_assert(sizeof(NKWebKit::WtfAbi::Vector<ProxyServer>) == 16);

}  // namespace WebCore

extern "C" {

NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const NKWebKit::WtfAbi::Vector<WebCore::ProxyServer>* proxyServers);

}
