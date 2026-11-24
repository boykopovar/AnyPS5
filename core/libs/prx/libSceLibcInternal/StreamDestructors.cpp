#include "prx/libSceLibcInternal/include/StreamDestructors.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <stdexcept>
#include <string>

namespace {

constexpr std::ptrdiff_t VirtualOffsetSlot = -24;

char* AddVirtualOffset(void* object, const char* name) {
    if (object == nullptr) throw std::invalid_argument(std::string(name) + ": null object");
    const char* vtable = nullptr;
    if (!GuestCopyIn_nid_no_patch(&vtable, object, sizeof(vtable))) throw std::invalid_argument(std::string(name) + ": unreadable object");
    if (vtable == nullptr) throw std::invalid_argument(std::string(name) + ": null vtable");
    std::ptrdiff_t offset = 0;
    if (!GuestCopyIn_nid_no_patch(&offset, vtable + VirtualOffsetSlot, sizeof(offset))) throw std::invalid_argument(std::string(name) + ": unreadable vtable");
    return static_cast<char*>(object) + offset;
}

void DestroyStream(void* stream, const char* name) {
    _ZNSt8ios_baseD2Ev_nid_postfix(reinterpret_cast<GuestLocale::IosBase*>(AddVirtualOffset(stream, name)));
}

void DeleteStream(void* stream, const char* name) {
    DestroyStream(stream, name);
    _ZdlPv_nid_postfix(stream);
}

}

extern "C" {

void APS5_VABI sceLibcInternalIstreamCompleteDestructor_nid_no_patch(void* self) {
    DestroyStream(self, "basic_istream<char> destructor");
}
APS5_EXPORT("tJU-ttrsXsk", sceLibcInternalIstreamCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalIstreamDeletingDestructor_nid_no_patch(void* self) {
    DeleteStream(self, "basic_istream<char> deleting destructor");
}
APS5_EXPORT("6ZDv6ZusiFg", sceLibcInternalIstreamDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalIstreamCompleteDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalIstreamCompleteDestructor_nid_no_patch(AddVirtualOffset(self, "basic_istream<char> destructor thunk"));
}
APS5_EXPORT("+Uuj++A+I14", sceLibcInternalIstreamCompleteDestructorThunk_nid_no_patch);

void APS5_VABI sceLibcInternalIstreamDeletingDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalIstreamDeletingDestructor_nid_no_patch(AddVirtualOffset(self, "basic_istream<char> deleting destructor thunk"));
}
APS5_EXPORT("0bGGr4zLE3w", sceLibcInternalIstreamDeletingDestructorThunk_nid_no_patch);

void APS5_VABI sceLibcInternalOstreamCompleteDestructor_nid_no_patch(void* self) {
    DestroyStream(self, "basic_ostream<char> destructor");
}
APS5_EXPORT("HpCeP12cuNY", sceLibcInternalOstreamCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalOstreamDeletingDestructor_nid_no_patch(void* self) {
    DeleteStream(self, "basic_ostream<char> deleting destructor");
}
APS5_EXPORT("lTTrDj5OIwQ", sceLibcInternalOstreamDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalOstreamCompleteDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalOstreamCompleteDestructor_nid_no_patch(AddVirtualOffset(self, "basic_ostream<char> destructor thunk"));
}
APS5_EXPORT("kvqg376KsJo", sceLibcInternalOstreamCompleteDestructorThunk_nid_no_patch);

void APS5_VABI sceLibcInternalOstreamDeletingDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalOstreamDeletingDestructor_nid_no_patch(AddVirtualOffset(self, "basic_ostream<char> deleting destructor thunk"));
}
APS5_EXPORT("QJJ-4Dgm8YQ", sceLibcInternalOstreamDeletingDestructorThunk_nid_no_patch);

void APS5_VABI sceLibcInternalWideIstreamCompleteDestructor_nid_no_patch(void* self) {
    DestroyStream(self, "basic_istream<wchar_t> destructor");
}
APS5_EXPORT("RP7ijkGGx50", sceLibcInternalWideIstreamCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalWideIstreamDeletingDestructor_nid_no_patch(void* self) {
    DeleteStream(self, "basic_istream<wchar_t> deleting destructor");
}
APS5_EXPORT("StJaKYTRdUE", sceLibcInternalWideIstreamDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalWideIstreamCompleteDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalWideIstreamCompleteDestructor_nid_no_patch(AddVirtualOffset(self, "basic_istream<wchar_t> destructor thunk"));
}
APS5_EXPORT("q05IXuNA2NE", sceLibcInternalWideIstreamCompleteDestructorThunk_nid_no_patch);

void APS5_VABI sceLibcInternalWideIstreamDeletingDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalWideIstreamDeletingDestructor_nid_no_patch(AddVirtualOffset(self, "basic_istream<wchar_t> deleting destructor thunk"));
}
APS5_EXPORT("izmoTISVoF8", sceLibcInternalWideIstreamDeletingDestructorThunk_nid_no_patch);

void APS5_VABI sceLibcInternalWideOstreamCompleteDestructor_nid_no_patch(void* self) {
    DestroyStream(self, "basic_ostream<wchar_t> destructor");
}
APS5_EXPORT("EYZJsnX58DE", sceLibcInternalWideOstreamCompleteDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalWideOstreamDeletingDestructor_nid_no_patch(void* self) {
    DeleteStream(self, "basic_ostream<wchar_t> deleting destructor");
}
APS5_EXPORT("7VRfkz22vPk", sceLibcInternalWideOstreamDeletingDestructor_nid_no_patch);

void APS5_VABI sceLibcInternalWideOstreamCompleteDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalWideOstreamCompleteDestructor_nid_no_patch(AddVirtualOffset(self, "basic_ostream<wchar_t> destructor thunk"));
}
APS5_EXPORT("HSkPyRyFFHQ", sceLibcInternalWideOstreamCompleteDestructorThunk_nid_no_patch);

void APS5_VABI sceLibcInternalWideOstreamDeletingDestructorThunk_nid_no_patch(void* self) {
    sceLibcInternalWideOstreamDeletingDestructor_nid_no_patch(AddVirtualOffset(self, "basic_ostream<wchar_t> deleting destructor thunk"));
}
APS5_EXPORT("0j1jspKbuFk", sceLibcInternalWideOstreamDeletingDestructorThunk_nid_no_patch);

}
