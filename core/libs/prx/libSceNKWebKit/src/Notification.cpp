#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <atomic>
#include <stdexcept>
#include <string>

namespace NKWebKit {

std::uint64_t GenerateNotificationIdentifier() {
    static std::atomic<std::uint64_t> currentIdentifier{0};
    return ++currentIdentifier;
}

}

extern "C" {

const void* APS5_VABI WKNotificationCopyTitle(const void* notificationRef) {
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(notification->title));
}

const void* APS5_VABI WKNotificationCopyBody(const void* notificationRef) {
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(notification->body));
}

const void* APS5_VABI WKNotificationCopyIconURL(const void* notificationRef) {
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(notification->iconURL));
}

const void* APS5_VABI WKNotificationCopyTag(const void* notificationRef) {
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(notification->tag));
}

const void* APS5_VABI WKNotificationCopyLang(const void* notificationRef) {
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(notification->lang));
}

const void* APS5_VABI WKNotificationCopyDir(const void* notificationRef) {
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    switch (notification->dir) {
    case NKWebKit::NotificationDirection::Auto:
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"auto")));
    case NKWebKit::NotificationDirection::Ltr:
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"ltr")));
    case NKWebKit::NotificationDirection::Rtl:
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(std::u16string(u"rtl")));
    }
    throw std::logic_error(std::string(__func__) + ": notification direction outside the enumeration");
}

std::uint64_t APS5_VABI WKNotificationGetID(const void* notificationRef) {
    const auto* notification = NKWebKit::Cast<NKWebKit::Notification>(notificationRef, __func__);
    return notification->notificationID;
}

}
