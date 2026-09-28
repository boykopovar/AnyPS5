#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

extern "C" {

int APS5_VABI sceAgcAcbPushMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetWorkloadComplete() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetWorkloadStreamInactive() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcbSetWorkloadsActive() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcAcquireMemSetEngine() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDcbPushMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDcbSetMarkerSpan() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDcbSetWorkloadStreamInactive() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcDebugRaiseException() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetDataPacketPayloadRange() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetDefaultCxStateFlat() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetGsOversubscription() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcGetIsTrinityMode() {
    return 0;
}

int APS5_VABI sceAgcGetSemaphoreLabel() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceAgcSetAmmSemaphoreMemory() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("-KRzWekV120", sceAgcUnknown__MKRzWekV120);
// Unnamed DCB state toggle: games call it as (cb, enable, 0, 0) and ignore the result. It sits
// between SetBaseIndirectArgs and SetIndexBuffer in the import table; treated as a no-op.
std::uint32_t* APS5_VABI sceAgcUnknown__MKRzWekV120(void* buf, std::uint32_t enable, std::uint64_t a2, std::uint64_t a3) {
    static bool logged = false;
    if (!logged) {
        logged = true;
        std::fprintf(stderr, "[agc] -KRzWekV120 treated as no-op (enable=%u args=0x%llx,0x%llx)" "\n", enable, static_cast<unsigned long long>(a2), static_cast<unsigned long long>(a3));
    }
    (void)buf;
    return nullptr;
}

APS5_EXPORT("6nths4DHNrs", sceAgcUnknown_6nths4DHNrs);
int APS5_VABI sceAgcUnknown_6nths4DHNrs() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("7Wa3aeJgeVU", sceAgcUnknown_7Wa3aeJgeVU);
int APS5_VABI sceAgcUnknown_7Wa3aeJgeVU() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("EJBA4dbmvfg", sceAgcUnknown_EJBA4dbmvfg);
int APS5_VABI sceAgcUnknown_EJBA4dbmvfg() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("ICkECTBxrMw", sceAgcUnknown_ICkECTBxrMw);
int APS5_VABI sceAgcUnknown_ICkECTBxrMw() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("Ikfdt-rIqCE", sceAgcUnknown_Ikfdt_MrIqCE);
int APS5_VABI sceAgcUnknown_Ikfdt_MrIqCE() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("RTpj-tIlvZc", sceAgcUnknown_RTpj_MtIlvZc);
int APS5_VABI sceAgcUnknown_RTpj_MtIlvZc() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("SwI6QxqwAC0", sceAgcUnknown_SwI6QxqwAC0);
int APS5_VABI sceAgcUnknown_SwI6QxqwAC0() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("d4NZIlguzv0", sceAgcUnknown_d4NZIlguzv0);
int APS5_VABI sceAgcUnknown_d4NZIlguzv0() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("eAy8eGNsCuU", sceAgcUnknown_eAy8eGNsCuU);
int APS5_VABI sceAgcUnknown_eAy8eGNsCuU() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("rP5xLdOf26k", sceAgcUnknown_rP5xLdOf26k);
int APS5_VABI sceAgcUnknown_rP5xLdOf26k() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("tmy-+rBpspY", sceAgcUnknown_tmy_M_PrBpspY);
int APS5_VABI sceAgcUnknown_tmy_M_PrBpspY() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

APS5_EXPORT("y5K5tPktiL8", sceAgcUnknown_y5K5tPktiL8);
int APS5_VABI sceAgcUnknown_y5K5tPktiL8() {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

}
