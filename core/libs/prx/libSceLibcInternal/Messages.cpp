#include "prx/libSceLibcInternal/include/Api.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstddef>

namespace {

struct MessagesVtable {
    GuestLocale::FacetVtable facet;
    int (APS5_VABI *open)(const GuestLocale::Facet* self, const GuestLocale::String* name, const void* locale);
    GuestLocale::String* (APS5_VABI *get)(GuestLocale::String* result, const GuestLocale::Facet* self, int catalog, int set, int message, const GuestLocale::String* fallback);
    void (APS5_VABI *close)(const GuestLocale::Facet* self, int catalog);
};

static_assert(offsetof(MessagesVtable, open) == 0x20);
static_assert(offsetof(MessagesVtable, get) == 0x28);
static_assert(offsetof(MessagesVtable, close) == 0x30);
static_assert(sizeof(MessagesVtable) == 0x38);

const MessagesVtable& Messages(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const MessagesVtable*>(self->vtable);
}

}

extern "C" {

int APS5_VABI _ZNKSt8messagesIcE4openERKSsRKSt6locale_nid_postfix(const GuestLocale::Facet* self, const GuestLocale::String* name, const void* locale) {
    return Messages(self).open(self, name, locale);
}

GuestLocale::String* APS5_VABI _ZNKSt8messagesIcE3getEiiiRKSs_nid_postfix(GuestLocale::String* result, const GuestLocale::Facet* self, int catalog, int set, int message, const GuestLocale::String* fallback) {
    return Messages(self).get(result, self, catalog, set, message, fallback);
}

}
