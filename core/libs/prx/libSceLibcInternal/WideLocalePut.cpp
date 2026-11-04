#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, char16_t fill, double value) {
    return LibcWideNumPutDouble_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("sX3o6Zmihw0", sceLibcInternalWideNumPutDoPutDouble_nid_no_patch);

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutLongDouble_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, char16_t fill, long double value) {
    return LibcWideNumPutLongDouble_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("6OYWLisfrB8", sceLibcInternalWideNumPutDoPutLongDouble_nid_no_patch);

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutPointer_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, char16_t fill, const void* value) {
    return LibcWideNumPutPointer_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("BDteGj1gqBY", sceLibcInternalWideNumPutDoPutPointer_nid_no_patch);

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideTimePutDoPut_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, char16_t fill, const GuestTm* time, char format, char modifier) {
    return LibcWideTimePutDoPut_nid_no_patch(self, out, stream, fill, time, format, modifier);
}
APS5_EXPORT("Fr7j8dMsy4s", sceLibcInternalWideTimePutDoPut_nid_no_patch);

}
