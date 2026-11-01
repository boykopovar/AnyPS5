#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"

#include <cstdio>
#include <stdexcept>

namespace {

PFN_vkGetDeviceProcAddr realProc = nullptr;
int countedCalls = 0;

PFN_vkVoidFunction VKAPI_CALL countingProc(VkDevice device, const char* name) {
    ++countedCalls;
    return realProc(device, name);
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr char DispatchName[] = "vkCmdDispatch";

}

int main() {
    try {
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        auto context = device->graphicsContext();
        require(context.functions != nullptr && context.functionCache != nullptr, "the device context carries its function cache");
        const auto first = context.Function<PFN_vkCmdDispatch>(DispatchName);
        const auto lookups = AgcDriver::Graphics::DeviceProcLookups();
        require(context.Function<PFN_vkCmdDispatch>(DispatchName) == first && AgcDriver::Graphics::DeviceProcLookups() == lookups,
                "a repeated lookup is served from the device's cache");
        realProc = context.deviceProc;
        context.deviceProc = countingProc;
        require(context.Function<PFN_vkCmdDispatch>(DispatchName) == first && countedCalls == 1, "another resolver is not served from the cache");
        context.deviceProc = realProc;
        require(context.Function<PFN_vkCmdDispatch>(DispatchName) == first && AgcDriver::Graphics::DeviceProcLookups() == lookups + 1u,
                "the device's resolver is still served from the cache");
        context.deviceProc = countingProc;
        context.functions = nullptr;
        static_cast<void>(context.Function<PFN_vkCmdDispatch>(DispatchName));
        require(countedCalls == 2, "a context without the device's table looks up every time");
        std::printf("device function cache tests passed\n");
        return 0;
    } catch (const std::exception& error) {
        std::printf("device function cache tests failed: %s\n", error.what());
        return 1;
    }
}
