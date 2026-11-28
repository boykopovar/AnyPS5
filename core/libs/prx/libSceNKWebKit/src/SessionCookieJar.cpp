#include "prx/libSceNKWebKit/src/SessionCookieJar.hpp"

#include "prx/libSceNKWebKit/include/WtfExports.hpp"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace NKWebKit::WebCoreAbi {

namespace {

using WtfAbi::StringView;

std::size_t UnitSize(const StringView& view) {
    return view.is8Bit ? 1 : 2;
}

char16_t Unit(const StringView& view, unsigned index) {
    return view.is8Bit
        ? static_cast<char16_t>(static_cast<const unsigned char*>(view.data)[index])
        : static_cast<const char16_t*>(view.data)[index];
}

StringView Slice(const StringView& view, unsigned start) {
    const std::size_t size = UnitSize(view);
    return StringView{static_cast<const unsigned char*>(view.data) + start * size, view.length - start, view.is8Bit};
}

char16_t ToASCIILower(char16_t value) {
    return value >= u'A' && value <= u'Z' ? static_cast<char16_t>(value - u'A' + u'a') : value;
}

bool EqualIgnoringAsciiCase(const StringView& a, const StringView& b) {
    if (a.length != b.length) return false;
    for (unsigned i = 0; i < a.length; ++i) {
        if (ToASCIILower(Unit(a, i)) != ToASCIILower(Unit(b, i))) return false;
    }
    return true;
}

bool KeyEqual(const Cookie& a, const Cookie& b) {
    const auto aPath = View(a.path);
    const auto bPath = View(b.path);
    return _ZN3WTF5equalEPKNS_10StringImplES2_(a.name.impl, b.name.impl)
        && EqualIgnoringAsciiCase(View(a.domain), View(b.domain))
        && ((!aPath.length && !bPath.length) || _ZN3WTF22equalRespectingNullityENS_10StringViewES0_(aPath, bPath));
}

bool SecureRequest(const WtfAbi::URL& url) {
    const StringView protocol = _ZNK3WTF3URL8protocolEv(&url);
    static constexpr StringView secureProtocols[] = {{"https", 5, true}, {"wss", 3, true}};
    for (const auto& secure : secureProtocols) {
        if (EqualIgnoringAsciiCase(protocol, secure)) return true;
    }
    return false;
}

double NowMilliseconds() {
    using namespace std::chrono;
    return static_cast<double>(duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count());
}

}  // namespace

StringView View(const WtfAbi::String& string) {
    return string.impl ? WtfAbi::ViewOf(string.impl) : StringView{nullptr, 0, true};
}

bool HostMatchesDomain(const StringView& domain, const StringView& host) {
    if (!domain.length || !host.length) return false;
    if (Unit(domain, 0) != '.') return EqualIgnoringAsciiCase(domain, host);
    if (domain.length == 1) return false;
    const auto suffix = Slice(domain, 1);
    if (EqualIgnoringAsciiCase(suffix, host)) return true;
    if (host.length <= domain.length) return false;
    const auto start = host.length - suffix.length;
    return Unit(host, start - 1) == '.' && EqualIgnoringAsciiCase(Slice(host, start), suffix);
}

bool PathMatches(const StringView& cookiePath, const StringView& requestPath) {
    if (!cookiePath.length) return true;
    if (cookiePath.length > requestPath.length) return false;
    for (unsigned i = 0; i < cookiePath.length; ++i) {
        if (Unit(cookiePath, i) != Unit(requestPath, i)) return false;
    }
    if (Unit(cookiePath, cookiePath.length - 1) == '/') return true;
    if (cookiePath.length == requestPath.length) return true;
    return Unit(requestPath, cookiePath.length) == '/';
}

bool IsExpired(const Cookie& cookie, double now) {
    return cookie.expires.has && now > cookie.expires.value;
}

SessionCookieJar& SessionCookies(const void* session) {
    if (!session) throw std::invalid_argument("NetworkStorageSession requires a session");
    static std::mutex mutex;
    static std::unordered_map<const void*, SessionCookieJar> jars;
    std::lock_guard lock(mutex);
    return jars.try_emplace(session).first->second;
}

void SessionCookieJar::addCookie(const Cookie& cookie, const WtfAbi::URL& url) {
    const StringView name = View(cookie.name);
    if (!name.length) return;
    if (!HostMatchesDomain(View(cookie.domain), _ZNK3WTF3URL4hostEv(&url))) return;
    const bool secureOrigin = SecureRequest(url);
    if (cookie.secure && !secureOrigin) return;
    const double now = NowMilliseconds();
    std::lock_guard<std::mutex> lock(m_mutex);
    const auto existing = std::find_if(m_cookies.begin(), m_cookies.end(),
        [&](const Cookie& stored) { return KeyEqual(stored, cookie); });
    if (existing != m_cookies.end() && existing->secure && !secureOrigin) return;
    if (IsExpired(cookie, now)) {
        if (existing != m_cookies.end()) m_cookies.erase(existing);
        return;
    }
    auto stored = cookie;
    stored.commentURL = {};
    if (existing != m_cookies.end()) {
        *existing = std::move(stored);
    } else {
        m_cookies.push_back(std::move(stored));
    }
}

void SessionCookieJar::deleteCookie(const Cookie& cookie) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::erase_if(m_cookies, [&](const Cookie& stored) { return KeyEqual(stored, cookie); });
}

void SessionCookieJar::deleteAllCookies() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_cookies.clear();
}

CookieVector SessionCookieJar::cookiesForURL(const WtfAbi::URL& url) const {
    const StringView host = _ZNK3WTF3URL4hostEv(&url);
    CookieVector cookies;
    if (!host.length) return cookies;
    StringView requestPath = _ZNK3WTF3URL4pathEv(&url);
    if (!requestPath.length) requestPath = StringView{"/", 1, true};
    const bool secure = SecureRequest(url);
    const double now = NowMilliseconds();
    std::lock_guard<std::mutex> lock(m_mutex);
    for (const auto& cookie : m_cookies) {
        if (IsExpired(cookie, now)) continue;
        if (cookie.secure && !secure) continue;
        if (!HostMatchesDomain(View(cookie.domain), host)) continue;
        if (!PathMatches(View(cookie.path), requestPath)) continue;
        cookies.append(cookie);
    }
    return cookies;
}

CookieVector SessionCookieJar::allCookies() const {
    const double now = NowMilliseconds();
    std::lock_guard<std::mutex> lock(m_mutex);
    CookieVector cookies;
    for (const auto& cookie : m_cookies) {
        if (IsExpired(cookie, now)) continue;
        cookies.append(cookie);
    }
    return cookies;
}

}  // namespace NKWebKit::WebCoreAbi
