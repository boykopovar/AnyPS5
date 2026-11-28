#include "prx/libScePlayReady2/include/Api.hpp"
#include <cerrno>
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int ErrorInvalidArgument = static_cast<int>(0x80070057u);
constexpr std::uint32_t InitParamsMagic = 0x50524459;

template <typename TValue>
bool ReadField(const void* base, std::size_t offset, TValue& value) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, static_cast<const unsigned char*>(base) + offset, sizeof(value));
    errno = savedErrno;
    return copied;
}

enum class Check { Pass, Fail, Unreadable };

template <typename TValue, typename TPredicate>
Check CheckField(const void* base, std::size_t offset, TPredicate accept) {
    TValue value{};
    if (!ReadField(base, offset, value)) return Check::Unreadable;
    return accept(value) ? Check::Pass : Check::Fail;
}

} // namespace

extern "C" {

int APS5_VABI scePlayReadyBufferInitWithData(void* buffer, const void* data, std::uint64_t size) {
    if (buffer == nullptr || (data == nullptr) != (size == 0)) return ErrorInvalidArgument;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyCloneDecryptContext() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDebugPrintf() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyDebugSetLevel() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseDelete() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseDeleteInMemory() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseGetExpirationTime() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyLicenseGetProperty() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderClose() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderDecryptMultiple() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderDecryptMultipleOpaque() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderDecryptOpaque() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderDecrypt_Legacy() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderFreeOpaqueContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReaderRetrieveOpaqueContent() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI scePlayReadyReinitialize() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}


int APS5_VABI scePlayReadyApiSetInitParamFlags(void* params, std::uint32_t flags) {
    if (params == nullptr) return ErrorInvalidArgument;
    const Check checks[]{
        CheckField<std::uint32_t>(params, 0x0, [](std::uint32_t v) { return v == InitParamsMagic; }),
        CheckField<std::uint16_t>(params, 0x4, [](std::uint16_t v) { return v == 3; }),
        CheckField<std::uint16_t>(params, 0x6, [](std::uint16_t v) { return v == 0x40; }),
        CheckField<std::uint32_t>(params, 0xC, [](std::uint32_t v) { return v == 0; }),
        CheckField<std::uint64_t>(params, 0x10, [](std::uint64_t v) { return v == 0; }),
        CheckField<std::uint64_t>(params, 0x18, [](std::uint64_t v) { return v != 0; }),
        CheckField<std::uint64_t>(params, 0x20, [](std::uint64_t v) { return v != 0; }),
        CheckField<std::uint64_t>(params, 0x28, [](std::uint64_t v) { return v != 0; }),
        CheckField<std::uint64_t>(params, 0x30, [](std::uint64_t v) { return v != 0; }),
    };
    for (const Check check : checks) {
        if (check == Check::Fail) return ErrorInvalidArgument;
        if (check == Check::Unreadable) NotImplemented_nid_no_patch(__func__);
    }
    const int savedErrno = errno;
    const bool written =
        GuestCopyOut_nid_no_patch(static_cast<unsigned char*>(params) + 0x8, &flags, sizeof(flags));
    errno = savedErrno;
    if (!written) NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
