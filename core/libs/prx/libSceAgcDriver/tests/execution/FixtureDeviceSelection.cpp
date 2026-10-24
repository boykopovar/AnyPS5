#include "FixtureDeviceSelection.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void SetRequest(const char* value) {
#ifdef _WIN32
    if (_putenv_s("ANYPS5_GPU", value) != 0) throw std::runtime_error("cannot set ANYPS5_GPU");
#else
    if (setenv("ANYPS5_GPU", value, 1) != 0) throw std::runtime_error("cannot set ANYPS5_GPU");
#endif
}

VkPhysicalDeviceProperties Properties(const char* name, VkPhysicalDeviceType type) {
    VkPhysicalDeviceProperties result{};
    result.apiVersion = VK_API_VERSION_1_1;
    result.deviceType = type;
    std::memcpy(result.deviceName, name, std::strlen(name) + 1u);
    return result;
}

std::array<VkPhysicalDeviceProperties, 3> described{
    Properties("llvmpipe (LLVM 20.1.2, 256 bits)", VK_PHYSICAL_DEVICE_TYPE_CPU),
    Properties("AMD Radeon(TM) Graphics", VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU),
    Properties("NVIDIA GeForce RTX 5080", VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU),
};

VkPhysicalDevice Handle(std::size_t index) {
    return reinterpret_cast<VkPhysicalDevice>(static_cast<std::uintptr_t>(index + 1u));
}

void VKAPI_CALL Describe(VkPhysicalDevice device, VkPhysicalDeviceProperties* properties) {
    *properties = described[reinterpret_cast<std::uintptr_t>(device) - 1u];
}

std::string SelectionError(const char* request) {
    SetRequest(request);
    try {
        static_cast<void>(SelectFixturePhysicalDevice(std::array{Handle(0), Handle(1), Handle(2)}, Describe));
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    throw std::runtime_error(std::string("ANYPS5_GPU=\"") + request + "\" selected a device");
}

void SelectionTests() {
    std::array devices = described;
    Require(!SelectFixtureDevice({}, {}).has_value(), "an empty device list selected a device");
    Require(SelectFixtureDevice(devices, {}) == 2u, "default selection did not prefer the discrete GPU");
    Require(SelectFixtureDevice(std::span(devices).first(2), {}) == 1u, "default selection did not prefer the integrated GPU to the CPU device");
    Require(SelectFixtureDevice(devices, "AMD") == 1u, "an AMD request lost to the discrete GPU");
    Require(SelectFixtureDevice(devices, "aMd rAdEoN") == 1u, "GPU name matching was not case-insensitive");
    Require(SelectFixtureDevice(devices, "RTX 5080") == 2u, "partial GPU name matching failed");
    Require(SelectFixtureDevice(devices, "llvmpipe") == 0u, "a requested CPU device was not selected");
    Require(!SelectFixtureDevice(devices, "missing device").has_value(), "an unmatched GPU request selected a device");
    Require(!SelectFixtureDevice(devices, "NVIDIA GeForce RTX 5080 with a longer name").has_value(), "a request longer than the device name matched");
    devices[0] = devices[1];
    Require(SelectFixtureDevice(devices, "AMD") == 0u, "equally ranked matches did not keep enumeration order");
    devices[1].apiVersion = VK_API_VERSION_1_0;
    devices[2].apiVersion = VK_API_VERSION_1_0;
    Require(!SelectFixtureDevice(devices, "NVIDIA").has_value(), "a requested device without Vulkan 1.1 was selected");
}

void EnvironmentTests() {
    const std::array handles{Handle(0), Handle(1), Handle(2)};
    SetRequest("");
    Require(SelectFixturePhysicalDevice(handles, Describe) == Handle(2), "an empty ANYPS5_GPU did not select the discrete GPU");
    SetRequest("radeon");
    Require(SelectFixturePhysicalDevice(handles, Describe) == Handle(1), "ANYPS5_GPU=radeon did not select the integrated GPU");
    SetRequest("LLVMPIPE");
    Require(SelectFixturePhysicalDevice(handles, Describe) == Handle(0), "ANYPS5_GPU=LLVMPIPE did not select the CPU device");
    const std::string error = SelectionError("no such device 1d3d154f");
    Require(error == "no Vulkan 1.1 device whose name contains ANYPS5_GPU=\"no such device 1d3d154f\"; devices: llvmpipe (LLVM 20.1.2, 256 bits), AMD Radeon(TM) Graphics, NVIDIA GeForce RTX 5080", "unexpected error: " + error);
}

}

int main() {
    try {
        SelectionTests();
        EnvironmentTests();
        std::puts("fixture device selection tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
