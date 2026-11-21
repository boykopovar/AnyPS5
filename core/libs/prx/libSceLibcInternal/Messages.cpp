#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>

namespace {

void APS5_VABI DestroyWideMessages_nid_no_patch(GuestLocale::Facet*) {}

void APS5_VABI DeleteWideMessages_nid_no_patch(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("messages<wchar_t> delete: null facet");
    _ZdlPv_nid_postfix(self);
}

void APS5_VABI RetainWideMessages_nid_no_patch(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("messages<wchar_t> retain: null facet");
    std::atomic_ref<std::uint32_t>(self->references).fetch_add(1);
}

GuestLocale::Facet* APS5_VABI ReleaseWideMessages_nid_no_patch(GuestLocale::Facet* self) {
    if (self == nullptr) throw std::invalid_argument("messages<wchar_t> release: null facet");
    return std::atomic_ref<std::uint32_t>(self->references).fetch_sub(1) == 1 ? self : nullptr;
}

std::int32_t APS5_VABI OpenWideMessages_nid_no_patch(const GuestLocale::Facet*, const GuestLocale::String*, const void*) {
    return -1;
}

GuestLocale::String* APS5_VABI GetWideMessages_nid_no_patch(GuestLocale::String* result, const GuestLocale::Facet*, std::int32_t, std::int32_t, std::int32_t, const GuestLocale::String* fallback) {
    return LibcCopyWideString_nid_no_patch(result, fallback);
}

void APS5_VABI CloseWideMessages_nid_no_patch(const GuestLocale::Facet*, std::int32_t) {}

const GuestLocale::MessagesVtable g_wideMessagesVtable{
    {DestroyWideMessages_nid_no_patch, DeleteWideMessages_nid_no_patch, RetainWideMessages_nid_no_patch, ReleaseWideMessages_nid_no_patch},
    OpenWideMessages_nid_no_patch, GetWideMessages_nid_no_patch, CloseWideMessages_nid_no_patch};

const GuestLocale::FacetVtable* NarrowVtable() {
    return reinterpret_cast<const GuestLocale::FacetVtable*>(&_ZTVSt8messagesIcE_nid_postfix[2]);
}

void Construct(void* self, const GuestLocale::FacetVtable* vtable, std::size_t references, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    auto* facet = static_cast<GuestLocale::Facet*>(self);
    facet->vtable = vtable;
    facet->references = static_cast<std::uint32_t>(references);
    facet->reserved = static_cast<std::uint32_t>(static_cast<std::uint64_t>(references) >> 32);
}

void ConstructNamed(void* self, const char* localeName, const GuestLocale::FacetVtable* vtable, std::size_t references, const char* function) {
    if (self == nullptr) throw std::invalid_argument(std::string(function) + ": null object");
    GuestLocale::LocinfoStorage locinfo;
    _ZNSt8_LocinfoC1EPKc_nid_postfix(&locinfo, localeName);
    _ZNSt8_LocinfoD1Ev_nid_postfix(&locinfo);
    Construct(self, vtable, references, function);
}

void ConstructFrom(void* self, const GuestLocale::LocinfoStorage* locinfo, const GuestLocale::FacetVtable* vtable, std::size_t references, const char* function) {
    if (locinfo == nullptr) throw std::invalid_argument(std::string(function) + ": null _Locinfo");
    Construct(self, vtable, references, function);
}

}

extern "C" {

void APS5_VABI _ZNSt8messagesIcEC1EPKcm_nid_postfix(void* self, const char* localeName, std::size_t references) {
    ConstructNamed(self, localeName, NarrowVtable(), references, "messages<char>::messages");
}

void APS5_VABI _ZNSt8messagesIcEC1ERKSt8_Locinfom_nid_postfix(void* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    ConstructFrom(self, locinfo, NarrowVtable(), references, "messages<char>::messages");
}

void APS5_VABI _ZNSt8messagesIcEC2ERKSt8_Locinfom_nid_postfix(void* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    ConstructFrom(self, locinfo, NarrowVtable(), references, "messages<char>::messages");
}

void APS5_VABI _ZNSt8messagesIcEC2Em_nid_postfix(void* self, std::size_t references) {
    Construct(self, NarrowVtable(), references, "messages<char>::messages");
}

void APS5_VABI _ZNSt8messagesIwEC1EPKcm_nid_postfix(void* self, const char* localeName, std::size_t references) {
    ConstructNamed(self, localeName, &g_wideMessagesVtable.facet, references, "messages<wchar_t>::messages");
}

void APS5_VABI _ZNSt8messagesIwEC1ERKSt8_Locinfom_nid_postfix(void* self, const GuestLocale::LocinfoStorage* locinfo, std::size_t references) {
    ConstructFrom(self, locinfo, &g_wideMessagesVtable.facet, references, "messages<wchar_t>::messages");
}

void APS5_VABI _ZNSt8messagesIwEC2Em_nid_postfix(void* self, std::size_t references) {
    Construct(self, &g_wideMessagesVtable.facet, references, "messages<wchar_t>::messages");
}

}
