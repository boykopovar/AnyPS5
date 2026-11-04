#include "prx/libSceLibcInternal/include/Api.hpp"
#include <stdexcept>
#include <string>

namespace {

const GuestLocale::MessagesVtable& WideMessagesVtableOf(const GuestLocale::Facet* self, const char* function) {
    if (self == nullptr || self->vtable == nullptr) throw std::invalid_argument(std::string(function) + ": null facet");
    return *reinterpret_cast<const GuestLocale::MessagesVtable*>(self->vtable);
}

}

extern "C" {

std::int32_t APS5_VABI _ZNKSt8messagesIwE4openERKSsRKSt6locale_nid_postfix(const GuestLocale::Facet* self, const GuestLocale::String* name, const GuestLocaleFacade* locale) {
    return WideMessagesVtableOf(self, "messages<wchar_t>::open").open(self, name, locale);
}

GuestLocale::String* APS5_VABI _ZNKSt8messagesIwE3getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(GuestLocale::String* result, const GuestLocale::Facet* self, std::int32_t catalog, std::int32_t set, std::int32_t message, const GuestLocale::String* dfault) {
    return WideMessagesVtableOf(self, "messages<wchar_t>::get").get(result, self, catalog, set, message, dfault);
}

void APS5_VABI _ZNKSt8messagesIwE5closeEi_nid_postfix(const GuestLocale::Facet* self, std::int32_t catalog) {
    WideMessagesVtableOf(self, "messages<wchar_t>::close").close(self, catalog);
}

GuestLocale::String* APS5_VABI _ZNKSt8messagesIwE6do_getEiiiRKSbIwSt11char_traitsIwESaIwEE_nid_postfix(GuestLocale::String* result, const GuestLocale::Facet*, std::int32_t, std::int32_t, std::int32_t, const GuestLocale::String* dfault) {
    if (dfault == nullptr) throw std::invalid_argument("messages<wchar_t>::do_get: null default message");
    return GuestLocale::AssignWideString(result, GuestLocale::WideStringData(*dfault), dfault->size);
}

}
