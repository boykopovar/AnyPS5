#include "prx/libSceAgcDriver/Execution/include/HostReport.hpp"
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <string_view>

namespace AgcDriver {

namespace {

bool nameCharacter(unsigned char value) {
    return std::isalnum(value) != 0 || value == '_';
}

std::string replaceNames(std::string text, const std::vector<std::string>& names) {
    for (const auto& name : names) {
        if (name.empty()) continue;
        std::string result;
        std::size_t cursor = 0;
        for (;;) {
            const auto found = text.find(name, cursor);
            if (found == std::string::npos) break;
            const auto end = found + name.size();
            const bool before = found == 0 || !nameCharacter(static_cast<unsigned char>(text[found - 1]));
            const bool after = end == text.size() || !nameCharacter(static_cast<unsigned char>(text[end]));
            result.append(text, cursor, found - cursor);
            if (before && after) result += "<redacted>";
            else result.append(text, found, name.size());
            cursor = end;
        }
        result.append(text, cursor, std::string::npos);
        text = std::move(result);
    }
    return text;
}

bool hardwareAddress(const std::string& token) {
    if (token.size() != 17) return false;
    for (std::size_t index = 0; index < token.size(); ++index) {
        const auto value = static_cast<unsigned char>(token[index]);
        if (index % 3 == 2) {
            if (value != ':' && value != '-') return false;
        } else if (std::isxdigit(value) == 0) {
            return false;
        }
    }
    return true;
}

std::string tokenPlaceholder(const std::string& token) {
    auto begin = token.find_first_not_of("(\"'[,;");
    auto end = token.find_last_not_of(")\"']',;");
    if (begin == std::string::npos || end == std::string::npos || end < begin) return token;
    const auto core = token.substr(begin, end - begin + 1);
    if (core.find("://") != std::string::npos) return "<url>";
    if (hardwareAddress(core)) return "<redacted>";
    const bool drive = core.size() >= 3 && std::isalpha(static_cast<unsigned char>(core[0])) != 0 && core[1] == ':' && (core[2] == '\\' || core[2] == '/');
    if (core[0] == '/' || core[0] == '~' || core.find('\\') != std::string::npos || drive) return "<path>";
    return token;
}

HostFacts sanitized(const HostFacts& facts) {
    const auto clean = [&](const std::string& text) { return RedactHostText(text, facts.redactions); };
    const auto cleanAll = [&](std::vector<std::string> values) {
        for (auto& value : values) value = clean(value);
        return values;
    };
    HostFacts result = facts;
    result.redactions.clear();
    result.commit = clean(facts.commit);
    if (facts.os) result.os = clean(*facts.os);
    if (facts.cpuModel) result.cpuModel = clean(*facts.cpuModel);
    if (facts.cpuMissing) result.cpuMissing = cleanAll(*facts.cpuMissing);
    if (facts.sdlLinked) result.sdlLinked = clean(*facts.sdlLinked);
    if (facts.sdlCompiled) result.sdlCompiled = clean(*facts.sdlCompiled);
    if (facts.gpu) {
        auto& gpu = *result.gpu;
        gpu.name = clean(gpu.name);
        gpu.type = clean(gpu.type);
        gpu.driverName = clean(gpu.driverName);
        gpu.driverInfo = clean(gpu.driverInfo);
        gpu.candidates = cleanAll(gpu.candidates);
        gpu.presentModes = cleanAll(gpu.presentModes);
        gpu.requestedPresentMode = clean(gpu.requestedPresentMode);
        for (auto& requirement : gpu.requirements) {
            requirement.name = clean(requirement.name);
            requirement.kind = clean(requirement.kind);
        }
    }
    result.unavailable = cleanAll(facts.unavailable);
    for (auto& entry : result.environment) {
        entry.first = clean(entry.first);
        entry.second = clean(entry.second);
    }
    return result;
}

std::string hex(std::uint32_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "0x%08x", value);
    return buffer;
}

std::string hex16(std::uint32_t value) {
    char buffer[16];
    std::snprintf(buffer, sizeof buffer, "0x%04x", value);
    return buffer;
}

std::string vulkanVersion(std::uint32_t version) {
    return std::to_string((version >> 22) & 0x7Fu) + "." + std::to_string((version >> 12) & 0x3FFu) + "." + std::to_string(version & 0xFFFu);
}

std::string join(const std::vector<std::string>& values) {
    std::string text;
    for (const auto& value : values) {
        if (!text.empty()) text += ", ";
        text += value;
    }
    return text;
}

void line(std::string& output, const std::string& key, const std::string& value) {
    output += key;
    output += ": ";
    output += value;
    output += '\n';
}

void optionalLine(std::string& output, const std::string& key, const std::optional<std::string>& value) {
    line(output, key, value ? *value : std::string("unavailable"));
}

std::optional<std::string> text(const std::optional<std::uint64_t>& value) {
    if (!value) return std::nullopt;
    return std::to_string(*value);
}

}

