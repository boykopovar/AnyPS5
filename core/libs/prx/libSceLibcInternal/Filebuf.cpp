#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <limits>

namespace {

void UnlockFilebuf_nid_no_patch(void* self) {
    constexpr std::uintptr_t FileOffset = 0xa8;
    const auto address = reinterpret_cast<std::uintptr_t>(self);
    if (address > std::numeric_limits<std::uintptr_t>::max() - FileOffset)
        NotImplemented_nid_no_patch(__func__);
    FileStream* stream = nullptr;
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&stream, reinterpret_cast<const void*>(address + FileOffset), sizeof(stream));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(__func__);
    if (stream != nullptr) _Unlockfilelock_nid_postfix(stream);
}

}

extern "C" {

void APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE7_UnlockEv_nid_postfix(void* self) {
    UnlockFilebuf_nid_no_patch(self);
}

void APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE7_UnlockEv_nid_postfix(void* self) {
    UnlockFilebuf_nid_no_patch(self);
}

}
