#include "prx/libSceAgcDriver/Execution/include/HostReportSources.hpp"
#include "prx/libSceAgcDriver/TraceRevision.hpp"
#include "prx/libkernel/System/include/HostCpu.hpp"
#include "ShaderCacheDirectory.hpp"
#include <SDL_version.h>
#include <cstdlib>
#include <filesystem>
#include <stdexcept>
#include <system_error>
#include <thread>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <sys/utsname.h>
#include <unistd.h>
#endif

namespace AgcDriver {

namespace {

constexpr std::uint64_t MaxCacheEntries = 200000;

std::string operatingSystem() {
#ifdef _WIN32
    using QueryVersion = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);
    static const auto query = reinterpret_cast<QueryVersion>(reinterpret_cast<void*>(::GetProcAddress(::GetModuleHandleW(L"ntdll.dll"), "RtlGetVersion")));
    RTL_OSVERSIONINFOW version{};
    version.dwOSVersionInfoSize = sizeof(version);
    if (query == nullptr || query(&version) != 0) throw std::runtime_error("RtlGetVersion failed");
    return "Windows " + std::to_string(version.dwMajorVersion) + "." + std::to_string(version.dwMinorVersion) + "." + std::to_string(version.dwBuildNumber);
#else
    utsname name{};
    if (::uname(&name) != 0) throw std::system_error(errno, std::generic_category(), "uname");
    return std::string(name.sysname) + " " + name.release + " " + name.machine;
#endif
}

std::uint64_t physicalMemoryMiB() {
#ifdef _WIN32
    MEMORYSTATUSEX status{};
    status.dwLength = sizeof(status);
    if (::GlobalMemoryStatusEx(&status) == 0) throw std::system_error(static_cast<int>(::GetLastError()), std::system_category(), "GlobalMemoryStatusEx");
    return status.ullTotalPhys / (1024 * 1024);
#else
    const long pages = ::sysconf(_SC_PHYS_PAGES);
    const long size = ::sysconf(_SC_PAGE_SIZE);
    if (pages <= 0 || size <= 0) throw std::system_error(errno, std::generic_category(), "sysconf physical memory");
    return static_cast<std::uint64_t>(pages) * static_cast<std::uint64_t>(size) / (1024 * 1024);
#endif
}

std::string cpuModel() {
    const std::string model = HostCpuModel_nid_no_patch().value;
    const auto first = model.find_first_not_of(' ');
    if (first == std::string::npos) throw std::runtime_error("the CPU reports no brand string");
    const auto last = model.find_last_not_of(' ');
    return model.substr(first, last - first + 1);
}

std::vector<std::string> cpuMissing() {
#if (defined(__x86_64__) || defined(__i386__)) && !defined(__APPLE__)
    const auto registers = ReadHostCpuRegisters_nid_no_patch();
    const std::string joined = MissingHostCpuFeatures_nid_no_patch(&registers).value;
    std::vector<std::string> missing;
    for (std::size_t start = 0; start < joined.size();) {
        auto end = joined.find(", ", start);
        if (end == std::string::npos) end = joined.size();
        missing.push_back(joined.substr(start, end - start));
        start = end + 2;
    }
    return missing;
#else
    throw std::runtime_error("the CPU gate runs on x86 Linux and Windows hosts only");
#endif
}

std::string versionText(const SDL_version& version) {
    return std::to_string(version.major) + "." + std::to_string(version.minor) + "." + std::to_string(version.patch);
}

std::string sdlLinked() {
    SDL_version version{};
    SDL_GetVersion(&version);
    return versionText(version);
}

std::string sdlCompiled() {
    SDL_version version{};
    SDL_VERSION(&version);
    return versionText(version);
}

HostShaderCacheFacts shaderCache() {
    HostShaderCacheFacts facts;
    const auto directory = ShaderRecompiler::ShaderCacheDirectory();
    if (directory.empty()) return facts;
    facts.enabled = true;
    std::error_code error;
    facts.present = std::filesystem::is_directory(directory, error) && !error;
    if (!facts.present) return facts;
    std::uint64_t bytes = 0;
    std::uint64_t entries = 0;
    std::filesystem::recursive_directory_iterator walk(directory, std::filesystem::directory_options::skip_permission_denied, error);
    for (; !error && walk != std::filesystem::recursive_directory_iterator(); walk.increment(error)) {
        if (++entries > MaxCacheEntries) break;
        std::error_code sizeError;
        if (walk->is_regular_file(sizeError) && !sizeError) {
            const auto size = walk->file_size(sizeError);
            if (!sizeError) bytes += size;
        }
    }
    facts.sizeMiB = bytes / (1024 * 1024);
    return facts;
}

std::vector<std::string> environmentEntries() {
    std::vector<std::string> entries;
#ifdef _WIN32
    char** variables = _environ;
#else
    char** variables = environ;
#endif
    for (; variables != nullptr && *variables != nullptr; ++variables) entries.emplace_back(*variables);
    return entries;
}

std::string variable(const char* name) {
    const char* value = std::getenv(name);
    return value != nullptr ? value : "";
}

std::vector<std::string> redactions() {
#ifdef _WIN32
    const auto user = variable("USERNAME");
    const auto host = variable("COMPUTERNAME");
    const auto home = variable("USERPROFILE");
#else
    const auto user = variable("USER").empty() ? variable("LOGNAME") : variable("USER");
    char buffer[256]{};
    const auto host = ::gethostname(buffer, sizeof buffer - 1) == 0 ? std::string(buffer) : std::string();
    const auto home = variable("HOME");
#endif
    return HostRedactionNames(user, host, home);
}

}

HostSources SystemHostSources() {
    HostSources sources;
    sources.commit = APS5_TRACE_COMMIT;
    sources.os = operatingSystem;
    sources.cpuModel = cpuModel;
    sources.cpuThreads = [] { return static_cast<std::uint64_t>(std::thread::hardware_concurrency()); };
    sources.memoryMiB = physicalMemoryMiB;
    sources.cpuMissing = cpuMissing;
    sources.sdlLinked = sdlLinked;
    sources.sdlCompiled = sdlCompiled;
    sources.shaderCache = shaderCache;
    sources.environment = environmentEntries;
    sources.redactions = redactions;
    return sources;
}

}
