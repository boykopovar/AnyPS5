#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/PasteboardWriterDataExports.hpp"

#include <stdexcept>
#include <string>

namespace {

using NKWebKit::WebCoreAbi::PasteboardPlainText;
using NKWebKit::WebCoreAbi::PasteboardURLData;
using NKWebKit::WebCoreAbi::PasteboardWriterData;

template <typename T>
T& Checked(T* self, const char* operation) {
    if (!self) throw std::invalid_argument(std::string("WebCore::PasteboardWriterData::") + operation + ": null this");
    return *self;
}

void Construct(PasteboardWriterData* self) {
    auto& data = Checked(self, "PasteboardWriterData");
    data.plainText.engaged = false;
    data.url.engaged = false;
    data.webContent.engaged = false;
}

void Destroy(PasteboardWriterData* self) {
    auto& data = Checked(self, "~PasteboardWriterData");
    data.webContent.engaged = false;
    if (data.url.engaged) {
        auto& url = data.url.value();
        url.title.~String();
        NKWebKit::WtfAbi::Release(url.url.impl);
        data.url.engaged = false;
    }
    if (data.plainText.engaged) {
        data.plainText.value().text.~String();
        data.plainText.engaged = false;
    }
}

}  // namespace

extern "C" {

void APS5_VABI _ZN7WebCore20PasteboardWriterDataC1Ev(PasteboardWriterData* self) {
    Construct(self);
}

void APS5_VABI _ZN7WebCore20PasteboardWriterDataC2Ev(PasteboardWriterData* self) {
    Construct(self);
}

void APS5_VABI _ZN7WebCore20PasteboardWriterDataD1Ev(PasteboardWriterData* self) {
    Destroy(self);
}

void APS5_VABI _ZN7WebCore20PasteboardWriterDataD2Ev(PasteboardWriterData* self) {
    Destroy(self);
}

bool APS5_VABI _ZNK7WebCore20PasteboardWriterData7isEmptyEv(const PasteboardWriterData* self) {
    const auto& data = Checked(self, "isEmpty");
    return !data.plainText.engaged && !data.url.engaged && !data.webContent.engaged;
}

}
