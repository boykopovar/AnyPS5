#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/ResourceRequestAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct SameSiteInfo {
    bool isSameSite{false};
    bool isTopSite{false};
    bool isSafeHTTPMethod{false};
};

static_assert(sizeof(SameSiteInfo) == 3);
static_assert(alignof(SameSiteInfo) == 1);
static_assert(offsetof(SameSiteInfo, isSameSite) == 0);
static_assert(offsetof(SameSiteInfo, isTopSite) == 1);
static_assert(offsetof(SameSiteInfo, isSafeHTTPMethod) == 2);
static_assert(std::is_trivially_copyable_v<SameSiteInfo>);

struct OptionalUInt16 {
    bool hasValue{false};
    std::uint16_t value{0};
};

static_assert(sizeof(OptionalUInt16) == 4);
static_assert(alignof(OptionalUInt16) == 2);
static_assert(offsetof(OptionalUInt16, hasValue) == 0);
static_assert(offsetof(OptionalUInt16, value) == 2);

struct SecurityOriginData {
    WtfAbi::String protocol;
    WtfAbi::String host;
    OptionalUInt16 port;
};

static_assert(sizeof(SecurityOriginData) == 24);
static_assert(alignof(SecurityOriginData) == 8);
static_assert(offsetof(SecurityOriginData, protocol) == 0);
static_assert(offsetof(SecurityOriginData, host) == 8);
static_assert(offsetof(SecurityOriginData, port) == 16);
static_assert(!std::is_trivially_destructible_v<SecurityOriginData>);

struct OptionalSecurityOriginData {
    bool hasValue{false};
    union {
        unsigned char dummy;
        SecurityOriginData value;
    };

    OptionalSecurityOriginData() : dummy(0) {}
    explicit OptionalSecurityOriginData(SecurityOriginData&& data) : hasValue(true), value(std::move(data)) {}
    OptionalSecurityOriginData(OptionalSecurityOriginData&& other) noexcept : hasValue(other.hasValue), dummy(0) {
        if (hasValue) new (&value) SecurityOriginData(std::move(other.value));
    }
    OptionalSecurityOriginData(const OptionalSecurityOriginData&) = delete;
    OptionalSecurityOriginData& operator=(const OptionalSecurityOriginData&) = delete;
    ~OptionalSecurityOriginData() {
        if (hasValue) value.~SecurityOriginData();
    }
};

static_assert(sizeof(OptionalSecurityOriginData) == 32);
static_assert(alignof(OptionalSecurityOriginData) == 8);
static_assert(offsetof(OptionalSecurityOriginData, hasValue) == 0);
static_assert(offsetof(OptionalSecurityOriginData, value) == 8);
static_assert(!std::is_trivially_destructible_v<OptionalSecurityOriginData>);

struct ProxyServer {
    enum Type : int { Direct, HTTP, HTTPS, SOCKS };

    Type type{Direct};
    WtfAbi::String hostName;
    int port{-1};
};

static_assert(sizeof(ProxyServer::Type) == 4);
static_assert(sizeof(ProxyServer) == 24);
static_assert(alignof(ProxyServer) == 8);
static_assert(offsetof(ProxyServer, type) == 0);
static_assert(offsetof(ProxyServer, hostName) == 8);
static_assert(offsetof(ProxyServer, port) == 16);

static_assert(sizeof(WtfAbi::Vector<ProxyServer>) == 16);
static_assert(alignof(WtfAbi::Vector<ProxyServer>) == 8);
static_assert(offsetof(WtfAbi::Vector<ProxyServer>, buffer) == 0);
static_assert(offsetof(WtfAbi::Vector<ProxyServer>, capacity) == 8);
static_assert(offsetof(WtfAbi::Vector<ProxyServer>, size) == 12);
static_assert(!std::is_trivially_destructible_v<WtfAbi::Vector<ProxyServer>>);

struct SleepDisablerClient;

static_assert(sizeof(std::unique_ptr<SleepDisablerClient>) == sizeof(void*));
static_assert(alignof(std::unique_ptr<SleepDisablerClient>) == alignof(void*));

}

extern "C" {

NKWebKit::WebCoreAbi::SameSiteInfo APS5_VABI _ZN7WebCore12SameSiteInfo6createERKNS_15ResourceRequestE(const NKWebKit::WebCoreAbi::ResourceRequestBase* request);
NKWebKit::WebCoreAbi::OptionalSecurityOriginData APS5_VABI _ZN7WebCore18SecurityOriginData22fromDatabaseIdentifierERKN3WTF6StringE(const NKWebKit::WtfAbi::String* databaseIdentifier);
NKWebKit::WtfAbi::String APS5_VABI _ZN7WebCore8toStringERKN3WTF6VectorINS_11ProxyServerELm0ENS0_15CrashOnOverflowELm16ENS0_10FastMallocEEE(const NKWebKit::WtfAbi::Vector<NKWebKit::WebCoreAbi::ProxyServer>* proxyServers);
NKWebKit::WtfAbi::Vector<NKWebKit::WebCoreAbi::ProxyServer> APS5_VABI _ZN7WebCore18proxyServersForURLERKN3WTF3URLE(const NKWebKit::WtfAbi::URL* url);
std::unique_ptr<NKWebKit::WebCoreAbi::SleepDisablerClient>& APS5_VABI _ZN7WebCore19sleepDisablerClientEv();

}
