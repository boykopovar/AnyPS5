#include "prx/libSceFreeType/include/Api.hpp"
#include "prx/libSceFreeType/include/GuestObjects.hpp"

#include FT_MODULE_H

#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"

using FreeType::GuestLibrary;

namespace {

struct ErrnoGuard {
    int saved = errno;
    ~ErrnoGuard() { errno = saved; }
};

bool ReadGuestLibrary(GuestLibrary* library, FT_Library& host) {
    host = nullptr;
    if (!library) return true;
    const std::uintptr_t address = reinterpret_cast<std::uintptr_t>(library);
    if (address % alignof(GuestLibrary) != 0) return false;
    static const std::size_t hostOffset = [] {
        GuestLibrary sample{};
        return reinterpret_cast<std::uintptr_t>(&sample.host) - reinterpret_cast<std::uintptr_t>(&sample);
    }();
    if (hostOffset > std::numeric_limits<std::uintptr_t>::max() - address) return false;
    const std::uintptr_t hostAddress = address + hostOffset;
    if (sizeof(host) - 1 > std::numeric_limits<std::uintptr_t>::max() - hostAddress) return false;
    const void* hostPointer = reinterpret_cast<const void*>(hostAddress);
    const ErrnoGuard savedErrno;
    GuestArena::GuestArenaPrepareHostRead_nid_no_patch(hostPointer, sizeof(host));
    return GuestReadable_nid_no_patch(hostPointer, sizeof(host)) &&
        GuestCopyIn_nid_no_patch(&host, hostPointer, sizeof(host));
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
    const std::int32_t outMajor = static_cast<std::int32_t>(hostMajor);
    const std::int32_t outMinor = static_cast<std::int32_t>(hostMinor);
    const std::int32_t outPatch = static_cast<std::int32_t>(hostPatch);
    const ErrnoGuard savedErrno;
    GuestArena::HostWrite majorWrite(major, major ? sizeof(*major) : 0);
    GuestArena::HostWrite minorWrite(minor, minor ? sizeof(*minor) : 0);
    GuestArena::HostWrite patchWrite(patch, patch ? sizeof(*patch) : 0);
    if ((major && (!majorWrite.Open() || !GuestWritable_nid_no_patch(major, sizeof(*major)))) ||
        (minor && (!minorWrite.Open() || !GuestWritable_nid_no_patch(minor, sizeof(*minor)))) ||
        (patch && (!patchWrite.Open() || !GuestWritable_nid_no_patch(patch, sizeof(*patch))))) {
        throw std::invalid_argument("FT_Library_Version: unwritable output");
    }
    if ((major && !GuestCopyOut_nid_no_patch(major, &outMajor, sizeof(outMajor))) ||
        (minor && !GuestCopyOut_nid_no_patch(minor, &outMinor, sizeof(outMinor))) ||
        (patch && !GuestCopyOut_nid_no_patch(patch, &outPatch, sizeof(outPatch)))) {
        throw std::invalid_argument("FT_Library_Version: unwritable output");
    }
}

std::int32_t APS5_VABI FT_Get_TrueType_Engine_Type_nid_postfix(FreeType::GuestLibrary* library) {
    return static_cast<std::int32_t>(FT_Get_TrueType_Engine_Type(ReadGuestLibraryOrNotImplemented(library, __func__)));
}

}
