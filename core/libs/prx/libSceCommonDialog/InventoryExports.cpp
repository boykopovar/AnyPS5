#include "prx/libSceCommonDialog/include/Exports.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <limits>
#include <stdexcept>

namespace {

constexpr int kResult80B8000E = static_cast<int>(0x80B8000Eu);

bool HasClientImplementation(const void* client) {
    std::uint64_t implementation = 0;
    const int savedErrno = errno;
    const bool copied = client != nullptr &&
        reinterpret_cast<std::uintptr_t>(client) <= std::numeric_limits<std::uintptr_t>::max() - 8 &&
        GuestCopyIn_nid_no_patch(&implementation, static_cast<const std::byte*>(client) + 8, sizeof(implementation));
    errno = savedErrno;
    if (!copied) {
        throw std::invalid_argument("CommonDialogUtil client: unreadable client");
    }
    return implementation != 0;
}

} // namespace

extern "C" {

int APS5_VABI _ZN3sce16CommonDialogUtil12getSelfAppIdEv() {
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6Client11closeModuleEv(void* self) {
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6Client11updateStateEv(void* self) {
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZN3sce16CommonDialogUtil6Client15launchCmnDialogEv(void* self) {
    if (!HasClientImplementation(self)) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

void APS5_VABI _ZN3sce16CommonDialogUtil6ClientD0Ev(void* self) {
    (void)self;
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce16CommonDialogUtil6ClientD1Ev(void* self) {
    if (!HasClientImplementation(self)) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI _ZN3sce16CommonDialogUtil6ClientD2Ev(void* self) {
    if (!HasClientImplementation(self)) {
        return;
    }
    NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Client10isCloseReqEv(const void* self) {
    if (!HasClientImplementation(self)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Client13getFinishDataEPvm(const void* self, void* data, std::uint64_t size) {
    (void)data;
    (void)size;
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Client14getClientStateEv(const void* self) {
    if (!HasClientImplementation(self)) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Client19isInitializedStatusEv(const void* self) {
    if (!HasClientImplementation(self)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Client8getAppIdEv(const void* self) {
    if (!HasClientImplementation(self)) {
        return -1;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Client8isFinishEv(const void* self) {
    if (!HasClientImplementation(self)) {
        return 0;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

int APS5_VABI _ZNK3sce16CommonDialogUtil6Client9getResultEv(const void* self) {
    if (!HasClientImplementation(self)) {
        return kResult80B8000E;
    }
    NotImplemented_nid_no_patch(__func__);
    return 0;
}

}
