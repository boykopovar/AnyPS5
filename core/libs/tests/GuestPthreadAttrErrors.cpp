#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

extern "C" {
int APS5_VABI scePthreadAttrInit(PthreadAttr* attr);
int APS5_VABI scePthreadAttrDestroy(PthreadAttr* attr);
int APS5_VABI scePthreadAttrSetstacksize(PthreadAttr* attr, std::size_t stackSize);
int APS5_VABI scePthreadAttrGetstacksize(const PthreadAttr* attr, std::size_t* stackSize);
int APS5_VABI scePthreadAttrSetdetachstate(PthreadAttr* attr, int state);
int APS5_VABI scePthreadAttrGetdetachstate(const PthreadAttr* attr, int* state);
int APS5_VABI scePthreadAttrSetschedpolicy(PthreadAttr* attr, int policy);
int APS5_VABI scePthreadAttrGetschedpolicy(const PthreadAttr* attr, int* policy);
int APS5_VABI scePthreadAttrSetinheritsched(PthreadAttr* attr, int inheritSched);
int APS5_VABI scePthreadAttrGetinheritsched(const PthreadAttr* attr, int* inheritSched);
int APS5_VABI scePthreadAttrSetschedparam(PthreadAttr* attr, const KernelSchedParam* param);
int APS5_VABI scePthreadAttrGetschedparam(const PthreadAttr* attr, KernelSchedParam* param);
int APS5_VABI scePthreadAttrSetaffinity(PthreadAttr* attr, KernelCpumask mask);
int APS5_VABI scePthreadAttrGetaffinity(const PthreadAttr* attr, KernelCpumask* mask);
}

static constexpr int SCE_OK = 0;
static constexpr int SCE_KERNEL_ERROR_EINVAL = static_cast<int>(0x80020016u);
static constexpr int SCE_KERNEL_ERROR_EOPNOTSUPP = static_cast<int>(0x8002002Du);

static void Require(bool value) { if (!value) std::abort(); }

int main() {
    PthreadAttr attr = nullptr;
    Require(scePthreadAttrInit(&attr) == SCE_OK);
    int value = -1;
    Require(scePthreadAttrGetschedpolicy(&attr, &value) == SCE_OK && value == 1);
    Require(scePthreadAttrGetinheritsched(&attr, &value) == SCE_OK && value == 4);

    for (std::size_t size : {std::size_t{0}, std::size_t{1}, std::size_t{16383}})
        Require(scePthreadAttrSetstacksize(&attr, size) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrSetstacksize(&attr, 16384) == SCE_OK);

    for (int state : {-1, 2, 3, 100}) Require(scePthreadAttrSetdetachstate(&attr, state) == SCE_KERNEL_ERROR_EINVAL);
    for (int state : {0, 1}) Require(scePthreadAttrSetdetachstate(&attr, state) == SCE_OK);

    for (int policy : {-1, 0, 2, 4}) {
        Require(scePthreadAttrSetschedpolicy(&attr, policy) == SCE_KERNEL_ERROR_EOPNOTSUPP);
        Require(scePthreadAttrGetschedpolicy(&attr, &value) == SCE_OK && value == 1);
    }
    for (int policy : {3, 1}) {
        Require(scePthreadAttrSetschedpolicy(&attr, policy) == SCE_OK);
        Require(scePthreadAttrGetschedpolicy(&attr, &value) == SCE_OK && value == policy);
    }

    for (int inherit : {-1, 1, 2, 8}) {
        Require(scePthreadAttrSetinheritsched(&attr, inherit) == SCE_KERNEL_ERROR_EOPNOTSUPP);
        Require(scePthreadAttrGetinheritsched(&attr, &value) == SCE_OK && value == 4);
    }
    for (int inherit : {0, 4}) {
        Require(scePthreadAttrSetinheritsched(&attr, inherit) == SCE_OK);
        Require(scePthreadAttrGetinheritsched(&attr, &value) == SCE_OK && value == inherit);
    }

    KernelSchedParam param{};
    for (int priority : {-1, 0, 255, 768, 900, 960}) {
        param.sched_priority = priority;
        Require(scePthreadAttrSetschedparam(&attr, &param) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    }
    Require(scePthreadAttrSetschedparam(&attr, nullptr) == SCE_KERNEL_ERROR_EOPNOTSUPP);
    for (int priority : {256, 767}) {
        param.sched_priority = priority;
        Require(scePthreadAttrSetschedparam(&attr, &param) == SCE_OK);
        KernelSchedParam read{};
        Require(scePthreadAttrGetschedparam(&attr, &read) == SCE_OK && read.sched_priority == priority);
    }
    for (KernelCpumask mask : {KernelCpumask{0}, KernelCpumask{1}, ~KernelCpumask{0}})
        Require(scePthreadAttrSetaffinity(&attr, mask) == SCE_OK);

    std::size_t size = 0;
    int state = 0;
    KernelCpumask mask = 0;
    Require(scePthreadAttrGetstacksize(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetdetachstate(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetschedparam(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(scePthreadAttrGetschedpolicy(&attr, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    bool threw = false;
    try { scePthreadAttrGetinheritsched(&attr, nullptr); } catch (const std::runtime_error&) { threw = true; }
    Require(threw);

    for (PthreadAttr* missing : {static_cast<PthreadAttr*>(nullptr), static_cast<PthreadAttr*>(&attr)}) {
        PthreadAttr unset = nullptr;
        PthreadAttr* target = missing ? &unset : nullptr;
        Require(scePthreadAttrSetstacksize(target, 16384) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetdetachstate(target, 0) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetschedpolicy(target, 1) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetinheritsched(target, 0) == SCE_KERNEL_ERROR_EINVAL);
        param.sched_priority = 700;
        Require(scePthreadAttrSetschedparam(target, &param) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrSetaffinity(target, 1) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetstacksize(target, &size) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetdetachstate(target, &state) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetschedparam(target, &param) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetschedpolicy(target, &state) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetinheritsched(target, &state) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrGetaffinity(target, &mask) == SCE_KERNEL_ERROR_EINVAL);
        Require(scePthreadAttrDestroy(target) == SCE_KERNEL_ERROR_EINVAL);
    }

    Require(scePthreadAttrDestroy(&attr) == SCE_OK);
    Require(scePthreadAttrDestroy(&attr) == SCE_KERNEL_ERROR_EINVAL);
}
