#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"

extern "C" {
const void* APS5_VABI WKPointCreate(NKWebKit::PointValue point) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Point>(point));
}

NKWebKit::PointValue APS5_VABI WKPointGetValue(const void* pointRef) {
    return NKWebKit::Cast<NKWebKit::Point>(pointRef, __func__)->value;
}

const void* APS5_VABI WKSizeCreate(NKWebKit::SizeValue size) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Size>(size));
}

NKWebKit::SizeValue APS5_VABI WKSizeGetValue(const void* sizeRef) {
    return NKWebKit::Cast<NKWebKit::Size>(sizeRef, __func__)->value;
}

const void* APS5_VABI WKRectCreate(NKWebKit::RectValue rect) {
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::Rect>(rect));
}

NKWebKit::RectValue APS5_VABI WKRectGetValue(const void* rectRef) {
    return NKWebKit::Cast<NKWebKit::Rect>(rectRef, __func__)->value;
}

}
