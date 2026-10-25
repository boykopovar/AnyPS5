#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

extern "C" {
void ReadThermalSensors_nid_no_patch(const char* root, int celsius[3]);

}

static void Require(bool value) {
    if (!value) {
        std::fprintf(stderr, "thermal sensors check failed\n");
        std::abort();
    }
}

int main() {
    const auto root = std::filesystem::path("anyps5-thermal-sensors-test");
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "hwmon" / "hwmon0");
    std::filesystem::create_directories(root / "hwmon" / "hwmon1");
    std::filesystem::create_directories(root / "thermal" / "thermal_zone0");
    std::ofstream(root / "hwmon" / "hwmon0" / "name") << "k10temp\n";
    std::ofstream(root / "hwmon" / "hwmon0" / "temp1_input") << "45200\n";
    std::ofstream(root / "hwmon" / "hwmon1" / "name") << "nvme\n";
    std::ofstream(root / "hwmon" / "hwmon1" / "temp1_input") << "39800\n";
    std::ofstream(root / "thermal" / "thermal_zone0" / "type") << "acpitz\n";
    std::ofstream(root / "thermal" / "thermal_zone0" / "temp") << "28000\n";

    int celsius[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch(root.string().c_str(), celsius);
    Require(celsius[0] == 45 && celsius[1] == 28 && celsius[2] == 39);

    int missing[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch((root / "absent").string().c_str(), missing);
    Require(missing[0] == -1 && missing[1] == -1 && missing[2] == -1);
    ReadThermalSensors_nid_no_patch(nullptr, missing);
    Require(missing[0] == -1);

    const auto skipped = std::filesystem::path("anyps5-thermal-sensors-skip");
    std::filesystem::remove_all(skipped);
    std::filesystem::create_directories(skipped / "hwmon" / "hwmonB");
    std::filesystem::create_directories(skipped / "hwmon" / "hwmonA");
    std::filesystem::create_directories(skipped / "hwmon" / "hwmonC");
    std::ofstream(skipped / "hwmon" / "hwmonA" / "name") << "amdgpu\n";
    std::ofstream(skipped / "hwmon" / "hwmonA" / "temp1_input") << "55000\n";
    std::ofstream(skipped / "hwmon" / "hwmonB" / "name") << "BAT0\n";
    std::ofstream(skipped / "hwmon" / "hwmonB" / "temp1_input") << "31600\n";
    std::ofstream(skipped / "hwmon" / "hwmonC" / "name") << "iwlwifi_1\n";
    std::ofstream(skipped / "hwmon" / "hwmonC" / "temp1_input") << "57000\n";
    int fallback[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch(skipped.string().c_str(), fallback);
    Require(fallback[0] == 55 && fallback[1] == -1 && fallback[2] == -1);
    std::filesystem::remove_all(skipped);

    int value = 0;
    Require(sceKernelGetCpuTemperature(nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetSocSensorTemperature(0, nullptr) == SCE_KERNEL_ERROR_EFAULT);
    Require(sceKernelGetSocSensorTemperature(-1, &value) == SCE_KERNEL_ERROR_EINVAL);
    Require(sceKernelGetSocSensorTemperature(3, &value) == SCE_KERNEL_ERROR_EINVAL);

    int sentinel = -12345;
    Require(sceKernelGetSocSensorTemperature(15, &sentinel) == SCE_KERNEL_ERROR_EINVAL && sentinel == -12345);
    int probe = -12345;
    const int result = sceKernelGetSocSensorTemperature(0, &probe);
    Require(result == 0 ? (probe >= 0 && probe <= 130) : (result == SCE_KERNEL_ERROR_EINVAL && probe == -12345));
    int cpuProbe = -12345;
    const int cpuResult = sceKernelGetCpuTemperature(&cpuProbe);
    Require(cpuResult == 0 ? (cpuProbe >= 0 && cpuProbe <= 130) : (cpuResult == SCE_KERNEL_ERROR_EINVAL && cpuProbe == -12345));

    Require(sceKernelGetSocPowerConsumption(nullptr) == SCE_KERNEL_ERROR_EFAULT);
    std::uint32_t consumption[32];
    std::fill(std::begin(consumption), std::end(consumption), 0xa5a5a5a5u);
    Require(sceKernelGetSocPowerConsumption(consumption) == SCE_KERNEL_ERROR_EINVAL);
    Require(std::all_of(std::begin(consumption), std::end(consumption), [](std::uint32_t word) { return word == 0xa5a5a5a5u; }));

    std::filesystem::remove_all(root);
    return 0;
}
