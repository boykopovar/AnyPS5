#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Mutex.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include <charconv>
#include <cstdlib>
#include <functional>
#include <optional>
#include <string_view>

namespace AgcDriver::DriverDetail {

namespace {

std::optional<std::set<std::uint64_t>> selectedPrograms() {
    const char* value = std::getenv("APS5_DUMP_SHADERS");
    if (value == nullptr || std::string_view(value) == "1") return std::nullopt;
    std::set<std::uint64_t> programs;
    std::string_view rest(value);
    while (!rest.empty()) {
        const auto comma = rest.find(',');
        auto token = rest.substr(0, comma);
        rest = comma == std::string_view::npos ? std::string_view{} : rest.substr(comma + 1);
        if (token.starts_with("0x") || token.starts_with("0X")) token.remove_prefix(2);
        std::uint64_t address = 0;
        const auto [end, error] = std::from_chars(token.data(), token.data() + token.size(), address, 16);
        if (token.empty() || error != std::errc{} || end != token.data() + token.size()) return std::nullopt;
        programs.insert(address);
    }
    if (programs.empty()) return std::nullopt;
    return programs;
}

}

void Driver::reportSkip(const char* kind, const std::string& what) {
    static AgcDriver::Mutex reportedMutex;
    static std::set<std::size_t> reported;
    static std::map<std::string, int> requestDumps;

    static const bool traceSkips = std::getenv("APS5_TRACE_SKIPS") != nullptr;
    static const int dumpLimit = [] { const char* text = std::getenv("APS5_SKIP_REQUEST_DUMPS"); return text ? std::atoi(text) : 8; }();
    static constexpr std::string_view marker = "\nRecompileRequest:\n";
    std::lock_guard lock(reportedMutex);
    std::string line;
    static bool announced = false;
    if (!announced) {
        announced = true;
        char text[160];
        if (dumpLimit < 0) {
            std::snprintf(text, sizeof(text), "[gpu] skip reports: one write each, RecompileRequest dumps uncapped (APS5_SKIP_REQUEST_DUMPS)\n");
        } else {
            std::snprintf(text, sizeof(text), "[gpu] skip reports: one write each, RecompileRequest dumps capped at %d per reason (APS5_SKIP_REQUEST_DUMPS)\n", dumpLimit);
        }
        line = text;
    }
    const std::string prefix = "[gpu] skipped " + std::string(kind);
    if (reported.insert(std::hash<std::string>{}(what)).second) {
        const auto request = what.find(marker);
        if (request == std::string::npos) {
            line += prefix + ": " + what + "\n";
        } else {
            const auto reason = what.substr(0, request);
            const auto suffix = what.find(' ', request + marker.size());
            auto& dumps = requestDumps[reason];
            if (dumpLimit < 0 || dumps < dumpLimit) {
                ++dumps;
                line += prefix + ": " + what + "\n";
            } else {
                if (dumps == dumpLimit) {
                    ++dumps;
                    line += prefix + ": further RecompileRequest dumps for \"" + reason + "\" are left out\n";
                }
                line += prefix + ": " + reason + " (request left out)" + (suffix == std::string::npos ? std::string() : what.substr(suffix)) + "\n";
            }
        }
    } else if (traceSkips) {
        line += prefix + " again: " + what.substr(0, 100) + "\n";
    }
    if (!line.empty()) std::fwrite(line.data(), 1, line.size(), stderr);
}

std::string Driver::dumpRequest(std::uint64_t address, const ShaderRecompiler::RecompileRequest& request, std::string_view suffix) {
    static AgcDriver::Mutex dumpMutex;
    static std::set<std::string> dumped;
    char hex[32];
    std::snprintf(hex, sizeof(hex), "%llx", static_cast<unsigned long long>(address));
    const std::string name = "shader_" + std::string(hex) + std::string(suffix) + ".req";
    if (const auto selected = selectedPrograms(); selected.has_value() && !selected->contains(address)) return {};
    std::lock_guard lock(dumpMutex);
    if (!dumped.insert(name).second) return name;
    try {
        const auto text = ShaderRecompiler::RequestSerializer{}.Serialize(request);
        if (std::FILE* file = std::fopen(name.c_str(), "wb")) {
            const auto written = std::fwrite(text.data(), 1, text.size(), file);
            const auto closed = std::fclose(file);
            if (written != text.size() || closed != 0) {
                dumped.erase(name);
                std::fprintf(stderr, "[gpu] incomplete write to dump file %s (%zu of %zu bytes, close status %d)\n", name.c_str(), written, text.size(), closed);
            }
        } else {
            dumped.erase(name);
            std::fprintf(stderr, "[gpu] could not open dump file %s for writing\n", name.c_str());
        }
    } catch (const std::exception& error) {
        dumped.erase(name);
        std::fprintf(stderr, "[gpu] could not serialize request for 0x%llx: %s\n", static_cast<unsigned long long>(address), error.what());
    }
    return name;
}

}
