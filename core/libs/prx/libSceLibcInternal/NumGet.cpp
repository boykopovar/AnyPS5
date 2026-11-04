#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdint>

namespace {

const GuestLocale::NumGetVtable& NumGet(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::NumGetVtable*>(self->vtable);
}

}

extern "C" {

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value) {
    return NumGet(self).getBool(self, first, last, stream, state, value);
}
APS5_EXPORT("OWO5cpNw3NA", sceLibcInternalNumGetBool_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return NumGet(self).getUnsignedShort(self, first, last, stream, state, value);
}
APS5_EXPORT("gozsp4urvq8", sceLibcInternalNumGetUnsignedShort_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return NumGet(self).getUnsignedInt(self, first, last, stream, state, value);
}
APS5_EXPORT("N1VqUWz2OEI", sceLibcInternalNumGetUnsignedInt_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLong(self, first, last, stream, state, value);
}
APS5_EXPORT("I2UzwkwwEPs", sceLibcInternalNumGetLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLong(self, first, last, stream, state, value);
}
APS5_EXPORT("2bfL3yIBi5k", sceLibcInternalNumGetUnsignedLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("4hiQK82QuLc", sceLibcInternalNumGetLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("eZfFLyWCkvg", sceLibcInternalNumGetUnsignedLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetFloat_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return NumGet(self).getFloat(self, first, last, stream, state, value);
}
APS5_EXPORT("6RGkooTERsE", sceLibcInternalNumGetFloat_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return NumGet(self).getDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("mAwXCpkWaYc", sceLibcInternalNumGetDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return NumGet(self).getLongDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("wUCRGap1j0U", sceLibcInternalNumGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalNumGetPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return NumGet(self).getPointer(self, first, last, stream, state, value);
}
APS5_EXPORT("my9ujasm6-0", sceLibcInternalNumGetPointer_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, bool* value) {
    return NumGet(self).getBool(self, first, last, stream, state, value);
}
APS5_EXPORT("vW-nnV62ea4", sceLibcInternalWideNumGetBool_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedShort_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint16_t* value) {
    return NumGet(self).getUnsignedShort(self, first, last, stream, state, value);
}
APS5_EXPORT("bZ+lKHGvOr8", sceLibcInternalWideNumGetUnsignedShort_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedInt_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint32_t* value) {
    return NumGet(self).getUnsignedInt(self, first, last, stream, state, value);
}
APS5_EXPORT("Y3hBU5FYmhM", sceLibcInternalWideNumGetUnsignedInt_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLong(self, first, last, stream, state, value);
}
APS5_EXPORT("-m2YPwVCwJQ", sceLibcInternalWideNumGetLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLong(self, first, last, stream, state, value);
}
APS5_EXPORT("94ZLp2+AOq0", sceLibcInternalWideNumGetUnsignedLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::int64_t* value) {
    return NumGet(self).getLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("cG5hQhjFGog", sceLibcInternalWideNumGetLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, std::uint64_t* value) {
    return NumGet(self).getUnsignedLongLong(self, first, last, stream, state, value);
}
APS5_EXPORT("banNSumaAZ0", sceLibcInternalWideNumGetUnsignedLongLong_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetFloat_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, float* value) {
    return NumGet(self).getFloat(self, first, last, stream, state, value);
}
APS5_EXPORT("2mb8FYgER+E", sceLibcInternalWideNumGetFloat_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, double* value) {
    return NumGet(self).getDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("+hjXHfvy1Mg", sceLibcInternalWideNumGetDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, long double* value) {
    return NumGet(self).getLongDouble(self, first, last, stream, state, value);
}
APS5_EXPORT("xLZr4GJRMLo", sceLibcInternalWideNumGetLongDouble_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideNumGetPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, void** value) {
    return NumGet(self).getPointer(self, first, last, stream, state, value);
}
APS5_EXPORT("zomvAQ5RFdA", sceLibcInternalWideNumGetPointer_nid_no_patch);

}
