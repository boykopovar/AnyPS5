#include "prx/libScePad/include/LaterExports.hpp"
#include "prx/libScePad/include/Pad.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

#include <cerrno>
#include <cstdint>

extern "C" {

int APS5_VABI libScePadUnknown_1YXygubV1Gc_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("1YXygubV1Gc", libScePadUnknown_1YXygubV1Gc_nid_no_patch);

int APS5_VABI scePadInternalSetAssignLed() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libScePadUnknown_7xA_plus_hFtvBCA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("7xA+hFtvBCA", libScePadUnknown_7xA_plus_hFtvBCA_nid_no_patch);

int APS5_VABI libScePadUnknown_KY0hSB_plus_Uyfo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("KY0hSB+Uyfo", libScePadUnknown_KY0hSB_plus_Uyfo_nid_no_patch);

int APS5_VABI libScePadUnknown_Ql4q5ePNaw0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Ql4q5ePNaw0", libScePadUnknown_Ql4q5ePNaw0_nid_no_patch);

int APS5_VABI scePadIsChargingCableConnected() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libScePadUnknown_XFqNcPu1meI_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("XFqNcPu1meI", libScePadUnknown_XFqNcPu1meI_nid_no_patch);

int APS5_VABI libScePadUnknown_eqldkyw7GAo_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("eqldkyw7GAo", libScePadUnknown_eqldkyw7GAo_nid_no_patch);

int APS5_VABI libScePadUnknown_ickjfjk9okM_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ickjfjk9okM", libScePadUnknown_ickjfjk9okM_nid_no_patch);

int APS5_VABI libScePadUnknown_qJgegDoE0g8_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("qJgegDoE0g8", libScePadUnknown_qJgegDoE0g8_nid_no_patch);

int APS5_VABI scePadSetVibrationRawStrength() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadVirtualDeviceInsertTriggerState() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI scePadAstonGetDeviceInformation() {
    NotImplemented_nid_no_patch(__func__);
}

}

extern "C" {

int APS5_VABI scePadIsInitialUserHandle() {
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI libScePadUnknown_710T88tQmn4_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("710T88tQmn4", libScePadUnknown_710T88tQmn4_nid_no_patch);

int APS5_VABI libScePadUnknown_FlTOSPI5cnk_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("FlTOSPI5cnk", libScePadUnknown_FlTOSPI5cnk_nid_no_patch);

int APS5_VABI libScePadUnknown_WH0f85fU998_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("WH0f85fU998", libScePadUnknown_WH0f85fU998_nid_no_patch);

int APS5_VABI libScePadUnknown_oARLr0ca9N8_nid_no_patch(int handle, std::uint8_t* out) {
    if (scePadUnknown_n3kSX62fgNo() != PAD_OK) {
        return PAD_ERROR_NOT_INITIALIZED;
    }
    if (out == nullptr) {
        return PAD_ERROR_INVALID_ARG;
    }
    if (!scePadIsValidHandle(handle, PAD_PORT_TYPE_STANDARD) &&
        !scePadIsValidHandle(handle, PAD_PORT_TYPE_SPECIAL)) {
        return PAD_ERROR_INVALID_HANDLE;
    }
    const std::uint8_t value = 0;
    const bool copied = GuestCopyOut_nid_no_patch(out, &value, sizeof(value));
    if (!copied) {
        return PAD_ERROR_INVALID_ARG;
    }
    return PAD_OK;
}
APS5_EXPORT("oARLr0ca9N8", libScePadUnknown_oARLr0ca9N8_nid_no_patch);

int APS5_VABI libScePadUnknown_ovL0DogAAww_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("ovL0DogAAww", libScePadUnknown_ovL0DogAAww_nid_no_patch);

int APS5_VABI libScePadUnknown_s7DjmajUf0I_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("s7DjmajUf0I", libScePadUnknown_s7DjmajUf0I_nid_no_patch);

int APS5_VABI libScePadUnknown_vtIU85W8xpY_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("vtIU85W8xpY", libScePadUnknown_vtIU85W8xpY_nid_no_patch);

}
