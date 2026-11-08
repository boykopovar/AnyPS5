#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"

extern "C" {

char APS5_VABI sceLibcInternalNumpunctDoDecimalPoint_nid_no_patch(const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE16do_decimal_pointEv_nid_postfix(self);
}
APS5_EXPORT("JXzQGOtumdM", sceLibcInternalNumpunctDoDecimalPoint_nid_no_patch);

char APS5_VABI sceLibcInternalNumpunctDoThousandsSep_nid_no_patch(const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE16do_thousands_sepEv_nid_postfix(self);
}
APS5_EXPORT("zv1EMhI7R1c", sceLibcInternalNumpunctDoThousandsSep_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalNumpunctDoGrouping_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE11do_groupingEv_nid_postfix(result, self);
}
APS5_EXPORT("cnNz2ftNwEU", sceLibcInternalNumpunctDoGrouping_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalNumpunctDoFalsename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE12do_falsenameEv_nid_postfix(result, self);
}
APS5_EXPORT("ozLi0i4r6ds", sceLibcInternalNumpunctDoFalsename_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalNumpunctDoTruename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE11do_truenameEv_nid_postfix(result, self);
}
APS5_EXPORT("nRf0VQ++OEw", sceLibcInternalNumpunctDoTruename_nid_no_patch);

char APS5_VABI sceLibcInternalNumpunctDecimalPoint_nid_no_patch(const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE13decimal_pointEv_nid_postfix(self);
}
APS5_EXPORT("klWxQ2nKAHY", sceLibcInternalNumpunctDecimalPoint_nid_no_patch);

char APS5_VABI sceLibcInternalNumpunctThousandsSep_nid_no_patch(const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE13thousands_sepEv_nid_postfix(self);
}
APS5_EXPORT("QGSIlqfIU2c", sceLibcInternalNumpunctThousandsSep_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalNumpunctGrouping_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE8groupingEv_nid_postfix(result, self);
}
APS5_EXPORT("JWplGh2O0Rs", sceLibcInternalNumpunctGrouping_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalNumpunctFalsename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE9falsenameEv_nid_postfix(result, self);
}
APS5_EXPORT("3+VwUA8-QPI", sceLibcInternalNumpunctFalsename_nid_no_patch);

GuestLocale::String* APS5_VABI sceLibcInternalNumpunctTruename_nid_no_patch(GuestLocale::String* result, const GuestLocale::NumpunctFacet* self) {
    return _ZNKSt8numpunctIcE8truenameEv_nid_postfix(result, self);
}
APS5_EXPORT("fXUuZEw7C24", sceLibcInternalNumpunctTruename_nid_no_patch);

}
