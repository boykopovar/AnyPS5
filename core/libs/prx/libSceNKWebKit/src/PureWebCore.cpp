#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebCoreExports.hpp"
#include "prx/libSceNKWebKit/include/WtfExports.hpp"
#include "prx/libSceNKWebKit/src/WebCoreTypes.hpp"
#include "prx/libkernel/include/Api.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

namespace WebCore {

}  // namespace WebCore

namespace {
std::optional<int> g_presentingApplicationPID;
}  // namespace

extern "C" {
bool APS5_VABI _ZN7WebCore30isStatusCodeCacheableByDefaultEi(int statusCode) {
    switch (statusCode) {
    case 200:
    case 203:
    case 204:
    case 206:
    case 300:
    case 301:
    case 404:
    case 405:
    case 410:
    case 414:
    case 501:
        return true;
    default:
        return false;
    }
}

bool APS5_VABI _ZN7WebCore32isStatusCodePotentiallyCacheableEi(int statusCode) {
    switch (statusCode) {
    case 201:
    case 202:
    case 205:
    case 302:
    case 303:
    case 307:
    case 403:
    case 406:
    case 415:
        return true;
    default:
        return false;
    }
}

bool APS5_VABI _ZN7WebCore24redirectChainAllowsReuseENS_24RedirectChainCacheStatusENS_28ReuseExpiredRedirectionOrNotE(
    WebCore::RedirectChainCacheStatus status, int reuseExpiredRedirection) {
    switch (status.status) {
    case WebCore::RedirectChainCacheStatus::Status::NoRedirection:
        return true;
    case WebCore::RedirectChainCacheStatus::Status::NotCachedRedirection:
        return false;
    case WebCore::RedirectChainCacheStatus::Status::CachedRedirection:
        return reuseExpiredRedirection != 0 || _ZN3WTF8WallTime3nowEv().m_value <= status.endOfValidity.m_value;
    }
    return false;
}

unsigned int APS5_VABI _ZN7WebCore43initializeMaximumHTTPConnectionCountPerHostEv() {
    return 4;
}

void APS5_VABI _ZN7WebCore27setPresentingApplicationPIDEi(int pid) {
    g_presentingApplicationPID = pid;
}

int APS5_VABI _ZN7WebCore24presentingApplicationPIDEv() {
    return g_presentingApplicationPID ? *g_presentingApplicationPID : getpid_nid_postfix();
}

WebCore::FloatRect APS5_VABI _ZN7WebCore36largestRectWithAspectRatioInsideRectEfRKNS_9FloatRectE(
    float aspectRatio, const WebCore::FloatRect& srcRect) {
    WebCore::FloatRect destRect = srcRect;
    if (aspectRatio > srcRect.size().aspectRatio()) {
        float dy = destRect.width() / aspectRatio - destRect.height();
        destRect.inflateY(dy / 2.0f);
    } else {
        float dx = destRect.height() * aspectRatio - destRect.width();
        destRect.inflateX(dx / 2.0f);
    }
    return destRect;
}

WebCore::FloatRect APS5_VABI _ZN7WebCore37smallestRectWithAspectRatioAroundRectEfRKNS_9FloatRectE(
    float aspectRatio, const WebCore::FloatRect& srcRect) {
    WebCore::FloatRect destRect = srcRect;
    if (aspectRatio < srcRect.size().aspectRatio()) {
        float dy = destRect.width() / aspectRatio - destRect.height();
        destRect.inflateY(dy / 2.0f);
    } else {
        float dx = destRect.height() * aspectRatio - destRect.width();
        destRect.inflateX(dx / 2.0f);
    }
    return destRect;
}

WebCore::IntRect APS5_VABI _ZN7WebCore14roundedIntRectERKNS_9FloatRectE(const WebCore::FloatRect& rect) {
    return WebCore::IntRect{
        WebCore::IntPoint{
            static_cast<int>(std::round(rect.x())),
            static_cast<int>(std::round(rect.y()))
        },
        WebCore::IntSize{
            static_cast<int>(std::round(rect.width())),
            static_cast<int>(std::round(rect.height()))
        }
    };
}

}
