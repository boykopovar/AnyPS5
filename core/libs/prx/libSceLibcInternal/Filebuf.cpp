#include "prx/libSceLibcInternal/include/InventoryExports.hpp"
#include "prx/libSceLibcInternal/include/InventoryExports6.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestLocale.hpp"
#include <cstdint>
#include <limits>

namespace {

template<typename T>
bool GuestCopyInErrno_nid_no_patch(T* dest, const void* src) {
    const bool copied = GuestCopyIn_nid_no_patch(dest, src, sizeof(T));
    return copied;
}

FileStream* FilebufStream_nid_no_patch(void* self) {
    constexpr std::uintptr_t FileOffset = 0xa8;
    const auto address = reinterpret_cast<std::uintptr_t>(self);
    if (address > std::numeric_limits<std::uintptr_t>::max() - FileOffset)
        NotImplemented_nid_no_patch(__func__);
    FileStream* stream = nullptr;
    if (!GuestCopyInErrno_nid_no_patch(&stream, reinterpret_cast<const void*>(address + FileOffset)))
        NotImplemented_nid_no_patch(__func__);
    return stream;
}

}

extern "C" {

void APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE5_LockEv_nid_postfix(void* self) {
    if (auto* stream = FilebufStream_nid_no_patch(self)) _Lockfilelock_nid_postfix(stream);
}

void APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE5_LockEv_nid_postfix(void* self) {
    if (auto* stream = FilebufStream_nid_no_patch(self)) _Lockfilelock_nid_postfix(stream);
}

void APS5_VABI _ZNSt13basic_filebufIcSt11char_traitsIcEE7_UnlockEv_nid_postfix(void* self) {
    if (auto* stream = FilebufStream_nid_no_patch(self)) _Unlockfilelock_nid_postfix(stream);
}

void APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE7_UnlockEv_nid_postfix(void* self) {
    if (auto* stream = FilebufStream_nid_no_patch(self)) _Unlockfilelock_nid_postfix(stream);
}

int APS5_VABI _ZNSt13basic_filebufIwSt11char_traitsIwEE4syncEv_nid_postfix(void* self) {
    FileStream* stream = FilebufStream_nid_no_patch(self);
    if (!stream)
        return 0;

    const GuestLocale::StreambufVtable* vtable = nullptr;
    if (!GuestCopyInErrno_nid_no_patch(&vtable, &reinterpret_cast<const GuestLocale::Streambuf*>(self)->vtable) || !vtable)
        NotImplemented_nid_no_patch(__func__);

    int (APS5_VABI *overflow)(GuestLocale::Streambuf*, int) = nullptr;
    if (!GuestCopyInErrno_nid_no_patch(&overflow, &vtable->overflow) || !overflow)
        NotImplemented_nid_no_patch(__func__);

    if (overflow(reinterpret_cast<GuestLocale::Streambuf*>(self), -1) == -1)
        return 0;

    return fflush_nid_postfix(stream) == 0 ? 0 : -1;
}

}
