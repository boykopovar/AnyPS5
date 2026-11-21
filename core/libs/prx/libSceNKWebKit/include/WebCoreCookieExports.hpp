#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct OptionalDouble {
    double value;
    bool engaged;
};

struct Cookie {
    NKWebKit::WtfAbi::String name;
    NKWebKit::WtfAbi::String value;
    NKWebKit::WtfAbi::String domain;
    NKWebKit::WtfAbi::String path;
    double created;
    OptionalDouble expires;
    bool httpOnly;
    bool secure;
    bool session;
    NKWebKit::WtfAbi::String comment;
    std::uint8_t commentURL[40];
    std::uint8_t ports[16];
    int sameSite;
};

static_assert(sizeof(OptionalDouble) == 16);
static_assert(sizeof(Cookie) == 136);
static_assert(offsetof(Cookie, domain) == 16);
static_assert(offsetof(Cookie, path) == 24);
static_assert(offsetof(Cookie, created) == 32);
static_assert(offsetof(Cookie, expires) == 40);
static_assert(offsetof(Cookie, httpOnly) == 56);
static_assert(offsetof(Cookie, secure) == 57);
static_assert(offsetof(Cookie, session) == 58);
static_assert(offsetof(Cookie, comment) == 64);
static_assert(offsetof(Cookie, commentURL) == 72);
static_assert(offsetof(Cookie, ports) == 112);
static_assert(offsetof(Cookie, sameSite) == 128);

}  // namespace NKWebKit::WebCoreAbi

extern "C" {

unsigned APS5_VABI _ZNK7WebCore6Cookie4hashEv(const NKWebKit::WebCoreAbi::Cookie* self);
bool APS5_VABI _ZNK7WebCore6CookieeqERKS0_(const NKWebKit::WebCoreAbi::Cookie* self, const NKWebKit::WebCoreAbi::Cookie* other);

}
