#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <array>
#include <cerrno>
#include <cstring>
#include <stdexcept>

extern "C" void JxrMetadataCleanup_nid_no_patch(std::int32_t type, void* payload);

extern "C" long JxrMetadataFree_nid_no_patch(void** memory) {
    return static_cast<long>(PKFree_nid_postfix(memory));
}

extern "C" void APS5_VABI FreeDescMetadata_nid_postfix(Jxr::MetadataVariant* value) {
    if (!value) throw std::invalid_argument("FreeDescMetadata requires a metadata record");
    Jxr::MetadataVariant guest{};
    int savedErrno = errno;
    const bool copiedIn = GuestCopyIn_nid_no_patch(&guest, value, sizeof(guest));
    errno = savedErrno;
    if (!copiedIn) throw std::invalid_argument("FreeDescMetadata requires a readable metadata record");
    const auto type = guest.type;
    if (type != 0 && type != 18 && type != 19 && type != 30 && type != 31)
        throw std::invalid_argument("FreeDescMetadata: unsupported metadata tag");
    std::array<std::byte, sizeof(guest.value)> payload;
    std::memcpy(payload.data(), &guest.value, payload.size());
    if (type == 30 || type == 31) {
        savedErrno = errno;
        const bool writable = GuestCopyOut_nid_no_patch(&value->value, payload.data(), payload.size());
        errno = savedErrno;
        if (!writable) throw std::invalid_argument("FreeDescMetadata requires a writable metadata value");
    }
    JxrMetadataCleanup_nid_no_patch(type, payload.data());
    if (type == 30 || type == 31) {
        savedErrno = errno;
        const bool copiedOut = GuestCopyOut_nid_no_patch(&value->value, payload.data(), payload.size());
        errno = savedErrno;
        if (!copiedOut) throw std::invalid_argument("FreeDescMetadata requires a writable metadata value");
    }
}
