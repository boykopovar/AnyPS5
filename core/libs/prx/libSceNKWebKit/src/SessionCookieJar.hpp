#pragma once

#include "prx/libSceNKWebKit/src/CookieAbi.hpp"

#include <mutex>
#include <vector>

namespace NKWebKit::WebCoreAbi {

class SessionCookieJar {
public:
    void addCookie(const Cookie& cookie, const WtfAbi::URL& url);
    void deleteCookie(const Cookie& cookie);
    void deleteAllCookies();
    CookieVector cookiesForURL(const WtfAbi::URL& url) const;
    CookieVector allCookies() const;

private:
    mutable std::mutex m_mutex;
    std::vector<Cookie> m_cookies;
};

SessionCookieJar& SessionCookies(const void* session);

WtfAbi::StringView View(const WtfAbi::String& string);
bool HostMatchesDomain(const WtfAbi::StringView& domain, const WtfAbi::StringView& host);
bool PathMatches(const WtfAbi::StringView& cookiePath, const WtfAbi::StringView& requestPath);
bool IsExpired(const Cookie& cookie, double now);

}  // namespace NKWebKit::WebCoreAbi
