#include "prx/libkernel/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libkernel/System/include/ThermalSensors.hpp"
#include "prx/libkernel/KernelErrors.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <oleauto.h>
#include <wbemidl.h>
#endif

extern "C" bool StorageDescriptorAllowsTemperatureQuery_nid_no_patch(const void* descriptor, std::size_t bytes) {
    constexpr std::size_t kBusTypeOffset = 28;
    constexpr std::uint32_t kBusTypeRaid = 8;
    std::uint32_t busType = 0;
    if (descriptor == nullptr || bytes < kBusTypeOffset + sizeof(busType)) return false;
    std::memcpy(&busType, static_cast<const char*>(descriptor) + kBusTypeOffset, sizeof(busType));
    return busType != kBusTypeRaid;
}

namespace {

#ifdef _WIN32

void ReadAcpiThermalZones(int celsius[2]) {
    celsius[0] = celsius[1] = -1;
    const HRESULT started = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    const bool uninitialize = SUCCEEDED(started);
    if (FAILED(started) && started != RPC_E_CHANGED_MODE) return;
    IWbemLocator* locator = nullptr;
    IWbemServices* services = nullptr;
    if (SUCCEEDED(CoCreateInstance(CLSID_WbemLocator, nullptr, CLSCTX_INPROC_SERVER, IID_IWbemLocator, reinterpret_cast<void**>(&locator))) && locator != nullptr) {
        BSTR path = SysAllocString(L"ROOT\\WMI");
        if (path != nullptr && SUCCEEDED(locator->ConnectServer(path, nullptr, nullptr, nullptr, 0, nullptr, nullptr, &services)) && services != nullptr) {
            CoSetProxyBlanket(services, RPC_C_AUTHN_WINNT, RPC_C_AUTHZ_NONE, nullptr, RPC_C_AUTHN_LEVEL_CALL, RPC_C_IMP_LEVEL_IMPERSONATE, nullptr, EOAC_NONE);
            IEnumWbemClassObject* results = nullptr;
            BSTR language = SysAllocString(L"WQL");
            BSTR query = SysAllocString(L"SELECT CurrentTemperature FROM MSAcpi_ThermalZoneTemperature");
            if (language != nullptr && query != nullptr && SUCCEEDED(services->ExecQuery(language, query, WBEM_FLAG_FORWARD_ONLY, nullptr, &results)) && results != nullptr) {
                int found = 0;
                while (found < 2) {
                    IWbemClassObject* object = nullptr;
                    ULONG returned = 0;
                    if (results->Next(1000, 1, &object, &returned) != S_OK || returned != 1 || object == nullptr) break;
                    VARIANT value;
                    VariantInit(&value);
                    if (SUCCEEDED(object->Get(L"CurrentTemperature", 0, &value, nullptr, nullptr))) {
                        if (value.vt == VT_I4 || value.vt == VT_INT) {
                            const long tenthsKelvin = value.vt == VT_I4 ? value.lVal : value.intVal;
                            int reading = static_cast<int>(tenthsKelvin / 10) - 273;
                            Plausible(reading);
                            if (reading >= 0) celsius[found++] = reading;
                        }
                    }
                    VariantClear(&value);
                    object->Release();
                }
                results->Release();
            }
            if (language != nullptr) SysFreeString(language);
            if (query != nullptr) SysFreeString(query);
            services->Release();
        }
        if (path != nullptr) SysFreeString(path);
        locator->Release();
    }
    if (uninitialize) CoUninitialize();
}

int ReadStorageTemperature() {
    constexpr ULONG kStorageDeviceTemperatureProperty = 52;
    constexpr ULONG kLegacyStorageDeviceTemperatureProperty = 22;
    constexpr ULONG kStorageDeviceProperty = 0;
    constexpr ULONG kPropertyStandardQuery = 0;
    constexpr DWORD kIoctlStorageQueryProperty = 0x2D1400;
    struct Information {
        USHORT index;
        SHORT temperature;
        SHORT overThreshold;
        SHORT underThreshold;
        UCHAR overThresholdChangable;
        UCHAR underThresholdChangable;
        UCHAR eventGenerated;
        UCHAR reserved0;
        ULONG reserved1;
    };
    struct Descriptor {
        ULONG version;
        ULONG size;
        SHORT criticalTemperature;
        SHORT warningTemperature;
        USHORT infoCount;
        UCHAR reserved0[2];
        ULONG reserved1[2];
        Information information[16];
    };
    constexpr std::size_t kTemperatureBytes =
        offsetof(Descriptor, information) + offsetof(Information, temperature) + sizeof(SHORT);
    static_assert(offsetof(Descriptor, infoCount) == 12 && offsetof(Descriptor, information) == 24 &&
        offsetof(Information, temperature) == 2);
    struct Query {
        ULONG property;
        ULONG query;
        UCHAR extra[1];
    };
    constexpr ULONG kProperties[] = {kStorageDeviceTemperatureProperty, kLegacyStorageDeviceTemperatureProperty};
    for (int index = 0; index < 16; ++index) {
        char name[32];
        std::snprintf(name, sizeof(name), "\\\\.\\PhysicalDrive%d", index);
        HANDLE handle = CreateFileA(name, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING, 0, nullptr);
        if (handle == INVALID_HANDLE_VALUE) continue;
        Query deviceQuery{};
        deviceQuery.property = kStorageDeviceProperty;
        deviceQuery.query = kPropertyStandardQuery;
        unsigned char deviceDescriptor[1024]{};
        DWORD deviceBytes = 0;
        if (!DeviceIoControl(handle, kIoctlStorageQueryProperty, &deviceQuery, sizeof(deviceQuery), deviceDescriptor, sizeof(deviceDescriptor), &deviceBytes, nullptr)
            || !StorageDescriptorAllowsTemperatureQuery_nid_no_patch(deviceDescriptor, deviceBytes)) {
            CloseHandle(handle);
            continue;
        }
        for (ULONG property : kProperties) {
            Query query{};
            query.property = property;
            query.query = kPropertyStandardQuery;
            Descriptor descriptor{};
            DWORD returned = 0;
            const bool ok = DeviceIoControl(handle, kIoctlStorageQueryProperty, &query, sizeof(query), &descriptor, sizeof(descriptor), &returned, nullptr) != 0;
            if (!ok || returned < kTemperatureBytes || descriptor.infoCount == 0) continue;
            int reading = descriptor.information[0].temperature;
            Plausible(reading);
            if (reading >= 0) {
                CloseHandle(handle);
                return reading;
            }
        }
        CloseHandle(handle);
    }
    return -1;
}

#endif

int Fail(int error) {
    *__error_nid_postfix() = error;
    return SceKernelError(error);
}

void ReadHostSensors(int celsius[3]) {
#ifdef _WIN32
    celsius[2] = ReadStorageTemperature();
    ReadAcpiThermalZones(celsius);
#elif defined(__linux__)
    ReadThermalSensors_nid_no_patch("/sys/class", celsius);
#else
    celsius[0] = celsius[1] = celsius[2] = -1;
#endif
}

}

