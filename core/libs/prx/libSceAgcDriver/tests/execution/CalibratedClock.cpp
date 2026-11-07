#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Context.hpp"
#include "VulkanTestDevice.hpp"
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <string>
#include <thread>

namespace {

using AgcDriver::Graphics::Require;

void EnableTraceTimestamps() {
#ifdef _WIN32
    _putenv_s("APS5_TRACE_SECONDS", "100000:1");
#else
    setenv("APS5_TRACE_SECONDS", "100000:1", 1);
#endif
}

double Milliseconds(std::chrono::steady_clock::duration duration) {
    return std::chrono::duration<double, std::milli>(duration).count();
}

}

int main() {
    try {
        EnableTraceTimestamps();
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        double firstGpuNs = 0.0;
        std::chrono::steady_clock::time_point firstCpu;
        if (!device->CalibrateClock(firstGpuNs, firstCpu)) {
            std::cout << "skipped, the device calibrates its timestamps against no host clock the driver reads\n";
            return VulkanTestSkipped;
        }
        const double offsetMs = std::abs(Milliseconds(std::chrono::steady_clock::now() - firstCpu));
        Require(offsetMs < 50.0, "the calibrated GPU timestamp maps to a CPU time " + std::to_string(offsetMs) + " ms away from now: the host clock read is not the one the driver calibrated against");
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        double secondGpuNs = 0.0;
        std::chrono::steady_clock::time_point secondCpu;
        Require(device->CalibrateClock(secondGpuNs, secondCpu), "a second calibration failed after the first succeeded");
        const double cpuMs = Milliseconds(secondCpu - firstCpu);
        const double gpuMs = (secondGpuNs - firstGpuNs) / 1e6;
        Require(cpuMs >= 150.0 && std::abs(gpuMs - cpuMs) < 2.0, "200 ms apart, the two calibrations differ by " + std::to_string(gpuMs) + " ms on the GPU clock and " + std::to_string(cpuMs) + " ms on the CPU clock");
        std::cout << "calibrated clock tests passed (offset " << offsetMs << " ms, GPU " << gpuMs << " ms against CPU " << cpuMs << " ms)\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
