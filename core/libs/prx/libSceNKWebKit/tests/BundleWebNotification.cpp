#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>
#include <utility>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKBundle web notification check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}  // namespace

int main() {
    auto bundle = NKWebKit::Make<NKWebKit::Bundle>();
    auto notification = NKWebKit::Make<NKWebKit::Notification>();
    auto page = NKWebKit::Make<NKWebKit::BundlePage>();
    auto origin = NKWebKit::Make<NKWebKit::SecurityOrigin>();

    const std::uint64_t id = WKBundleGetWebNotificationID(bundle.Get(), notification.Get());
    Require(id == notification->notificationID);
    Require(id != 0);

    WKBundleSetWebNotificationPermission(bundle.Get(), page.Get(), origin.Get(), true);
    auto it = bundle->notificationPermissions.find(std::make_pair(static_cast<const void*>(page.Get()), static_cast<const void*>(origin.Get())));
    Require(it != bundle->notificationPermissions.end());
    Require(it->second == true);

    WKBundleSetWebNotificationPermission(bundle.Get(), page.Get(), origin.Get(), false);
    it = bundle->notificationPermissions.find(std::make_pair(static_cast<const void*>(page.Get()), static_cast<const void*>(origin.Get())));
    Require(it != bundle->notificationPermissions.end());
    Require(it->second == false);

    RequireInvalidArgument([&] { WKBundleGetWebNotificationID(nullptr, notification.Get()); });
    RequireInvalidArgument([&] { WKBundleGetWebNotificationID(bundle.Get(), nullptr); });
    RequireInvalidArgument([&] { WKBundleSetWebNotificationPermission(nullptr, page.Get(), origin.Get(), true); });
    RequireInvalidArgument([&] { WKBundleSetWebNotificationPermission(bundle.Get(), nullptr, origin.Get(), true); });
    RequireInvalidArgument([&] { WKBundleSetWebNotificationPermission(bundle.Get(), page.Get(), nullptr, true); });

    RequireInvalidArgument([&] { WKBundleGetWebNotificationID(page.Get(), notification.Get()); });
    RequireInvalidArgument([&] { WKBundleGetWebNotificationID(bundle.Get(), page.Get()); });
    RequireInvalidArgument([&] { WKBundleSetWebNotificationPermission(page.Get(), page.Get(), origin.Get(), true); });
    RequireInvalidArgument([&] { WKBundleSetWebNotificationPermission(bundle.Get(), bundle.Get(), origin.Get(), true); });
    RequireInvalidArgument([&] { WKBundleSetWebNotificationPermission(bundle.Get(), page.Get(), page.Get(), true); });

    return 0;
}
