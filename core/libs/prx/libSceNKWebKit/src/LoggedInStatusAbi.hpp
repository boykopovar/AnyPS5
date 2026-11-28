#pragma once

#include "prx/libSceNKWebKit/src/MediaTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfTimeTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct RegistrableDomain {
    WtfAbi::String string;
};

struct LoggedInStatus {
    RegistrableDomain domain;
    WtfAbi::String username;
    bool tokenType;
    std::uint8_t authType;
    WTF::WallTime loggedInTime;
    WTF::Seconds timeToLive;
};

struct LoggedInStatusOrException {
    union {
        LoggedInStatus* value;
        WebCore::Exception exception;
    };
    bool hasValue;

    explicit LoggedInStatusOrException(LoggedInStatus* status) : value(status), hasValue(true) {}
    LoggedInStatusOrException(int code, WtfAbi::String message) : exception{code, static_cast<WtfAbi::String&&>(message)}, hasValue(false) {}
    LoggedInStatusOrException(const LoggedInStatusOrException&) = delete;
    LoggedInStatusOrException& operator=(const LoggedInStatusOrException&) = delete;
    ~LoggedInStatusOrException();
};

static_assert(sizeof(RegistrableDomain) == 8);
static_assert(sizeof(LoggedInStatus) == 40);
static_assert(offsetof(LoggedInStatus, username) == 0x08);
static_assert(offsetof(LoggedInStatus, tokenType) == 0x10);
static_assert(offsetof(LoggedInStatus, authType) == 0x11);
static_assert(offsetof(LoggedInStatus, loggedInTime) == 0x18);
static_assert(offsetof(LoggedInStatus, timeToLive) == 0x20);
static_assert(sizeof(LoggedInStatusOrException) == 24);
static_assert(alignof(LoggedInStatusOrException) == 8);
static_assert(offsetof(LoggedInStatusOrException, hasValue) == 0x10);
static_assert(!std::is_trivially_destructible_v<LoggedInStatusOrException>);

}  // namespace NKWebKit::WebCoreAbi
