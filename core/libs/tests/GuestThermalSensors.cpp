#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include <algorithm>
#include <array>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#ifndef _WIN32
#include <sys/stat.h>
#endif

extern "C" {
void ReadThermalSensors_nid_no_patch(const char* root, int celsius[3]);
bool StorageDescriptorAllowsTemperatureQuery_nid_no_patch(const void* descriptor, std::size_t bytes);
int ExtractStorageTemperature_nid_no_patch(const void* descriptor, std::size_t bytes);

}

static void Require(bool value) {
    if (!value) {
        std::fprintf(stderr, "thermal sensors check failed\n");
        std::abort();
    }
}

static void WriteU16(unsigned char* buffer, std::size_t offset, std::uint16_t value) {
    std::memcpy(buffer + offset, &value, sizeof(value));
}

static void WriteI16(unsigned char* buffer, std::size_t offset, std::int16_t value) {
    std::memcpy(buffer + offset, &value, sizeof(value));
}

int main() {
    unsigned char storageDescriptor[32]{};
    storageDescriptor[28] = 8;
    Require(!StorageDescriptorAllowsTemperatureQuery_nid_no_patch(storageDescriptor, sizeof(storageDescriptor)));
    storageDescriptor[28] = 17;
    Require(StorageDescriptorAllowsTemperatureQuery_nid_no_patch(storageDescriptor, sizeof(storageDescriptor)));
    Require(!StorageDescriptorAllowsTemperatureQuery_nid_no_patch(storageDescriptor, sizeof(storageDescriptor) - 1));
    Require(!StorageDescriptorAllowsTemperatureQuery_nid_no_patch(nullptr, sizeof(storageDescriptor)));

    constexpr std::size_t kInfoCountOffset = 12;
    constexpr std::size_t kInfoOffset = 24;
    constexpr std::size_t kInfoSize = 16;
    constexpr std::size_t kTempOffset = 2;
    std::array<unsigned char, kInfoOffset + 16 * kInfoSize> tempDescriptor{};
    Require(ExtractStorageTemperature_nid_no_patch(nullptr, tempDescriptor.size()) == -1);
    WriteU16(tempDescriptor.data(), kInfoCountOffset, 1);
    WriteI16(tempDescriptor.data(), kInfoOffset + kTempOffset, 42);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), 0) == -1);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), kInfoOffset) == -1);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), kInfoOffset + kTempOffset + 1) == -1);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), kInfoOffset + kTempOffset + 2) == 42);

    WriteU16(tempDescriptor.data(), kInfoCountOffset, 0);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), tempDescriptor.size()) == -1);

    tempDescriptor.fill(0);
    WriteU16(tempDescriptor.data(), kInfoCountOffset, 2);
    WriteI16(tempDescriptor.data(), kInfoOffset + kTempOffset, -5);
    WriteI16(tempDescriptor.data(), kInfoOffset + kInfoSize + kTempOffset, 41);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), tempDescriptor.size()) == 41);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), kInfoOffset + kInfoSize) == -1);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), kInfoOffset + kInfoSize + kTempOffset + 1) == -1);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), kInfoOffset + kInfoSize + kTempOffset + 2) == 41);

    tempDescriptor.fill(0);
    WriteU16(tempDescriptor.data(), kInfoCountOffset, 3);
    WriteI16(tempDescriptor.data(), kInfoOffset + kTempOffset, 500);
    WriteI16(tempDescriptor.data(), kInfoOffset + kInfoSize + kTempOffset, -10);
    WriteI16(tempDescriptor.data(), kInfoOffset + 2 * kInfoSize + kTempOffset, 38);
    Require(ExtractStorageTemperature_nid_no_patch(tempDescriptor.data(), tempDescriptor.size()) == 38);

    std::array<unsigned char, kInfoOffset + 17 * kInfoSize> oversizedDescriptor{};
    WriteU16(oversizedDescriptor.data(), kInfoCountOffset, 64);
    for (std::size_t i = 0; i < 16; ++i) {
        WriteI16(oversizedDescriptor.data(), kInfoOffset + i * kInfoSize + kTempOffset, -5);
    }
    WriteI16(oversizedDescriptor.data(), kInfoOffset + 16 * kInfoSize + kTempOffset, 44);
    Require(ExtractStorageTemperature_nid_no_patch(oversizedDescriptor.data(), kInfoOffset + 16 * kInfoSize) == -1);
    Require(ExtractStorageTemperature_nid_no_patch(oversizedDescriptor.data(), oversizedDescriptor.size()) == -1);
    WriteI16(oversizedDescriptor.data(), kInfoOffset + 15 * kInfoSize + kTempOffset, 44);
    Require(ExtractStorageTemperature_nid_no_patch(oversizedDescriptor.data(), kInfoOffset + 16 * kInfoSize) == 44);

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
    std::ofstream(skipped / "hwmon" / "hwmonC" / "name") << "iwlwifi_1\n";
