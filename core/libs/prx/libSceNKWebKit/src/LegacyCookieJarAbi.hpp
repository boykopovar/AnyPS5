#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::WebCoreAbi {

struct LegacySameSiteInfo {
    bool isSameSite;
    bool isTopSite;
    bool isSafeHTTPMethod;
};

struct LegacyOptionalIdentifier {
    bool engaged;
    std::uint64_t value;
};

struct LegacyCookieRequestHeaderFieldValue {
    WtfAbi::String value;
    bool secureCookiesAccessed;
};

struct LegacyVaryingRequestHeader {
    WtfAbi::String name;
    WtfAbi::String value;
};

struct LegacyCookieJar;

struct LegacyCookieJarVtable {
    void* cookies;
    void* setCookies;
    void* cookiesEnabled;
    LegacyCookieRequestHeaderFieldValue (APS5_VABI *cookieRequestHeaderFieldValue)(
        const LegacyCookieJar*, const WtfAbi::URL* firstParty, const LegacySameSiteInfo*, const WtfAbi::URL*, const LegacyOptionalIdentifier* frame,
        const LegacyOptionalIdentifier* page, bool includeSecureCookies);
    void* getRawCookies;
    void* setRawCookie;
    void* deleteCookie;
    void* clearCache;
    void* clearCacheForHost;
    void* completeDestructor;
    void* deletingDestructor;
};

struct LegacyCookieJar {
    const LegacyCookieJarVtable* vptr;
    unsigned refCount;
    void* storageSessionProvider;
};

static_assert(sizeof(LegacySameSiteInfo) == 3);
static_assert(sizeof(LegacyOptionalIdentifier) == 16);
static_assert(offsetof(LegacyOptionalIdentifier, value) == 8);
static_assert(sizeof(LegacyCookieRequestHeaderFieldValue) == 16);
static_assert(offsetof(LegacyCookieRequestHeaderFieldValue, secureCookiesAccessed) == 8);
static_assert(!std::is_trivially_copyable_v<LegacyCookieRequestHeaderFieldValue>);
static_assert(sizeof(LegacyVaryingRequestHeader) == 16);
static_assert(offsetof(LegacyVaryingRequestHeader, value) == 8);
static_assert(sizeof(LegacyCookieJarVtable) == 88);
static_assert(offsetof(LegacyCookieJarVtable, cookieRequestHeaderFieldValue) == 24);
static_assert(offsetof(LegacyCookieJarVtable, completeDestructor) == 72);
static_assert(sizeof(LegacyCookieJar) == 24);
static_assert(offsetof(LegacyCookieJar, vptr) == 0);
static_assert(offsetof(LegacyCookieJar, refCount) == 8);
static_assert(offsetof(LegacyCookieJar, storageSessionProvider) == 16);

}  // namespace NKWebKit::WebCoreAbi
