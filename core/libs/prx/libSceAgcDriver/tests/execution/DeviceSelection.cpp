#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "VulkanTestDevice.hpp"
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {

void SetRequest(const std::string& value) {
#ifdef _WIN32
    if (_putenv_s("ANYPS5_GPU", value.c_str()) != 0) throw std::runtime_error("cannot set ANYPS5_GPU");
#else
    if (setenv("ANYPS5_GPU", value.c_str(), 1) != 0) throw std::runtime_error("cannot set ANYPS5_GPU");
#endif
}

void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

}

int main() {
    try {
        const auto surfaceWarning = AgcDriver::SoftwareFallbackWarning(VK_PHYSICAL_DEVICE_TYPE_CPU, "llvmpipe", "Hardware GPU", AgcDriver::PresentationRejection::NoSurfaceQueue, false);
        Require(surfaceWarning.find("llvmpipe") != std::string::npos && surfaceWarning.find("Hardware GPU") != std::string::npos && surfaceWarning.find("cannot present to this window") != std::string::npos, "the software fallback warning omitted the selected device, rejected GPU or surface reason");
        const auto extensionWarning = AgcDriver::SoftwareFallbackWarning(VK_PHYSICAL_DEVICE_TYPE_CPU, "llvmpipe", "Hardware GPU", AgcDriver::PresentationRejection::MissingSwapchain, false);
        Require(extensionWarning.find("VK_KHR_swapchain") != std::string::npos, "the software fallback warning omitted the swapchain reason");
        Require(AgcDriver::SoftwareFallbackWarning(VK_PHYSICAL_DEVICE_TYPE_CPU, "llvmpipe", "Hardware GPU", AgcDriver::PresentationRejection::NoSurfaceQueue, true).empty(), "an explicit CPU request warned about an automatic fallback");
        Require(AgcDriver::SoftwareFallbackWarning(VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, "Hardware GPU", "Other GPU", AgcDriver::PresentationRejection::NoSurfaceQueue, false).empty(), "a hardware selection warned about a software fallback");
        Require(AgcDriver::SoftwareFallbackWarning(VK_PHYSICAL_DEVICE_TYPE_CPU, "llvmpipe", "", AgcDriver::PresentationRejection::None, false).empty(), "a CPU-only host warned about an absent hardware GPU");
        SetRequest("");
        const auto defaultDevice = OpenVulkanTestDevice();
        if (!defaultDevice) return VulkanTestSkipped;
        const std::string name = defaultDevice->DeviceName();
        Require(!name.empty(), "the default device has no name");

        std::string request = name;
        for (auto& character : request) character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
        SetRequest(request);
        const AgcDriver::VulkanDevice requested;
        Require(requested.DeviceName() == name, "ANYPS5_GPU=" + request + " selected " + requested.DeviceName() + " instead of " + name);

        SetRequest("no such device 1d3d154f");
        try {
            const AgcDriver::VulkanDevice missing;
            throw std::runtime_error("ANYPS5_GPU naming no device selected " + missing.DeviceName());
        } catch (const std::runtime_error& error) {
            const std::string message = error.what();
            Require(message.find("ANYPS5_GPU=\"no such device 1d3d154f\"") != std::string::npos && message.find(name) != std::string::npos, "unexpected error: " + message);
        }
        std::puts("device selection tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
