#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceCommonDialog/CommonDialog.hpp"
#include "prx/libSceCommonDialog/include/Exports.hpp"
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace {

constexpr int kResult80B8000E = static_cast<int>(0x80B8000Eu);

bool HasClientImplementation(const void* client) {
    std::uint64_t implementation = 0;
    if (client == nullptr || !GuestCopyIn_nid_no_patch(&implementation, static_cast<const std::byte*>(client) + 8, sizeof(implementation))) {
        throw std::invalid_argument("CommonDialogUtil client: unreadable client");
    }
    return implementation != 0;
}

} // namespace

extern "C" {

int APS5_VABI libSceCommonDialogUnknown_8q7icGBWIrA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("8q7icGBWIrA", libSceCommonDialogUnknown_8q7icGBWIrA_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_D_minus_V35OhFeIM_nid_no_patch() {
    return 0;
}
APS5_EXPORT("D-V35OhFeIM", libSceCommonDialogUnknown_D_minus_V35OhFeIM_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_QXFsLON5QWw_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("QXFsLON5QWw", libSceCommonDialogUnknown_QXFsLON5QWw_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_SDpCfY9uB0g_nid_no_patch(const void* self) {
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("SDpCfY9uB0g", libSceCommonDialogUnknown_SDpCfY9uB0g_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_Zaa6fhiKQDE_nid_no_patch(const void* self) {
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("Zaa6fhiKQDE", libSceCommonDialogUnknown_Zaa6fhiKQDE_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_aUS4PgJye98_nid_no_patch(const void* self) {
    if (!HasClientImplementation(self)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("aUS4PgJye98", libSceCommonDialogUnknown_aUS4PgJye98_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch(const void* param) {
    const auto address = reinterpret_cast<std::uintptr_t>(param);
    std::uint32_t magic = 0;
    if (param == nullptr || !GuestCopyIn_nid_no_patch(&magic, static_cast<const std::byte*>(param) + offsetof(CommonDialogBaseParam, magic), sizeof(magic))) {
        throw std::invalid_argument("afLdI6i0lQw: unreadable parameter");
    }
    if (magic != static_cast<std::uint32_t>(address) - 1060003575u) {
        return COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
    }
    CommonDialogBaseParam local{};
    if (!GuestCopyIn_nid_no_patch(&local, param, sizeof(local))) {
        throw std::invalid_argument("afLdI6i0lQw: unreadable parameter");
    }
    return ValidateCommonDialogBaseParam(local, address) ? COMMON_DIALOG_OK : COMMON_DIALOG_ERROR_INVALID_ARGUMENT;
}
APS5_EXPORT("afLdI6i0lQw", libSceCommonDialogUnknown_afLdI6i0lQw_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_mVRnPerBcK0_nid_no_patch(const void* self) {
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("mVRnPerBcK0", libSceCommonDialogUnknown_mVRnPerBcK0_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_p9TTq4bLdFU_nid_no_patch() {
    return 0;
}
APS5_EXPORT("p9TTq4bLdFU", libSceCommonDialogUnknown_p9TTq4bLdFU_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_q3_minus_8WWCG2N0_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("q3-8WWCG2N0", libSceCommonDialogUnknown_q3_minus_8WWCG2N0_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_txNJzxX6yrA_nid_no_patch() {
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("txNJzxX6yrA", libSceCommonDialogUnknown_txNJzxX6yrA_nid_no_patch);

int APS5_VABI libSceCommonDialogUnknown_yxjgDvqUbGQ_nid_no_patch(const void* self) {
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
}
APS5_EXPORT("yxjgDvqUbGQ", libSceCommonDialogUnknown_yxjgDvqUbGQ_nid_no_patch);

}
