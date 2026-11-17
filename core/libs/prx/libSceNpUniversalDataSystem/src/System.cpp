#include "prx/libSceNpUniversalDataSystem/include/Api.hpp"
#include <cerrno>
#include <atomic>
#include <cinttypes>
#include <cstddef>
#include <cstdint>
#include "NpUniversalDataSystem.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

std::atomic<bool> g_systemInitialized{false};

}

bool Uds::SystemInitialized() {
    return g_systemInitialized.load();
}

extern "C" {

int APS5_VABI sceNpUniversalDataSystemInitialize(const NpUniversalDataSystemInitParam* param) {
    if (g_systemInitialized.load()) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_ALREADY_INITIALIZED;
    }
    if (param == nullptr) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    NpUniversalDataSystemInitParam copy{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&copy, param, sizeof(copy));
    errno = savedErrno;
    if (!copied) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    if (copy.size != sizeof(NpUniversalDataSystemInitParam)) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_PARAM_SIZE;
    }
    if (copy.pool_size == 0) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    }
    if (g_systemInitialized.exchange(true)) {
        return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_ALREADY_INITIALIZED;
    }
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemTerminate(void) {
    g_systemInitialized.store(false);
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemGetMemoryStat(NpUniversalDataSystemMemoryStat* stat) {
    if (stat == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    const NpUniversalDataSystemMemoryStat value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(stat, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

int APS5_VABI sceNpUniversalDataSystemGetStorageStat(int context, NpUniversalDataSystemStorageStat* stat) {
    if (stat == nullptr) {
        APS5_INVALID_ARG_EX;
    }
    const NpUniversalDataSystemStorageStat value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(stat, &value, sizeof(value));
    errno = savedErrno;
    if (!copied) return SCE_NP_UNIVERSAL_DATA_SYSTEM_ERROR_INVALID_ARGUMENT;
    return SCE_NP_UNIVERSAL_DATA_SYSTEM_OK;
}

}
