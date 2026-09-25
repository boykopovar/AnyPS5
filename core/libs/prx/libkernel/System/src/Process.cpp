#include <cstdint>
#include <cstddef>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"
#include <cstring>
#include <mutex>
#include <random>

static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016);

extern "C" {

int APS5_VABI getargc_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

const char** APS5_VABI getargv_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI getpagesize_nid_postfix(void) {
    return 0x4000;
}

int APS5_VABI getpid_nid_postfix(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void APS5_VABI exit_nid_postfix(int code) {
 (void)code;
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sceKernelGetCurrentCpu(void) {
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

uint64_t APS5_VABI sceKernelGetGPI(void) {
    return 0;
}

void APS5_VABI sceKernelSetGPO(uint32_t bits) {
    (void)bits;
}

int APS5_VABI sceKernelGetOpenPsId(void* open_ps_id) {
 (void)open_ps_id;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

void* APS5_VABI sceKernelGetProcParam(void) {
 NotImplemented_nid_no_patch(__func__);
 return nullptr;
}

int APS5_VABI sceKernelUuidCreate(uint8_t* uuid) {
    if (!uuid) return SCE_KERNEL_ERROR_EINVAL;
    static std::mutex lock;
    static std::mt19937_64 generator{std::random_device{}()};
    std::lock_guard guard(lock);
    for (int index = 0; index < 16; index += 8) {
        const uint64_t bits = generator();
        std::memcpy(uuid + index, &bits, 8);
    }
    uuid[6] = static_cast<uint8_t>((uuid[6] & 0x0F) | 0x40);
    uuid[8] = static_cast<uint8_t>((uuid[8] & 0x3F) | 0x80);
    return 0;
}

void APS5_VABI sceKernelSync(void) {
 NotImplemented_nid_no_patch(__func__);
}

int APS5_VABI sched_get_priority_max_nid_postfix(int policy) {
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sched_get_priority_min_nid_postfix(int policy) {
 (void)policy;
 NotImplemented_nid_no_patch(__func__);
 return 0;
}

int APS5_VABI sceKernelIsTrinityMode(void) {
    return 0;
}

}
