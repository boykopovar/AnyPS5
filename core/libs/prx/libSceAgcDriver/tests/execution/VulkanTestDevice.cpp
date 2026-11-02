#include "VulkanTestDevice.hpp"
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

enum class Outcome { Device, Skipped, Unavailable, Failed };

void SetVariable(const char* name, const char* value) {
#ifdef _WIN32
    if (_putenv_s(name, value) != 0) throw std::runtime_error(std::string("cannot set ") + name);
#else
    if ((*value == '\0' ? unsetenv(name) : setenv(name, value, 1)) != 0) throw std::runtime_error(std::string("cannot set ") + name);
#endif
}

void Require(const bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

Outcome Open(std::string& message) {
    message.clear();
    try {
        const auto device = OpenVulkanTestDevice();
        if (device) message = device->DeviceName();
        return device ? Outcome::Device : Outcome::Skipped;
    } catch (const AgcDriver::VulkanUnavailable& error) {
        message = error.what();
        return Outcome::Unavailable;
    } catch (const std::exception& error) {
        message = error.what();
        return Outcome::Failed;
    }
}

int DevicePresentTests() {
    SetVariable("ANYPS5_GPU", "");
    std::string message;
    const auto healthy = Open(message);
    if (healthy == Outcome::Skipped) return VulkanTestSkipped;
    Require(healthy == Outcome::Device, "opening the default device failed: " + message);
    const std::string name = message;

    SetVariable("ANYPS5_REQUIRE_VULKAN", "1");
    Require(Open(message) == Outcome::Device, "ANYPS5_REQUIRE_VULKAN=1 did not open " + name + ": " + message);

    SetVariable("ANYPS5_REQUIRE_VULKAN", "");
    SetVariable("ANYPS5_GPU", "no such device 5c1e0b7a");
    const auto failed = Open(message);
    Require(failed != Outcome::Skipped, "a device construction failure on a host with " + name + " was reported as a skip");
    Require(failed == Outcome::Failed, "a device construction failure on a host with " + name + " was reported as no usable Vulkan device: " + message);
    Require(message.find("ANYPS5_GPU=\"no such device 5c1e0b7a\"") != std::string::npos && message.find(name) != std::string::npos, "unexpected error: " + message);
    return 0;
}

int NoDriverTests() {
    SetVariable("ANYPS5_REQUIRE_VULKAN", "");
    SetVariable("ANYPS5_GPU", "");
    std::string message;
    const auto optional = Open(message);
    Require(optional == Outcome::Skipped, "without a Vulkan driver the optional device was not skipped: " + message);

    SetVariable("ANYPS5_REQUIRE_VULKAN", "1");
    const auto required = Open(message);
    Require(required != Outcome::Skipped, "without a Vulkan driver ANYPS5_REQUIRE_VULKAN=1 was reported as a skip");
    Require(required == Outcome::Unavailable, "without a Vulkan driver ANYPS5_REQUIRE_VULKAN=1 failed with an unexpected error: " + message);
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        Require(mode.empty() || mode == "--no-driver", "unknown mode " + std::string(mode));
        const int result = mode.empty() ? DevicePresentTests() : NoDriverTests();
        if (result == 0) std::cout << "Vulkan test device tests passed\n";
        return result;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
