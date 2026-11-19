#include "prx/libc/include/GuestLocale.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <cstdint>

namespace {

const GuestLocale::TimeGetVtable& TimeGet(const GuestLocale::Facet* self) {
    return *reinterpret_cast<const GuestLocale::TimeGetVtable*>(self->vtable);
}

}

extern "C" {

std::int32_t APS5_VABI sceLibcInternalTimeGetDateOrder_nid_no_patch(const GuestLocale::Facet* self) {
    return TimeGet(self).dateOrder(self);
}
APS5_EXPORT("ShlQcYrzRF8", sceLibcInternalTimeGetDateOrder_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalTimeGetTime_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getTime(self, first, last, stream, state, time);
}
APS5_EXPORT("rrqNi95bhMs", sceLibcInternalTimeGetTime_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalTimeGetDate_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getDate(self, first, last, stream, state, time);
}
APS5_EXPORT("OY5mqEBxP+8", sceLibcInternalTimeGetDate_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalTimeGetWeekday_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getWeekday(self, first, last, stream, state, time);
}
APS5_EXPORT("KwJ5V3D0v3A", sceLibcInternalTimeGetWeekday_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalTimeGetMonthname_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getMonthname(self, first, last, stream, state, time);
}
APS5_EXPORT("vvA7HtdtWnY", sceLibcInternalTimeGetMonthname_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalTimeGetYear_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getYear(self, first, last, stream, state, time);
}
APS5_EXPORT("5L5Aft+9nZU", sceLibcInternalTimeGetYear_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalTimeGet_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char format, char modifier) {
    return TimeGet(self).get(self, first, last, stream, state, time, format, modifier);
}
APS5_EXPORT("+RuThw5axA4", sceLibcInternalTimeGet_nid_no_patch);

std::int32_t APS5_VABI sceLibcInternalWideTimeGetDateOrder_nid_no_patch(const GuestLocale::Facet* self) {
    return TimeGet(self).dateOrder(self);
}
APS5_EXPORT("nc6OsiDx630", sceLibcInternalWideTimeGetDateOrder_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideTimeGetTime_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getTime(self, first, last, stream, state, time);
}
APS5_EXPORT("9TfGnN6xq-U", sceLibcInternalWideTimeGetTime_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideTimeGetDate_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getDate(self, first, last, stream, state, time);
}
APS5_EXPORT("kwp-0uidHpw", sceLibcInternalWideTimeGetDate_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideTimeGetWeekday_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getWeekday(self, first, last, stream, state, time);
}
APS5_EXPORT("A0PftWMfrhk", sceLibcInternalWideTimeGetWeekday_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideTimeGetMonthname_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getMonthname(self, first, last, stream, state, time);
}
APS5_EXPORT("xy0MR+OOZI8", sceLibcInternalWideTimeGetMonthname_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideTimeGetYear_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time) {
    return TimeGet(self).getYear(self, first, last, stream, state, time);
}
APS5_EXPORT("Krt-A7EnHHs", sceLibcInternalWideTimeGetYear_nid_no_patch);

GuestLocale::InputIterator APS5_VABI sceLibcInternalWideTimeGet_nid_no_patch(const GuestLocale::Facet* self, GuestLocale::InputIterator first, GuestLocale::InputIterator last, GuestLocale::IosBase* stream, std::int32_t* state, GuestTm* time, char format, char modifier) {
    return TimeGet(self).get(self, first, last, stream, state, time, format, modifier);
}
APS5_EXPORT("64pqofAwJEg", sceLibcInternalWideTimeGet_nid_no_patch);

}
