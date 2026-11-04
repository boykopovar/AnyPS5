#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, bool value) {
    return LibcNumPutMethods_nid_no_patch()->putBool(self, out, stream, fill, value);
}
APS5_EXPORT("ke36E2bqNmI", sceLibcInternalNumPutDoPutBool_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return LibcNumPutMethods_nid_no_patch()->putLong(self, out, stream, fill, value);
}
APS5_EXPORT("I3+xmBWGPGo", sceLibcInternalNumPutDoPutLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return LibcNumPutMethods_nid_no_patch()->putUnsignedLong(self, out, stream, fill, value);
}
APS5_EXPORT("nlAk46weq1w", sceLibcInternalNumPutDoPutUnsignedLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::int64_t value) {
    return LibcNumPutMethods_nid_no_patch()->putLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("H2KGT3vA7yQ", sceLibcInternalNumPutDoPutLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, std::uint64_t value) {
    return LibcNumPutMethods_nid_no_patch()->putUnsignedLongLong(self, out, stream, fill, value);
}
APS5_EXPORT("Vbeoft607aI", sceLibcInternalNumPutDoPutUnsignedLongLong_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, double value) {
    return LibcNumPutMethods_nid_no_patch()->putDouble(self, out, stream, fill, value);
}
APS5_EXPORT("F+cp2B3cWNU", sceLibcInternalNumPutDoPutDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, long double value) {
    return LibcNumPutMethods_nid_no_patch()->putLongDouble(self, out, stream, fill, value);
}
APS5_EXPORT("rLiFc4+HyHw", sceLibcInternalNumPutDoPutLongDouble_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalNumPutDoPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const void* value) {
    return LibcNumPutMethods_nid_no_patch()->putPointer(self, out, stream, fill, value);
}
APS5_EXPORT("0xgFRKf0Lc4", sceLibcInternalNumPutDoPutPointer_nid_no_patch);

GuestLocale::OutputIterator APS5_VABI sceLibcInternalTimePutDoPut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::OutputIterator out, GuestLocale::IosBase* stream, char fill, const GuestTm* time, char format, char modifier) {
    return LibcTimePutMethods_nid_no_patch()->put(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("+i81FtUCarA", sceLibcInternalTimePutDoPut_nid_no_patch);

}