extern "C" {

int APS5_VABI sceKernelGetCpuTemperature(int* outCelsius) {
    if (outCelsius == nullptr) return Fail(14);
    int celsius[3];
    ReadHostSensors(celsius);
    if (celsius[0] < 0) return Fail(22);
    if (!GuestCopyOut_nid_no_patch(outCelsius, &celsius[0], sizeof(int))) return Fail(14);
    return 0;
}

int APS5_VABI sceKernelGetSocSensorTemperature(int sensorId, int* outCelsius) {
    if (sensorId < 0 || sensorId > 64) return Fail(22);
    if (outCelsius == nullptr) return Fail(14);
    if (sensorId == 64) {
        const int value = -49;
        if (!GuestCopyOut_nid_no_patch(outCelsius, &value, sizeof(value))) return Fail(14);
        return 0;
    }
    if (sensorId > 3 && sensorId < 62) throw std::runtime_error("sceKernelGetSocSensorTemperature: channels 4 to 61 have no host sensor and were not measured");
    int celsius[3];
    ReadHostSensors(celsius);
    const int host = sensorId >= 3 ? 0 : sensorId;
    if (celsius[host] < 0) return Fail(22);
    if (!GuestCopyOut_nid_no_patch(outCelsius, &celsius[host], sizeof(int))) return Fail(14);
    return 0;
}

int APS5_VABI sceKernelGetSocPowerConsumption(void* consumption) {
    if (consumption == nullptr) return SCE_KERNEL_ERROR_EFAULT;
    return SCE_KERNEL_ERROR_EINVAL;
}

}
