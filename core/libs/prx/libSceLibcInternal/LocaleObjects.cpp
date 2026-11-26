#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports2.hpp"
#include "prx/libSceLibcInternal/include/RuntimeInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <stdexcept>

namespace {

std::atomic<GuestLocale::Implementation*> g_globalLocale{nullptr};

} // namespace

extern "C" {

void APS5_VABI sceLibcInternalLocinfoBaseConstructor_nid_no_patch(void* self, const char* localeName) {
    _ZNSt8_LocinfoC1EPKc_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self), localeName);
}
APS5_EXPORT("pN02FS5SPgg", sceLibcInternalLocinfoBaseConstructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocinfoBaseDestructor_nid_no_patch(void* self) {
    _ZNSt8_LocinfoD1Ev_nid_postfix(static_cast<GuestLocale::LocinfoStorage*>(self));
}
APS5_EXPORT("YXVCU6PdgZk", sceLibcInternalLocinfoBaseDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch(void* self, const void* source) {
    _ZNSt6locale7_LocimpC1ERKS0__nid_postfix(static_cast<GuestLocale::Implementation*>(self), static_cast<const GuestLocale::Implementation*>(source));
}
APS5_EXPORT("dsJKehuajH4", sceLibcInternalLocimpBaseCopyConstructor_nid_no_patch);

GuestLocaleFacade* APS5_VABI _ZNSt6locale6globalERKS__nid_postfix(GuestLocaleFacade* result, const GuestLocaleFacade* locale) {
    if (result == nullptr || locale == nullptr || locale->implementation == nullptr) {
        throw std::invalid_argument("locale::global: null locale");
    }
    result->implementation = LibcSwapGlobalLocale_nid_no_patch(locale->implementation);
    return result;
}

void APS5_VABI _ZNSt6locale7_LocimpC2Eb_nid_postfix(GuestLocale::Implementation* self, bool transparent) {
    if (self == nullptr) {
        throw std::invalid_argument("_Locimp: null object");
    }
    *self = {{LibcLocimpVtable_nid_no_patch(), 1, 0}, nullptr, 0, 0, transparent, "*"};
}

void APS5_VABI _ZNSt8_LocinfoC1EiPKc_nid_postfix(GuestLocale::LocinfoStorage* self, int category, const char* localeName) {
    if (self == nullptr) {
        throw std::invalid_argument("_Locinfo: null object");
    }
    if (category < 0 || (static_cast<unsigned int>(category) & ~0x3fU) != 0) {
        throw std::invalid_argument("_Locinfo: invalid locale category");
    }
    _ZNSt8_LocinfoC1EPKc_nid_postfix(self, localeName);
}

void* APS5_VABI _Getgloballocale_nid_postfix() {
    return g_globalLocale.load(std::memory_order_acquire);
}

void APS5_VABI _Setgloballocale_nid_postfix(void* pointer) {
    g_globalLocale.store(static_cast<GuestLocale::Implementation*>(pointer), std::memory_order_release);
}

void APS5_VABI _Deletegloballocale_nid_postfix(void* pointer) {
    if (pointer == nullptr) throw std::invalid_argument("_Deletegloballocale: null locale reference");
    auto* locale = *static_cast<GuestLocale::Implementation**>(pointer);
    if (locale == nullptr) return;
    if (auto* released = locale->base.vtable->release(&locale->base)) released->vtable->deleteObject(released);
}

}
