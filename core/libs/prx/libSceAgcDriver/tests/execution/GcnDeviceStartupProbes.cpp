#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "VulkanTestDevice.hpp"
#include <cstdio>
#include <exception>
#include <iostream>

int main() {
    try {
        ShaderRecompiler::SetGcnGuestShaders(true);
        const auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        if (!ShaderRecompiler::GcnGuestShaders()) {
            std::cerr << "opening a device turned GCN guest shaders off\n";
            return 1;
        }
        std::puts("GCN device startup probe tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
