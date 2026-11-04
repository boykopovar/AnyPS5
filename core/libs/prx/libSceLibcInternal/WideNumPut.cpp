#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutBool_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, bool value) {
    return LibcWideNumPutBool_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("1C2-2WB9NN4", sceLibcInternalWideNumPutDoPutBool_nid_no_patch);

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    return LibcWideNumPutLong_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("VpwhOe58wsM", sceLibcInternalWideNumPutDoPutLong_nid_no_patch);

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutUnsignedLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    return LibcWideNumPutUnsignedLong_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("jHo78LGEtmU", sceLibcInternalWideNumPutDoPutUnsignedLong_nid_no_patch);

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::int64_t value) {
    return LibcWideNumPutLongLong_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("9SSHrlIamto", sceLibcInternalWideNumPutDoPutLongLong_nid_no_patch);

GuestLocale::WideOutputIterator APS5_VABI sceLibcInternalWideNumPutDoPutUnsignedLongLong_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::WideOutputIterator out, GuestLocale::IosBase* stream, std::uint16_t fill, std::uint64_t value) {
    return LibcWideNumPutUnsignedLongLong_nid_no_patch(self, out, stream, fill, value);
}
APS5_EXPORT("uX0nKsUo8gc", sceLibcInternalWideNumPutDoPutUnsignedLongLong_nid_no_patch);

}
