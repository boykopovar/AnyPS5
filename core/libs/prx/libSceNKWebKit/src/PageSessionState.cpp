#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

extern "C" {

const void* APS5_VABI WKPageCopySessionState(const void* pageRef, void* context, bool (APS5_VABI *filter)(const void*, std::uint32_t, const void*, void*)) {
    const auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    auto contextVal = reinterpret_cast<std::uintptr_t>(context);
    if ((contextVal & 1) != 0) {
        return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SessionState>(page->sessionStateData));
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Data>(page->sessionStateData));
}

void APS5_VABI WKPageRestoreFromSessionState(const void* pageRef, const void* sessionStateRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* object = NKWebKit::ToObject(sessionStateRef, __func__);
    std::vector<std::uint8_t> data;
    if (object->Type() == NKWebKit::ObjectType::SessionState) {
        data = static_cast<const NKWebKit::SessionState*>(object)->data;
    } else if (object->Type() == NKWebKit::ObjectType::Data) {
        data = static_cast<const NKWebKit::Data*>(object)->bytes;
    } else {
        throw std::invalid_argument(std::string(__func__) + ": invalid session state object type");
    }
    if (page->isClosed) {
        return;
    }
    page->sessionStateData = std::move(data);
    page->sessionStateRestored = true;
    page->sessionStateRestoredWithNavigation = true;
}

void APS5_VABI WKPageRestoreFromSessionStateWithoutNavigation(const void* pageRef, const void* sessionStateRef) {
    auto* page = NKWebKit::Cast<NKWebKit::Page>(pageRef, __func__);
    const auto* object = NKWebKit::ToObject(sessionStateRef, __func__);
    std::vector<std::uint8_t> data;
    if (object->Type() == NKWebKit::ObjectType::SessionState) {
        data = static_cast<const NKWebKit::SessionState*>(object)->data;
    } else if (object->Type() == NKWebKit::ObjectType::Data) {
        data = static_cast<const NKWebKit::Data*>(object)->bytes;
    } else {
        throw std::invalid_argument(std::string(__func__) + ": invalid session state object type");
    }
    if (page->isClosed) {
        return;
    }
    page->sessionStateData = std::move(data);
    page->sessionStateRestored = true;
    page->sessionStateRestoredWithNavigation = false;
}

}
