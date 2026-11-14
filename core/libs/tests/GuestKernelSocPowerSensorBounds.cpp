#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "SceTypes.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "soc power sensor bounds check failed at %s:%u\n", location.file_name(), static_cast<unsigned>(location.line()));
        std::abort();
    }
}

void AuditSocPowerConsumption() {
    Require(sceKernelGetSocPowerConsumption(nullptr) == SCE_KERNEL_ERROR_EFAULT);

    std::array<std::uint8_t, 128> buffer{};
    buffer.fill(0xAA);
    Require(sceKernelGetSocPowerConsumption(buffer.data()) == SCE_KERNEL_ERROR_EINVAL);
    for (std::size_t i = 0; i < buffer.size(); ++i) {
        Require(buffer[i] == 0xAA);
    }

    std::array<std::uint8_t, 64> smallBuffer{};
    smallBuffer.fill(0x55);
    Require(sceKernelGetSocPowerConsumption(smallBuffer.data()) == SCE_KERNEL_ERROR_EINVAL);
    for (std::size_t i = 0; i < smallBuffer.size(); ++i) {
        Require(smallBuffer[i] == 0x55);
    }
}

void AuditSocSensorTemperatureBounds() {
    Require(sceKernelGetSocSensorTemperature(-1, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == 22);

    int temperature = 999;
    Require(sceKernelGetSocSensorTemperature(-100, &temperature) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == 22);
    Require(temperature == 999);

    Require(sceKernelGetSocSensorTemperature(65, nullptr) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == 22);

    Require(sceKernelGetSocSensorTemperature(65, &temperature) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == 22);
    Require(temperature == 999);

    Require(sceKernelGetSocSensorTemperature(1000, &temperature) == SCE_KERNEL_ERROR_EINVAL);
    Require(*__error_nid_postfix() == 22);
    Require(temperature == 999);

    Require(sceKernelGetSocSensorTemperature(64, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 14);

    Require(sceKernelGetSocSensorTemperature(0, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 14);

    Require(sceKernelGetSocSensorTemperature(1, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 14);

    Require(sceKernelGetSocSensorTemperature(2, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 14);

    int sensor64Out = 0;
    Require(sceKernelGetSocSensorTemperature(64, &sensor64Out) == 0);
    Require(sensor64Out == -49);

    for (int channel : {3, 10, 32, 63}) {
        bool caught = false;
        try {
            int out = 0;
            sceKernelGetSocSensorTemperature(channel, &out);
        } catch (const std::runtime_error&) {
            caught = true;
        }
        Require(caught);
    }

    for (int channel : {0, 1, 2}) {
        int out = -999;
        const int result = sceKernelGetSocSensorTemperature(channel, &out);
        Require(result == 0 || result == SCE_KERNEL_ERROR_EINVAL);
        if (result == 0) {
            Require(out >= 0 && out <= 130);
        }
    }
}

void AuditCpuTemperature() {
    Require(sceKernelGetCpuTemperature(nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(*__error_nid_postfix() == 14);

    int out = -999;
    const int result = sceKernelGetCpuTemperature(&out);
    Require(result == 0 || result == SCE_KERNEL_ERROR_EINVAL);
    if (result == 0) {
        Require(out >= 0 && out <= 130);
    }
}

} // namespace

int main() {
    AuditSocPowerConsumption();
    AuditSocSensorTemperatureBounds();
    AuditCpuTemperature();
    return 0;
}
