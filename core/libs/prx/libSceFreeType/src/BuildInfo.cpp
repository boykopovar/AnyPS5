#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_MODULE_H

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

using FreeType::GuestLibrary;

namespace {

bool CopyInPreservingErrno(void* destination, const void* source, std::size_t bytes) {
    const int saved = errno;
    const bool ok = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = saved;
    return ok;
}

bool ReadGuestLibrary(GuestLibrary* library, FT_Library& host) {
    host = nullptr;
    return !library || (reinterpret_cast<std::uintptr_t>(library) % alignof(GuestLibrary) == 0 && CopyInPreservingErrno(&host, &library->host, sizeof(host)));
}

FT_Library ReadGuestLibraryVersion(GuestLibrary* library) {
    FT_Library host = nullptr;
    if (!ReadGuestLibrary(library, host)) throw std::invalid_argument("FT_Library_Version: unreadable library");
    return host;
}

FT_Library ReadGuestLibraryOrNotImplemented(GuestLibrary* library, const char* func) {
    FT_Library host = nullptr;
    if (!ReadGuestLibrary(library, host)) NotImplemented_nid_no_patch(func);
    return host;
}

}

extern "C" {

void APS5_VABI FT_Library_Version_nid_postfix(FreeType::GuestLibrary* library, std::int32_t* major, std::int32_t* minor, std::int32_t* patch) {
    FT_Int hostMajor = 0;
    FT_Int hostMinor = 0;
    FT_Int hostPatch = 0;
    FT_Library_Version(ReadGuestLibraryVersion(library), &hostMajor, &hostMinor, &hostPatch);
    GuestArena::HostWrite majorWrite(major, major ? sizeof(*major) : 0);
    GuestArena::HostWrite minorWrite(minor, minor ? sizeof(*minor) : 0);
    GuestArena::HostWrite patchWrite(patch, patch ? sizeof(*patch) : 0);
    if ((major && (!majorWrite.Open() || !GuestWritable_nid_no_patch(major, sizeof(*major)))) ||
        (minor && (!minorWrite.Open() || !GuestWritable_nid_no_patch(minor, sizeof(*minor)))) ||
        (patch && (!patchWrite.Open() || !GuestWritable_nid_no_patch(patch, sizeof(*patch))))) {
        throw std::invalid_argument("FT_Library_Version: unwritable output");
    }
    if (major) *major = hostMajor;
    if (minor) *minor = hostMinor;
    if (patch) *patch = hostPatch;
}

std::int32_t APS5_VABI FT_Get_TrueType_Engine_Type_nid_postfix(FreeType::GuestLibrary* library) {
    return static_cast<std::int32_t>(FT_Get_TrueType_Engine_Type(ReadGuestLibraryOrNotImplemented(library, __func__)));
}

}
