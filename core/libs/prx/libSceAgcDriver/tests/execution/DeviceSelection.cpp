#include "prx/libSceAgcDriver/Execution/include/VulkanDevice.hpp"
#include "VulkanTestDevice.hpp"
#include <cctype>
#include <cstdlib>
#include <stdexcept>
#include <string>

namespace {

using Testing::Require;

int WriteRequest(const std::string& value) {
#ifdef _WIN32
    return _putenv_s("ANYPS5_GPU", value.c_str());
#else
    return setenv("ANYPS5_GPU", value.c_str(), 1);
#endif
}

class Request {
public:
    explicit Request(const std::string& value) {
        Require(WriteRequest(value) == 0, "cannot set ANYPS5_GPU");
    }

    ~Request() {
        static_cast<void>(WriteRequest(""));
    }

    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;
};

std::string DefaultName() {
    const Request request("");
    const std::string name = SharedVulkanTestDevice().DeviceName();
    Require(!name.empty(), "the default device has no name");
    return name;
}

const Testing::Case defaultDevice{"DeviceSelection_EmptyRequest_SelectsNamedDevice", [] {
    static_cast<void>(DefaultName());
}};

const Testing::Case uppercaseName{"DeviceSelection_UppercaseName_SelectsDefaultDevice", [] {
    const std::string name = DefaultName();
    std::string upper = name;
    for (auto& character : upper) character = static_cast<char>(std::toupper(static_cast<unsigned char>(character)));
    const Request request(upper);
    const AgcDriver::VulkanDevice requested;
    Require(requested.DeviceName() == name, "ANYPS5_GPU=" + upper + " selected " + requested.DeviceName() + " instead of " + name);
}};

const Testing::Case unknownName{"DeviceSelection_UnknownName_ThrowsNamingRequestAndDevices", [] {
    const std::string name = DefaultName();
    const Request request("no such device 1d3d154f");
    const auto error = Testing::RequireThrows<std::runtime_error>([] {
        const AgcDriver::VulkanDevice missing;
        Testing::Fail("ANYPS5_GPU naming no device selected " + missing.DeviceName());
    }, "ANYPS5_GPU naming no device");
    const std::string message = error.what();
    Require(message.find("ANYPS5_GPU=\"no such device 1d3d154f\"") != std::string::npos && message.find(name) != std::string::npos, "unexpected error: " + message);
}};

} // namespace
