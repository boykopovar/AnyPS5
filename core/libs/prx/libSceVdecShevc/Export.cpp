#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceVdecShevc/include/InventoryExports.hpp"
#include "prx/libSceVdecShevc/include/Api.hpp"
#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>

namespace {

constexpr int SCE_SDEC_SW_HEVC_NO_DECODER = 1;
constexpr int SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM = 3;

std::uint64_t ReadQuad(const void* source, std::size_t offset, const char* function) {
    std::uint64_t value = 0;
    if (!GuestCopyIn_nid_no_patch(&value, static_cast<const unsigned char*>(source) + offset, sizeof(value))) {
        NotImplemented_nid_no_patch(function);
    }
    return value;
}

bool HasNoDecoder(const void* handle, const char* function) {
    return handle == nullptr || ReadQuad(handle, 0, function) == 0;
}

} // namespace

extern "C" {

int APS5_VABI sceSdecCreateSwHevc(const void* param) {
    std::uint32_t first = 0;
    if (!GuestCopyIn_nid_no_patch(&first, param, sizeof(first))) NotImplemented_nid_no_patch(__func__);
    if (first != 1) return SCE_SDEC_SW_HEVC_UNSUPPORTED_PARAM;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecDestroySwHevc(void* handle) {
    if (handle == nullptr) return 0;
    if (ReadQuad(handle, 0x10, __func__) != 0) NotImplemented_nid_no_patch(__func__);
    if (ReadQuad(handle, 0x8, __func__) != 0) {
        const std::uint64_t cleared = 0;
        auto* field = static_cast<unsigned char*>(handle) + 0x8;
        if (!GuestCopyOut_nid_no_patch(field, &cleared, sizeof(cleared))) NotImplemented_nid_no_patch(__func__);
    }
    return 0;
}

int APS5_VABI sceSdecExecuteSwHevc(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecGetFrameSwHevc(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI sceSdecGetVersionSwHevc(std::uint32_t* version) {
    if (version == nullptr) return;
    const std::uint32_t value = 0x13590001u;
    const auto preserved = errno;
    GuestCopyOut_nid_no_patch(version, &value, sizeof(value));
    errno = preserved;
}

int APS5_VABI sceSdecQueryMemorySwHevc() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI sceSdecResetSwHevc(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}

extern "C" {

int APS5_VABI libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("79qqGAVnWfA", libSceVdecShevcUnknown_79qqGAVnWfA_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("PTO17yTeoSk", libSceVdecShevcUnknown_PTO17yTeoSk_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("hZXstu74B4I", libSceVdecShevcUnknown_hZXstu74B4I_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("lLaAHswK1zQ", libSceVdecShevcUnknown_lLaAHswK1zQ_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("wvq+GPpswaw", libSceVdecShevcUnknown_wvq_plus_GPpswaw_nid_no_patch);

int APS5_VABI libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch(void* handle) {
    if (HasNoDecoder(handle, __func__)) return SCE_SDEC_SW_HEVC_NO_DECODER;
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("xbRElcEVmdM", libSceVdecShevcUnknown_xbRElcEVmdM_nid_no_patch);

}
