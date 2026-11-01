#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libkernel_sys/include/Api.hpp"
#include "prx/libkernel/include/Api.hpp"
#include "SceTypes.hpp"
#include <climits>
#include <cstdint>
#include <cstdlib>

static void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

int main() {
    PthreadBarrierPrivate* barrier = nullptr;
    PthreadBarrierattrPrivate* attr = nullptr;

    Require(scePthreadBarrierInit(&barrier, &attr, static_cast<unsigned>(INT_MAX) + 1u, "overflow") == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    Require(scePthreadBarrierInit(&barrier, &attr, static_cast<unsigned>(INT_MIN), "intmin") == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    Require(scePthreadBarrierInit(&barrier, nullptr, static_cast<unsigned>(INT_MAX) + 1u, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    Require(scePthreadBarrierInit(&barrier, nullptr, static_cast<unsigned>(INT_MIN), nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    Require(scePthreadBarrierInit(&barrier, &attr, 0x80000000u, "intmin_hex") == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    Require(scePthreadBarrierInit(&barrier, &attr, 0xffffffffu, "max_uint") == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    Require(scePthreadBarrierInit(&barrier, nullptr, 0x80000000u, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    Require(scePthreadBarrierInit(&barrier, nullptr, 0xffffffffu, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(barrier == nullptr);

    return 0;
}
