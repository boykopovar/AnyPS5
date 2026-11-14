#include "prx/libSceLibcInternal/include/CodecvtUtfMetadata.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace {

constexpr std::uintptr_t ModeOffset = 0x18;
constexpr unsigned GenerateHeader = 2;
constexpr unsigned ConsumeHeader = 4;

template<typename TValue>
TValue ReadMode_nid_no_patch(const void* self) {
    const auto address = reinterpret_cast<std::uintptr_t>(self);
    if (address > std::numeric_limits<std::uintptr_t>::max() - ModeOffset)
        throw std::invalid_argument("UTF codecvt: invalid facet address");
    TValue mode = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&mode, reinterpret_cast<const void*>(address + ModeOffset), sizeof(mode));
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("UTF codecvt: unreadable facet mode");
    return mode;
}

void WriteNext_nid_no_patch(char** next, char* first) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(next, &first, sizeof(first));
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("UTF codecvt: unwritable next pointer");
}

}

extern "C" {

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(const void*, void*, char* first, char*, char** next) {
    WriteNext_nid_no_patch(next, first);
    return 0;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE11do_encodingEv_nid_postfix(const void* self) {
    return (ReadMode_nid_no_patch<std::uint8_t>(self) & (GenerateHeader | ConsumeHeader)) != 0 ? -1 : 0;
}

int APS5_VABI _ZNKSt7codecvtIDic9_MbstatetE13do_max_lengthEv_nid_postfix(const void* self) {
    return (ReadMode_nid_no_patch<std::uint8_t>(self) & (GenerateHeader | ConsumeHeader)) != 0 ? 9 : 6;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE10do_unshiftERS0_PcS3_RS3__nid_postfix(const void*, void* state, char* first, char*, char** next) {
    WriteNext_nid_no_patch(next, first);
    std::uint16_t pending = 0;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&pending, state, sizeof(pending));
    errno = savedErrno;
    if (!copied)
        throw std::invalid_argument("UTF codecvt: unreadable shift state");
    return pending > 1 ? 2 : 0;
}

int APS5_VABI _ZNKSt7codecvtIDsc9_MbstatetE13do_max_lengthEv_nid_postfix(const void* self) {
    const auto mode = ReadMode_nid_no_patch<std::uint32_t>(self);
    return (mode & ConsumeHeader) != 0 ? 9 : (mode & GenerateHeader) != 0 ? 7 : 6;
}

}