namespace {

template <typename TValue>
std::optional<TValue> gather(const std::function<TValue()>& source, const char* name, std::vector<std::string>& unavailable) {
    if (!source) {
        unavailable.push_back(std::string(name) + ": no source");
        return std::nullopt;
    }
    try {
        return source();
    } catch (const std::exception& error) {
        unavailable.push_back(std::string(name) + ": " + error.what());
    }
    return std::nullopt;
}

}

HostFacts CollectHostFacts(const HostSources& sources) {
    HostFacts facts;
    facts.commit = sources.commit;
    facts.redactions = gather(sources.redactions, "redactions", facts.unavailable).value_or(std::vector<std::string>{});
    facts.os = gather(sources.os, "os", facts.unavailable);
    facts.cpuModel = gather(sources.cpuModel, "cpu", facts.unavailable);
    facts.cpuThreads = gather(sources.cpuThreads, "cpu_threads", facts.unavailable);
    facts.memoryMiB = gather(sources.memoryMiB, "memory", facts.unavailable);
    facts.cpuMissing = gather(sources.cpuMissing, "cpu_gate", facts.unavailable);
    facts.sdlLinked = gather(sources.sdlLinked, "sdl_linked", facts.unavailable);
    facts.sdlCompiled = gather(sources.sdlCompiled, "sdl_compiled", facts.unavailable);
    facts.shaderCache = gather(sources.shaderCache, "shader_cache", facts.unavailable);
    facts.environment = FilterHostEnvironment(gather(sources.environment, "environment", facts.unavailable).value_or(std::vector<std::string>{}));
    return facts;
}

std::vector<std::pair<std::string, std::string>> FilterHostEnvironment(const std::vector<std::string>& entries) {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto& entry : entries) {
        const auto equals = entry.find('=');
        if (equals == std::string::npos || equals == 0) continue;
        const auto name = entry.substr(0, equals);
        for (const char* prefix : {"APS5_", "ANYPS5_", "VK_", "SDL_"}) {
            if (name.rfind(prefix, 0) == 0) {
                result.emplace_back(name, entry.substr(equals + 1));
                break;
            }
        }
    }
    std::sort(result.begin(), result.end());
    return result;
}

std::vector<std::string> HostRedactionNames(const std::string& user, const std::string& hostName, const std::string& homeDirectory) {
    std::vector<std::string> names;
    const auto add = [&](const std::string& name) {
        if (!name.empty() && std::find(names.begin(), names.end(), name) == names.end()) names.push_back(name);
    };
    add(user);
    add(hostName);
    auto home = homeDirectory;
    while (!home.empty() && (home.back() == '/' || home.back() == '\\')) home.pop_back();
    const auto separator = home.find_last_of("/\\");
    add(separator == std::string::npos ? home : home.substr(separator + 1));
    return names;
}

std::vector<HostRequirement> MissingRequirements(const HostGpuFacts& gpu) {
    std::vector<HostRequirement> missing;
    for (const auto& requirement : gpu.requirements)
        if (requirement.required && !requirement.present) missing.push_back(requirement);
    std::sort(missing.begin(), missing.end(), [](const auto& left, const auto& right) {
        if (left.kind != right.kind) return left.kind < right.kind;
        return left.name < right.name;
    });
    return missing;
}

std::string DescribeMissingRequirements(const HostGpuFacts& gpu, const std::vector<std::string>& redactions) {
    const auto missing = MissingRequirements(gpu);
    if (missing.empty()) return {};
    std::string lacks;
    for (const auto& requirement : missing) {
        if (!lacks.empty()) lacks += ", ";
        lacks += requirement.name + " (" + requirement.kind + ")";
    }
    std::string driver = gpu.driverName.empty() ? std::string("unknown driver") : gpu.driverName;
    if (!gpu.driverInfo.empty()) driver += " " + gpu.driverInfo;
    std::string text = "Vulkan: " + gpu.name + " (" + driver + ", Vulkan " + vulkanVersion(gpu.apiVersion) + ") lacks " + std::to_string(missing.size()) +
        (missing.size() == 1 ? " item" : " items") + " AnyPS5 needs: " + lacks + ". Update the graphics driver; an older GPU or driver may not offer them.";
    std::vector<std::string> others;
    for (const auto& candidate : gpu.candidates)
        if (candidate != gpu.name) others.push_back(candidate);
    if (!others.empty()) text += " Other Vulkan devices on this machine: " + join(others) + "; set ANYPS5_GPU to a part of one name to run on it.";
    text += " Start again with ANYPS5_HOST_REPORT=1 and attach the report to the issue.";
    return RedactHostText(text, redactions);
}

