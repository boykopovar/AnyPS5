#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {

void APS5_VABI WKOpenPanelResultListenerCancel(const void* listenerRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::OpenPanelResultListener>(listenerRef, __func__);
    listener->cancelled = true;
    listener->chosenFiles.clear();
}

void APS5_VABI WKOpenPanelResultListenerChooseFiles(const void* listenerRef, const void* fileURLsRef, const void* allowedMimeTypesRef) {
    auto* listener = NKWebKit::Cast<NKWebKit::OpenPanelResultListener>(listenerRef, __func__);
    listener->cancelled = false;
    listener->chosenFiles.clear();
    const auto* array = NKWebKit::CastOrNull<NKWebKit::Array>(fileURLsRef, __func__);
    if (array) {
        for (const auto& item : array->elements) {
            if (const auto* url = NKWebKit::CastOrNull<NKWebKit::URL>(item.Get(), __func__)) {
                std::string path = url->text;
                if (path.rfind("file://", 0) == 0) {
                    path = path.substr(7);
                }
                listener->chosenFiles.push_back(NKWebKit::String::FromUtf8(path.c_str()));
            } else if (const auto* str = NKWebKit::CastOrNull<NKWebKit::String>(item.Get(), __func__)) {
                std::string path = str->Utf8();
                if (path.rfind("file://", 0) == 0) {
                    path = path.substr(7);
                }
                listener->chosenFiles.push_back(NKWebKit::String::FromUtf8(path.c_str()));
            }
        }
    }
}

}
