#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <cstddef>
#include <cstdlib>
#include <initializer_list>
#include <stdexcept>

static constexpr int SCE_OK = 0;

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