bool ParseHostReportSetting(const char* value) {
    const std::string_view text = value != nullptr ? value : "";
    if (text.empty() || text == "0") return false;
    if (text == "1") return true;
    throw std::runtime_error("ANYPS5_HOST_REPORT must be 0 or 1, not '" + std::string(text) + "'");
}

std::string RedactHostText(const std::string& text, const std::vector<std::string>& names) {
    std::string flat = text;
    for (auto& character : flat) {
        const auto value = static_cast<unsigned char>(character);
        if (value < 0x20 || value == 0x7f) character = ' ';
    }
    const auto named = replaceNames(flat, names);
    std::string result;
    std::size_t cursor = 0;
    while (cursor <= named.size()) {
        const auto space = named.find(' ', cursor);
        const auto stop = space == std::string::npos ? named.size() : space;
        result += tokenPlaceholder(named.substr(cursor, stop - cursor));
        if (space == std::string::npos) break;
        result += ' ';
        cursor = space + 1;
    }
    return result;
}

std::string FormatHostReport(const HostFacts& original) {
    const HostFacts facts = sanitized(original);
    std::string output = "AnyPS5 host report (format 1)\n";
    line(output, "commit", facts.commit.empty() ? "unknown" : facts.commit);
    output += "[system]\n";
    optionalLine(output, "os", facts.os);
    optionalLine(output, "cpu", facts.cpuModel);
    optionalLine(output, "cpu_threads", text(facts.cpuThreads));
    optionalLine(output, "memory_mib", text(facts.memoryMiB));
    if (facts.cpuMissing) line(output, "cpu_gate", facts.cpuMissing->empty() ? "passed" : "failed, lacks " + join(*facts.cpuMissing));
    else line(output, "cpu_gate", "unavailable");
    output += "[sdl]\n";
    optionalLine(output, "linked", facts.sdlLinked);
    optionalLine(output, "compiled", facts.sdlCompiled);
    output += "[gpu]\n";
    if (!facts.gpu) {
        line(output, "device", "none selected");
    } else {
        const auto& gpu = *facts.gpu;
        line(output, "device", gpu.name);
        line(output, "type", gpu.type);
        line(output, "vendor_id", hex16(gpu.vendorId));
        line(output, "device_id", hex16(gpu.deviceId));
        line(output, "vulkan_api", vulkanVersion(gpu.apiVersion));
        line(output, "driver", gpu.driverName.empty() ? "unknown" : gpu.driverName);
        line(output, "driver_info", gpu.driverInfo.empty() ? "unknown" : gpu.driverInfo);
        line(output, "driver_version_raw", hex(gpu.driverVersion));
        line(output, "subgroup_size", std::to_string(gpu.subgroupSize));
        line(output, "device_local_mib", std::to_string(gpu.deviceLocalMiB));
        line(output, "candidates", gpu.candidates.empty() ? "none" : join(gpu.candidates));
        auto presentModes = gpu.presentModes;
        std::sort(presentModes.begin(), presentModes.end());
        line(output, "present_modes", presentModes.empty() ? "unknown" : join(presentModes));
        line(output, "present_mode_requested", gpu.requestedPresentMode.empty() ? "default" : gpu.requestedPresentMode);
        output += "[vulkan]\n";
        auto requirements = gpu.requirements;
        std::sort(requirements.begin(), requirements.end(), [](const auto& left, const auto& right) {
            if (left.required != right.required) return left.required;
            if (left.kind != right.kind) return left.kind < right.kind;
            return left.name < right.name;
        });
        for (const auto& requirement : requirements) {
            line(output, requirement.name, std::string(requirement.kind) + " " + (requirement.required ? "required" : "optional") + " " + (requirement.present ? "present" : "missing"));
        }
    }
    output += "[shader_cache]\n";
    if (!facts.shaderCache) {
        line(output, "state", "unavailable");
    } else if (!facts.shaderCache->enabled) {
        line(output, "state", "disabled");
    } else if (!facts.shaderCache->present) {
        line(output, "state", "absent");
    } else {
        line(output, "state", "present");
        line(output, "size_mib", std::to_string(facts.shaderCache->sizeMiB));
    }
    output += "[environment]\n";
    auto environment = facts.environment;
    std::sort(environment.begin(), environment.end());
    if (environment.empty()) output += "none\n";
    for (const auto& entry : environment) line(output, entry.first, entry.second);
    if (!facts.unavailable.empty()) {
        auto unavailable = facts.unavailable;
        std::sort(unavailable.begin(), unavailable.end());
        output += "[unavailable]\n";
        for (const auto& item : unavailable) output += item + "\n";
    }
    return output;
}

}
