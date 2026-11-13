#include "prx/libSceRegMgr/include/InventoryExports.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"

namespace {

constexpr int ErrorUnknownKey = static_cast<int>(0x800d0105u);
constexpr int ErrorNullValue = static_cast<int>(0x800d0202u);
constexpr int ErrorSize = static_cast<int>(0x800d0208u);

bool KeyInRemovedRange(std::uint32_t key) {
    const std::uint32_t field = (key >> 18) & 0x3f;
    return (key & 0xff000000u) == 0x07000000u && (key & 0x07fc0000u) == 0x07800000u && field * 4 - 0x80 <= 0x13;
}

bool SizeInRange(std::uint64_t size) {
    return size - 0x800 >= 0xfffffffffffff801ull;
}

bool WriteZeros(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

} // namespace

extern "C" {

int APS5_VABI libSceRegMgrUnknown_JfNNpUQdhQw_nid_no_patch(std::uint32_t key) {
    return KeyInRemovedRange(key) ? 0 : ErrorUnknownKey;
}
APS5_EXPORT("JfNNpUQdhQw", libSceRegMgrUnknown_JfNNpUQdhQw_nid_no_patch);

int APS5_VABI libSceRegMgrUnknown_UuGXdpQj1Aw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("UuGXdpQj1Aw", libSceRegMgrUnknown_UuGXdpQj1Aw_nid_no_patch);

int APS5_VABI libSceRegMgrUnknown_YtmvqKL9PEo_nid_no_patch(std::uint32_t key, const void* value, std::uint64_t size) {
    if (!KeyInRemovedRange(key)) return ErrorUnknownKey;
    if (value == nullptr) return ErrorNullValue;
    return SizeInRange(size) ? 0 : ErrorSize;
}
APS5_EXPORT("YtmvqKL9PEo", libSceRegMgrUnknown_YtmvqKL9PEo_nid_no_patch);

int APS5_VABI libSceRegMgrUnknown_csy_minus_cEETOKg_nid_no_patch(std::uint32_t key, std::int32_t* value) {
    if (!KeyInRemovedRange(key)) return ErrorUnknownKey;
    if (value == nullptr) return ErrorNullValue;
    const std::int32_t zero = 0;
    if (!WriteZeros(value, &zero, sizeof(zero))) NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("csy-cEETOKg", libSceRegMgrUnknown_csy_minus_cEETOKg_nid_no_patch);

int APS5_VABI libSceRegMgrUnknown_iHi1Omn25M4_nid_no_patch(std::uint32_t key, char* value, std::uint64_t size) {
    if (!KeyInRemovedRange(key)) return ErrorUnknownKey;
    if (value == nullptr) return ErrorNullValue;
    if (!SizeInRange(size)) return ErrorSize;
    const char zero = 0;
    if (!WriteZeros(value, &zero, sizeof(zero))) NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("iHi1Omn25M4", libSceRegMgrUnknown_iHi1Omn25M4_nid_no_patch);

int APS5_VABI libSceRegMgrUnknown_wpJW0oIM6rg_nid_no_patch(std::uint32_t key, void* value, std::uint64_t size) {
    if (!KeyInRemovedRange(key)) return ErrorUnknownKey;
    if (value == nullptr) return ErrorNullValue;
    if (!SizeInRange(size)) return ErrorSize;
    const std::array<unsigned char, 0x7ff> zeros{};
    if (!WriteZeros(value, zeros.data(), static_cast<std::size_t>(size))) NotImplemented_nid_no_patch(__func__);
    return 0;
}
APS5_EXPORT("wpJW0oIM6rg", libSceRegMgrUnknown_wpJW0oIM6rg_nid_no_patch);

int APS5_VABI libSceRegMgrUnknown_x0YJQilTJDo_nid_no_patch(std::uint32_t key, const void* value, std::uint64_t size) {
    if (!KeyInRemovedRange(key)) return ErrorUnknownKey;
    if (value == nullptr) return ErrorNullValue;
    return SizeInRange(size) ? 0 : ErrorSize;
}
APS5_EXPORT("x0YJQilTJDo", libSceRegMgrUnknown_x0YJQilTJDo_nid_no_patch);

int APS5_VABI libSceRegMgrUnknown_x9WN_plus_tFTY34_nid_no_patch(std::uint32_t key) {
    return KeyInRemovedRange(key) ? 0 : ErrorUnknownKey;
}
APS5_EXPORT("x9WN+tFTY34", libSceRegMgrUnknown_x9WN_plus_tFTY34_nid_no_patch);

}
