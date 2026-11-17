#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

std::uintptr_t ReadWord(std::uintptr_t address, std::size_t offset, const char* function) {
    std::uintptr_t word = 0;
    const int savedErrno = errno;
    const bool copied = address <= std::numeric_limits<std::uintptr_t>::max() - offset &&
                        GuestCopyIn_nid_no_patch(&word, reinterpret_cast<const void*>(address + offset), sizeof(word));
    errno = savedErrno;
    if (!copied) throw std::invalid_argument(std::string(function) + ": unreadable facet vtable");
    return word;
}

template<typename TVtable>
decltype(TVtable::put) TimePutSlot(const GuestLocale::Facet* self, const char* function) {
    const std::uintptr_t vtable = ReadWord(reinterpret_cast<std::uintptr_t>(self), 0, function);
    return reinterpret_cast<decltype(TVtable::put)>(ReadWord(vtable, offsetof(TVtable, put), function));
}

const GuestLocale::NumPutVtable& NumPut(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::NumPutVtable*>(self->vtable);
}

}

extern "C" {

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const void* value) {
    return NumPut(self).putPointer(self, out, stream, fill, value);
}
APS5_EXPORT("8NVUcufbklM", sceLibcInternalNumPutPointer_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, bool value) {
    return NumPut(self).putBool(self, out, stream, fill, value);
}
APS5_EXPORT("nftirmo6hBg", sceLibcInternalNumPutBool_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, double value) {
    return NumPut(self).putDouble(self, out, stream, fill, value);
}
APS5_EXPORT("w9NzCYAjEpQ", sceLibcInternalNumPutDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, long double value) {
    return NumPut(self).putLongDouble(self, out, stream, fill, value);
}
APS5_EXPORT("VPcTGA-LwSo", sceLibcInternalNumPutLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return NumPut(self).putLong(self, out, stream, fill, value);
}
APS5_EXPORT("ffnhh0HcxJ4", sceLibcInternalNumPutLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return NumPut(self).putUnsignedLong(self, out, stream, fill, value);
}
APS5_EXPORT("uODuM76vS4U", sceLibcInternalNumPutUnsignedLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return NumPut(self).putLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("NJtKruu9qOs", sceLibcInternalNumPutLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return NumPut(self).putUnsignedLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("dep6W2Ix35s", sceLibcInternalNumPutUnsignedLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalTimePutPattern_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, const char* first, const char* last) {
    return LibcTimePutPattern_nid_no_patch(self, out, stream, fill, time, first, last);
}
APS5_EXPORT("j9LU8GsuEGw", sceLibcInternalTimePutPattern_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalTimePut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, char format, char modifier) {
    return TimePutSlot<GuestLocale::TimePutVtable>(self, __func__)(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("qkuA-unH7PU", sceLibcInternalTimePut_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideTimePutPattern_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, const std::uint16_t* first, const std::uint16_t* last) {
    return LibcWideTimePutPattern_nid_no_patch(self, out, stream, fill, time, first, last);
}
APS5_EXPORT("Sc0lXhQG5Ko", sceLibcInternalWideTimePutPattern_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideTimePut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier) {
    return TimePutSlot<GuestLocale::WideTimePutVtable>(self, __func__)(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("Nt6eyVKm+Z4", sceLibcInternalWideTimePut_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalWideTimePutDoPut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, const GuestTm* time, char format, char modifier) {
    return LibcWideTimePutDoPut_nid_no_patch(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("Fr7j8dMsy4s", sceLibcInternalWideTimePutDoPut_nid_no_patch);

std::size_t APS5_VABI sceLibcInternalWideTimePutGetcat_nid_no_patch(GuestLocale::Facet** facet, const GuestLocale::Implementation* const* locale) {
    return LibcWideTimePutGetcat_nid_no_patch(facet, locale);
}
APS5_EXPORT("bF2uVCqVhBY", sceLibcInternalWideTimePutGetcat_nid_no_patch);

}
