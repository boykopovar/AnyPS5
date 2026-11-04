#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdint>

namespace {

const GuestLocale::WideNumPutVtable& WideNumPut(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::WideNumPutVtable*>(self->vtable);
}

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, bool value) {
    return WideNumPut(self).putBool(self, out, stream, fill, value);
}
APS5_EXPORT("mY9FWooxqJY", sceLibcInternalWideNumPutBool_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    return WideNumPut(self).putLong(self, out, stream, fill, value);
}
APS5_EXPORT("USLhWp7sZoU", sceLibcInternalWideNumPutLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    return WideNumPut(self).putUnsignedLong(self, out, stream, fill, value);
}
APS5_EXPORT("qtpzdwMMCPc", sceLibcInternalWideNumPutUnsignedLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    return WideNumPut(self).putLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("ryykbHJ04Cw", sceLibcInternalWideNumPutLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    return WideNumPut(self).putUnsignedLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("lmb3oBpMNPU", sceLibcInternalWideNumPutUnsignedLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, double value) {
    return WideNumPut(self).putDouble(self, out, stream, fill, value);
}
APS5_EXPORT("V7aIsVIsIIA", sceLibcInternalWideNumPutDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, long double value) {
    return WideNumPut(self).putLongDouble(self, out, stream, fill, value);
}
APS5_EXPORT("vCIFGeI6adI", sceLibcInternalWideNumPutLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideNumPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const void* value) {
    return WideNumPut(self).putPointer(self, out, stream, fill, value);
}
APS5_EXPORT("xfOSCbCiY44", sceLibcInternalWideNumPutPointer_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideTimePutPattern_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, const std::uint16_t* first, const std::uint16_t* last) {
    return LibcWideTimePutPattern_nid_no_patch(self, out, stream, fill, time, first, last);
}
APS5_EXPORT("Sc0lXhQG5Ko", sceLibcInternalWideTimePutPattern_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideTimePut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier) {
    return reinterpret_cast<const GuestLocale::WideTimePutVtable*>(self->vtable)->put(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("Nt6eyVKm+Z4", sceLibcInternalWideTimePut_nid_no_patch);

}