#ifdef _WIN32
    std::ofstream(skipped / "hwmon" / "hwmonB" / "temp1_input") << "31600\n";
    std::ofstream(skipped / "hwmon" / "hwmonC" / "temp1_input") << "57000\n";
#else
    Require(mkfifo((skipped / "hwmon" / "hwmonB" / "temp1_input").c_str(), 0600) == 0);
    Require(mkfifo((skipped / "hwmon" / "hwmonC" / "temp1_input").c_str(), 0600) == 0);
#endif
    int fallback[3] = {0, 0, 0};
    ReadThermalSensors_nid_no_patch(skipped.string().c_str(), fallback);
    Require(fallback[0] == 55 && fallback[1] == -1 && fallback[2] == -1);
    std::filesystem::remove_all(skipped);

    *__error_nid_postfix() = 0;
    Require(sceKernelGetCpuTemperature(nullptr) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sceKernelGetSocSensorTemperature(0, nullptr) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 14);
    for (const int invalid : {-1, 65, 256, INT_MAX, INT_MIN}) {
        int untouched = -12345;
        *__error_nid_postfix() = 0;
        Require(sceKernelGetSocSensorTemperature(invalid, &untouched) == SCE_KERNEL_ERROR_EINVAL && *__error_nid_postfix() == 22 && untouched == -12345);
        *__error_nid_postfix() = 0;
        Require(sceKernelGetSocSensorTemperature(invalid, nullptr) == SCE_KERNEL_ERROR_EINVAL && *__error_nid_postfix() == 22);
    }
    for (const int unmapped : {3, 15, 32, 63}) {
        *__error_nid_postfix() = 0;
        Require(sceKernelGetSocSensorTemperature(unmapped, nullptr) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 14);
    }

    int probe = -12345;
    const int result = sceKernelGetSocSensorTemperature(0, &probe);
    Require(result == 0 ? (probe >= 0 && probe <= 130) : (result == SCE_KERNEL_ERROR_EINVAL && probe == -12345));
    *__error_nid_postfix() = 0;
    Require(sceKernelGetSocSensorTemperature(64, nullptr) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 14);
    *__error_nid_postfix() = 0;
    Require(sceKernelGetSocSensorTemperature(64, reinterpret_cast<int*>(0x10)) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 14);
    int channel64 = -12345;
    Require(sceKernelGetSocSensorTemperature(64, &channel64) == 0 && channel64 == -49);
    for (int measured = 3; measured <= 63; ++measured) {
        int channel = -12345;
        const int measuredResult = sceKernelGetSocSensorTemperature(measured, &channel);
        Require(measuredResult == result);
        Require(measuredResult == 0 ? (channel >= 0 && channel <= 130) : channel == -12345);
    }
    int cpuProbe = -12345;
    const int cpuResult = sceKernelGetCpuTemperature(&cpuProbe);
    Require(cpuResult == 0 ? (cpuProbe >= 0 && cpuProbe <= 130) : (cpuResult == SCE_KERNEL_ERROR_EINVAL && cpuProbe == -12345));
    if (result == 0) {
        *__error_nid_postfix() = 0;
        Require(sceKernelGetSocSensorTemperature(0, reinterpret_cast<int*>(0x10)) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 14);
    }
    if (cpuResult == 0) {
        *__error_nid_postfix() = 0;
        Require(sceKernelGetCpuTemperature(reinterpret_cast<int*>(0x10)) == SCE_KERNEL_ERROR_EFAULT && *__error_nid_postfix() == 14);
    }

    Require(sceKernelGetSocPowerConsumption(nullptr) == SCE_KERNEL_ERROR_EFAULT);
    std::uint32_t consumption[32];
    std::fill(std::begin(consumption), std::end(consumption), 0xa5a5a5a5u);
    Require(sceKernelGetSocPowerConsumption(consumption) == SCE_KERNEL_ERROR_EINVAL);
    Require(std::all_of(std::begin(consumption), std::end(consumption), [](std::uint32_t word) { return word == 0xa5a5a5a5u; }));

    std::filesystem::remove_all(root);
    return 0;
}
