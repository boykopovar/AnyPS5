#include "prx/libSceAddressSanitizer/src/Globals.hpp"
#include "prx/libSceAddressSanitizer/src/Container.hpp"
#include "prx/libSceAddressSanitizer/src/ErrorReport.hpp"
#include "prx/libSceAddressSanitizer/src/Report.hpp"
#include "prx/libSceAddressSanitizer/src/ShadowRequire.hpp"
#include <cstdio>
#include <limits>
#include <mutex>
#include <vector>

namespace AsanRuntime::Globals {

namespace {

constexpr std::uint8_t kGlobalRedzoneMagic = 0xf9;
constexpr std::uintptr_t kMinimalDistanceFromAnotherGlobal = 64;
constexpr std::uintptr_t kHighMemoryEnd = 1ull << 47;

struct Entry {
    const Global* source;
    Global global;
};

struct Registry {
    std::recursive_mutex mutex;
    std::vector<Entry> entries;
};

Registry& GetRegistry() {
    static Registry registry;
    return registry;
}

bool InMemory(std::uintptr_t begin, std::uintptr_t size) {
    const std::uintptr_t end = begin + size;
    return end >= begin && end <= kHighMemoryEnd && (end <= ShadowBase || begin >= ShadowBase + ShadowSize);
}

std::string Location(const Global& global) {
    if (global.location) {
        std::string text = global.location->filename ? global.location->filename : global.moduleName;
        if (global.location->lineNo) {
            text += ":" + std::to_string(global.location->lineNo);
        }
        if (global.location->columnNo) {
            text += ":" + std::to_string(global.location->columnNo);
        }
        return text;
    }
    return global.moduleName ? global.moduleName : "";
}

std::string Name(const Global& global) {
    return global.name ? global.name : "";
}

void PoisonRedzones(const Global& global) {
    const std::uintptr_t alignedSize = RoundUp_nid_no_patch(global.size);
    PoisonShadow_nid_no_patch(global.beg + alignedSize, global.sizeWithRedzone - alignedSize, kGlobalRedzoneMagic);
    if (global.size != alignedSize) {
        *ShadowByte_nid_no_patch(global.beg + RoundDown_nid_no_patch(global.size)) = static_cast<std::uint8_t>(global.size % kGranularity);
    }
}

[[noreturn]] void ReportOdrViolation(const Global& first, const Global& second) {
    std::string text = ReportSeparator_nid_no_patch();
    text += "ERROR: AddressSanitizer: odr-violation (";
    AppendPointer_nid_no_patch(text, first.beg);
    text += "):\n  [1] size=" + std::to_string(static_cast<std::intptr_t>(first.size)) + " '" + Name(first) + "' " + Location(first) + "\n";
    text += "  [2] size=" + std::to_string(static_cast<std::intptr_t>(second.size)) + " '" + Name(second) + "' " + Location(second) + "\n";
    text += "HINT: if you don't care about these errors you may set ASAN_OPTIONS=detect_odr_violation=0\n";
    text += "SUMMARY: AddressSanitizer: odr-violation: global '" + Name(first) + "' at " + Location(first) + "\n";
    ReportFatal_nid_no_patch({false, 0, 0, 0, first.beg, false, 0, "odr-violation", true}, text);
}

void CheckOdrViolationViaIndicator(const Registry& registry, const Global& global) {
    if (global.odrIndicator == std::numeric_limits<std::uintptr_t>::max()) {
        return;
    }
    auto* indicator = reinterpret_cast<std::uint8_t*>(global.odrIndicator);
    if (*indicator == 0) {
        *indicator = 1;
        return;
    }
    for (auto it = registry.entries.rbegin(); it != registry.entries.rend(); ++it) {
        if (global.odrIndicator == it->global.odrIndicator) {
            ReportOdrViolation(global, it->global);
        }
    }
}

void CheckOdrViolationViaPoisoning(const Registry& registry, const Global& global) {
    RequireShadowRange_nid_no_patch(global.beg, global.sizeWithRedzone, __func__);
    const auto* shadow = reinterpret_cast<const std::int8_t*>(ShadowBase);
    if (!FindFirstPoisoned_nid_no_patch(shadow, ShadowSize, global.beg, global.sizeWithRedzone)) {
        return;
    }
    for (auto it = registry.entries.rbegin(); it != registry.entries.rend(); ++it) {
        if (global.beg == it->global.beg) {
            ReportOdrViolation(global, it->global);
        }
    }
}

void RegisterGlobal(Registry& registry, const Global* global) {
    if (!InMemory(global->beg, global->sizeWithRedzone)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    if (!Aligned_nid_no_patch(global->beg)) {
        std::string text = "The following global variable is not properly aligned.\n"
                           "This may happen if another global with the same name\n"
                           "resides in another non-instrumented module.\n"
                           "Or the global comes from a C file built w/o -fno-common.\n"
                           "In either case this is likely an ODR violation bug,\n"
                           "but AddressSanitizer can not provide more details.\n";
        EmitDiagnostic_nid_no_patch(text);
        ReportOdrViolation(*global, *global);
    }
    if (!Aligned_nid_no_patch(global->sizeWithRedzone) || global->sizeWithRedzone < global->size) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadowRange_nid_no_patch(global->beg, global->sizeWithRedzone, __func__);
    if (global->odrIndicator > 0) {
        CheckOdrViolationViaIndicator(registry, *global);
    } else {
        CheckOdrViolationViaPoisoning(registry, *global);
    }
    PoisonRedzones(*global);
    registry.entries.push_back({global, *global});
}

void UnregisterGlobal(Registry& registry, const Global* global) {
    if (!InMemory(global->beg, global->sizeWithRedzone) || !Aligned_nid_no_patch(global->beg) || !Aligned_nid_no_patch(global->sizeWithRedzone)) {
        InvalidArgument_nid_no_patch(__func__);
    }
    RequireShadowRange_nid_no_patch(global->beg, global->sizeWithRedzone, __func__);
    PoisonShadow_nid_no_patch(global->beg, global->sizeWithRedzone, 0);
    if (global->odrIndicator > 0 && global->odrIndicator != std::numeric_limits<std::uintptr_t>::max()) {
        *reinterpret_cast<std::uint8_t*>(global->odrIndicator) = 0;
    }
    for (auto it = registry.entries.begin(); it != registry.entries.end(); ++it) {
        if (it->source == global) {
            registry.entries.erase(it);
            break;
        }
    }
}

bool IsNear(std::uintptr_t address, const Global& global) {
    return address + kMinimalDistanceFromAnotherGlobal > global.beg && address < global.beg + global.sizeWithRedzone;
}

std::string AsciiLine(const Global& global) {
    if (global.size == 0) {
        return "";
    }
    const auto* bytes = reinterpret_cast<const unsigned char*>(global.beg);
    for (std::uintptr_t i = 0; i + 1 < global.size; ++i) {
        if (bytes[i] == 0 || bytes[i] > 0x7f) {
            return "";
        }
    }
    if (bytes[global.size - 1] != 0) {
        return "";
    }
    return "  '" + Name(global) + "' is ascii string '" + reinterpret_cast<const char*>(bytes) + "'\n";
}

}  // namespace

void Register_nid_no_patch(Global* globals, std::uintptr_t count) {
    if (count == 0) {
        return;
    }
    if (!globals || count > std::numeric_limits<std::uintptr_t>::max() / sizeof(Global) || !Aligned_nid_no_patch(reinterpret_cast<std::uintptr_t>(globals)) || !InMemory(reinterpret_cast<std::uintptr_t>(globals), count * sizeof(Global))) {
        InvalidArgument_nid_no_patch(__func__);
    }
    Registry& registry = GetRegistry();
    std::lock_guard lock(registry.mutex);
    for (std::uintptr_t i = 0; i < count; ++i) {
        RegisterGlobal(registry, &globals[i]);
    }
    RequireShadowRange_nid_no_patch(reinterpret_cast<std::uintptr_t>(globals), count * sizeof(Global), __func__);
    PoisonShadow_nid_no_patch(reinterpret_cast<std::uintptr_t>(globals), count * sizeof(Global), kGlobalRedzoneMagic);
}

void Unregister_nid_no_patch(Global* globals, std::uintptr_t count) {
    if (count == 0) {
        return;
    }
    if (!globals || count > std::numeric_limits<std::uintptr_t>::max() / sizeof(Global) || !Aligned_nid_no_patch(reinterpret_cast<std::uintptr_t>(globals)) || !InMemory(reinterpret_cast<std::uintptr_t>(globals), count * sizeof(Global))) {
        InvalidArgument_nid_no_patch(__func__);
    }
    Registry& registry = GetRegistry();
    std::lock_guard lock(registry.mutex);
    for (std::uintptr_t i = 0; i < count; ++i) {
        UnregisterGlobal(registry, &globals[i]);
    }
    RequireShadowRange_nid_no_patch(reinterpret_cast<std::uintptr_t>(globals), count * sizeof(Global), __func__);
    PoisonShadow_nid_no_patch(reinterpret_cast<std::uintptr_t>(globals), count * sizeof(Global), 0);
}

void RegisterRange_nid_no_patch(std::uintptr_t* flag, void* start, void* stop) {
    if (*flag || !start) {
        return;
    }
    const auto bytes = reinterpret_cast<std::uintptr_t>(stop) - reinterpret_cast<std::uintptr_t>(start);
    if (bytes % sizeof(Global) != 0) {
        InvalidArgument_nid_no_patch(__func__);
    }
    Register_nid_no_patch(static_cast<Global*>(start), bytes / sizeof(Global));
    *flag = 1;
}

void UnregisterRange_nid_no_patch(std::uintptr_t* flag, void* start, void* stop) {
    if (!*flag || !start) {
        return;
    }
    const auto bytes = reinterpret_cast<std::uintptr_t>(stop) - reinterpret_cast<std::uintptr_t>(start);
    if (bytes % sizeof(Global) != 0) {
        InvalidArgument_nid_no_patch(__func__);
    }
    Unregister_nid_no_patch(static_cast<Global*>(start), bytes / sizeof(Global));
    *flag = 0;
}

std::size_t ForAddress_nid_no_patch(std::uintptr_t address, Global* out) {
    Registry& registry = GetRegistry();
    std::lock_guard lock(registry.mutex);
    std::size_t found = 0;
    for (auto it = registry.entries.rbegin(); it != registry.entries.rend() && found < kMaxDescribed; ++it) {
        if (IsNear(address, it->global)) {
            out[found++] = it->global;
        }
    }
    return found;
}

std::string Describe_nid_no_patch(std::uintptr_t address, std::size_t accessSize) {
    Global globals[kMaxDescribed];
    const std::size_t count = ForAddress_nid_no_patch(address, globals);
    std::string text;
    for (std::size_t i = 0; i < count; ++i) {
        const Global& global = globals[i];
        std::uintptr_t reported = address;
        std::string relation;
        std::uintptr_t distance = 0;
        if (address < global.beg) {
            relation = "before";
            distance = global.beg - address;
        } else if (address + accessSize > global.beg + global.size) {
            if (reported < global.beg + global.size) {
                reported = global.beg + global.size;
            }
            relation = "after";
            distance = reported - (global.beg + global.size);
        } else {
            relation = "inside of";
            distance = address - global.beg;
        }
        AppendPointer_nid_no_patch(text, reported);
        text += " is located " + std::to_string(static_cast<std::intptr_t>(distance)) + " bytes " + relation + " global variable '" + Name(global) + "' defined in '" + Location(global) + "' (0x";
        char buffer[40];
        std::snprintf(buffer, sizeof(buffer), "%llx", static_cast<unsigned long long>(global.beg));
        text += buffer;
        text += ") of size " + std::to_string(global.size) + "\n";
        text += AsciiLine(global);
    }
    return text;
}

}  // namespace AsanRuntime::Globals
