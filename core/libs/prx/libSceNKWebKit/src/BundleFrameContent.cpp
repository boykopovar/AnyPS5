#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <stdexcept>
#include <string>

namespace {

template<typename T>
const T* CheckedObject(const void* ref, const char* function) {
    if (!ref) throw std::invalid_argument(std::string(function) + ": null object reference");
    if (!GuestReadable_nid_no_patch(ref, sizeof(NKWebKit::Object)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    const auto* object = NKWebKit::Cast<T>(ref, function);
    if (!GuestReadable_nid_no_patch(ref, sizeof(T)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return object;
}

double ReadGuestDouble(const double* pointer, const char* function) {
    double value = 0.0;
    if (!GuestCopyIn_nid_no_patch(&value, pointer, sizeof(double)))
        NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void RequireWritableGuestDouble(double* pointer, const char* function) {
    bool ok = false;
    {
        const GuestArena::HostWrite guard(pointer, sizeof(double));
        ok = guard.Open() && GuestWritable_nid_no_patch(pointer, sizeof(double));
    }
    if (!ok) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

bool TryPublishGuestDouble(double* pointer, double value, double previous) {
    if (GuestCopyOut_nid_no_patch(pointer, &value, sizeof(double))) return true;
    static_cast<void>(GuestCopyOut_nid_no_patch(pointer, &previous, sizeof(double)));
    return false;
}

}  // namespace

extern "C" {

const void* APS5_VABI WKBundleFrameCopyCounterValue(const void* frameRef, const void* nodeHandleRef) {
    const auto* frame = CheckedObject<NKWebKit::BundleFrame>(frameRef, __func__);
    static_cast<void>(CheckedObject<NKWebKit::BundleNodeHandle>(nodeHandleRef, __func__));
    return frame->counterValue ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->counterValue->characters))
                               : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopyInnerText(const void* frameRef) {
    const auto* frame = CheckedObject<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->innerText ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->innerText->characters))
                            : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopyLayerTreeAsText(const void* frameRef) {
    const auto* frame = CheckedObject<NKWebKit::BundleFrame>(frameRef, __func__);
    return frame->layerTreeAsText ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->layerTreeAsText->characters))
                                  : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopyMIMETypeForResourceWithURL(const void* frameRef, const void* urlRef) {
    const auto* frame = CheckedObject<NKWebKit::BundleFrame>(frameRef, __func__);
    static_cast<void>(CheckedObject<NKWebKit::URL>(urlRef, __func__));
    return frame->mimeTypeForResource ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->mimeTypeForResource->characters))
                                      : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

const void* APS5_VABI WKBundleFrameCopySuggestedFilenameForResourceWithURL(const void* frameRef, const void* urlRef) {
    const auto* frame = CheckedObject<NKWebKit::BundleFrame>(frameRef, __func__);
    static_cast<void>(CheckedObject<NKWebKit::URL>(urlRef, __func__));
    return frame->suggestedFilenameForResource
        ? NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>(frame->suggestedFilenameForResource->characters))
        : NKWebKit::ToRef(NKWebKit::String::FromUtf8(""));
}

bool APS5_VABI WKBundleFrameGetDocumentBackgroundColor(const void* frameRef, double* red, double* green, double* blue, double* alpha) {
    const auto* frame = CheckedObject<NKWebKit::BundleFrame>(frameRef, __func__);
    if (!red || !green || !blue || !alpha) {
        throw std::invalid_argument(std::string(__func__) + ": null pointer argument");
    }
    const double previousRed = ReadGuestDouble(red, __func__);
    const double previousGreen = ReadGuestDouble(green, __func__);
    const double previousBlue = ReadGuestDouble(blue, __func__);
    const double previousAlpha = ReadGuestDouble(alpha, __func__);
    RequireWritableGuestDouble(red, __func__);
    RequireWritableGuestDouble(green, __func__);
    RequireWritableGuestDouble(blue, __func__);
    RequireWritableGuestDouble(alpha, __func__);
    const bool hasColor = frame->hasDocumentBackgroundColor;
    const double outRed = hasColor ? frame->documentBackgroundColor[0] : 1.0;
    const double outGreen = hasColor ? frame->documentBackgroundColor[1] : 1.0;
    const double outBlue = hasColor ? frame->documentBackgroundColor[2] : 1.0;
    const double outAlpha = hasColor ? frame->documentBackgroundColor[3] : 1.0;
    if (!TryPublishGuestDouble(red, outRed, previousRed))
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    if (!TryPublishGuestDouble(green, outGreen, previousGreen)) {
        static_cast<void>(GuestCopyOut_nid_no_patch(red, &previousRed, sizeof(double)));
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (!TryPublishGuestDouble(blue, outBlue, previousBlue)) {
        static_cast<void>(GuestCopyOut_nid_no_patch(green, &previousGreen, sizeof(double)));
        static_cast<void>(GuestCopyOut_nid_no_patch(red, &previousRed, sizeof(double)));
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    if (!TryPublishGuestDouble(alpha, outAlpha, previousAlpha)) {
        static_cast<void>(GuestCopyOut_nid_no_patch(blue, &previousBlue, sizeof(double)));
        static_cast<void>(GuestCopyOut_nid_no_patch(green, &previousGreen, sizeof(double)));
        static_cast<void>(GuestCopyOut_nid_no_patch(red, &previousRed, sizeof(double)));
        NotImplementedAt_nid_no_patch(__func__, __FILE__, __LINE__);
    }
    return hasColor;
}

}
