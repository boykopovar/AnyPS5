#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HOSTREPORT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_HOSTREPORT_HPP

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace AgcDriver {

struct HostRequirement {
    std::string name;
    std::string kind;
    bool required = false;
    bool present = false;
};

struct HostGpuFacts {
    std::string name;
    std::string type;
    std::string driverName;
    std::string driverInfo;
    std::uint32_t vendorId = 0;
    std::uint32_t deviceId = 0;
    std::uint32_t apiVersion = 0;
    std::uint32_t driverVersion = 0;
    std::uint32_t subgroupSize = 0;
    std::uint64_t deviceLocalMiB = 0;
    std::vector<std::string> candidates;
    std::vector<std::string> presentModes;
    std::string requestedPresentMode;
    std::vector<HostRequirement> requirements;
};

struct HostShaderCacheFacts {
    bool enabled = false;
    bool present = false;
    std::uint64_t sizeMiB = 0;
};

struct HostFacts {
    std::string commit;
    std::optional<std::string> os;
    std::optional<std::string> cpuModel;
    std::optional<std::uint64_t> cpuThreads;
    std::optional<std::uint64_t> memoryMiB;
    std::optional<std::vector<std::string>> cpuMissing;
    std::optional<std::string> sdlLinked;
    std::optional<std::string> sdlCompiled;
    std::optional<HostGpuFacts> gpu;
    std::optional<HostShaderCacheFacts> shaderCache;
    std::vector<std::pair<std::string, std::string>> environment;
    std::vector<std::string> redactions;
    std::vector<std::string> unavailable;
};

struct HostSources {
    std::string commit;
    std::function<std::string()> os;
    std::function<std::string()> cpuModel;
    std::function<std::uint64_t()> cpuThreads;
    std::function<std::uint64_t()> memoryMiB;
    std::function<std::vector<std::string>()> cpuMissing;
    std::function<std::string()> sdlLinked;
    std::function<std::string()> sdlCompiled;
    std::function<HostShaderCacheFacts()> shaderCache;
    std::function<std::vector<std::string>()> environment;
    std::function<std::vector<std::string>()> redactions;
};

HostFacts CollectHostFacts(const HostSources& sources);
std::vector<std::pair<std::string, std::string>> FilterHostEnvironment(const std::vector<std::string>& entries);
std::vector<std::string> HostRedactionNames(const std::string& user, const std::string& hostName, const std::string& homeDirectory);
std::vector<HostRequirement> MissingRequirements(const HostGpuFacts& gpu);
std::string DescribeMissingRequirements(const HostGpuFacts& gpu, const std::vector<std::string>& redactions = {});
bool ParseHostReportSetting(const char* value);
std::string RedactHostText(const std::string& text, const std::vector<std::string>& names = {});
std::string FormatHostReport(const HostFacts& facts);

}

#endif
