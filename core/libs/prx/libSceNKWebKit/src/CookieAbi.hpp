#pragma once

#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit::WebCoreAbi {

struct OptionalDouble {
    double value{0};
    bool has{false};
};

struct Cookie {
    WtfAbi::String name;
    WtfAbi::String value;
    WtfAbi::String domain;
    WtfAbi::String path;
    double created{0};
    OptionalDouble expires;
    bool httpOnly{false};
    bool secure{false};
    bool session{false};
    WtfAbi::String comment;
    WtfAbi::URL commentURL{};
    WtfAbi::Vector<std::uint16_t> ports;
    std::uint8_t sameSite{0};
};

using CookieVector = WtfAbi::Vector<Cookie>;

static_assert(sizeof(OptionalDouble) == 16);
static_assert(alignof(OptionalDouble) == 8);
static_assert(offsetof(OptionalDouble, value) == 0);
static_assert(offsetof(OptionalDouble, has) == 8);

static_assert(sizeof(Cookie) == 136);
static_assert(alignof(Cookie) == 8);
static_assert(offsetof(Cookie, name) == 0);
static_assert(offsetof(Cookie, value) == 8);
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
static_assert(!std::is_trivially_destructible_v<Cookie>);

static_assert(sizeof(CookieVector) == 16);
static_assert(offsetof(CookieVector, buffer) == 0);
static_assert(offsetof(CookieVector, capacity) == 8);
static_assert(offsetof(CookieVector, size) == 12);
static_assert(!std::is_trivially_destructible_v<CookieVector>);

}  // namespace NKWebKit::WebCoreAbi
