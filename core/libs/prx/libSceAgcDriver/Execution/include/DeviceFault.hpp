#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DEVICEFAULT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DEVICEFAULT_HPP

#ifndef VK_NO_PROTOTYPES
#define VK_NO_PROTOTYPES
#endif
#include <vulkan/vulkan.h>
#include "SubmissionSnapshots.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace AgcDriver {

struct DeviceDiagnostics {
    bool config = false;
    bool checkpoints = false;
    VkDeviceDiagnosticsConfigFlagsNV flags = 0;
};

inline DeviceDiagnostics ChooseDeviceDiagnostics(bool requested, bool configSupported, bool checkpointsSupported) {
    if (!requested || !configSupported) return {};
    return {true, checkpointsSupported, VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_DEBUG_INFO_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_RESOURCE_TRACKING_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_AUTOMATIC_CHECKPOINTS_BIT_NV | VK_DEVICE_DIAGNOSTICS_CONFIG_ENABLE_SHADER_ERROR_REPORTING_BIT_NV};
}

inline const char* DeviceFaultAddressKind(VkDeviceFaultAddressTypeEXT type) {
    switch (type) {
    case VK_DEVICE_FAULT_ADDRESS_TYPE_NONE_EXT: return "none";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_READ_INVALID_EXT: return "read invalid";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_WRITE_INVALID_EXT: return "write invalid";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_EXECUTE_INVALID_EXT: return "execute invalid";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_UNKNOWN_EXT: return "instruction pointer unknown";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_INVALID_EXT: return "instruction pointer invalid";
    case VK_DEVICE_FAULT_ADDRESS_TYPE_INSTRUCTION_POINTER_FAULT_EXT: return "instruction pointer fault";
    default: return "unknown";
    }
}

inline std::string DeviceFaultText(const char (&text)[VK_MAX_DESCRIPTION_SIZE]) {
    return std::string(text, strnlen(text, VK_MAX_DESCRIPTION_SIZE));
}

inline std::uint64_t DeviceFaultRangeLow(std::uint64_t address, std::uint64_t precision) {
    return precision == 0 ? address : address & ~(precision - 1);
}

inline std::uint64_t DeviceFaultRangeHigh(std::uint64_t address, std::uint64_t precision) {
    return precision == 0 ? address : address | (precision - 1);
}

inline const char* DeviceFaultRangePermissions(std::uint32_t permissions) {
    constexpr std::uint32_t readWrite = ShaderRecompiler::BdaAbi::Read | ShaderRecompiler::BdaAbi::Write;
    if ((permissions & readWrite) == readWrite) return "read write";
    if ((permissions & ShaderRecompiler::BdaAbi::Write) != 0) return "write";
    if ((permissions & ShaderRecompiler::BdaAbi::Read) != 0) return "read";
    return "none";
}

inline std::vector<std::string> DeviceFaultRangeLines(VkDeviceFaultAddressTypeEXT type, std::uint64_t address, std::uint64_t precision, const std::vector<ShaderRecompiler::BdaAbi::Range>& snapshot, std::uint64_t submitSerial, std::uint64_t spaceSerial) {
    std::vector<std::string> lines;
    char line[320];
    const std::uint64_t low = DeviceFaultRangeLow(address, precision);
    const std::uint64_t high = DeviceFaultRangeHigh(address, precision);
    const ShaderRecompiler::BdaAbi::Range* owner = nullptr;
    const ShaderRecompiler::BdaAbi::Range* before = nullptr;
    const ShaderRecompiler::BdaAbi::Range* after = nullptr;
    for (const auto& range : snapshot) {
        if (owner == nullptr && range.begin <= low && high < range.end) owner = &range;
        if (range.end <= low && (before == nullptr || before->end < range.end)) before = &range;
        if (range.begin > high && (after == nullptr || range.begin < after->begin)) after = &range;
    }
    if (owner != nullptr) {
        std::snprintf(line, sizeof(line), "[device-fault] %s at 0x%llx (precision 0x%llx) in guest range [0x%llx, 0x%llx) %s (batch %llu, space %llu)", DeviceFaultAddressKind(type), static_cast<unsigned long long>(address), static_cast<unsigned long long>(precision), static_cast<unsigned long long>(owner->begin), static_cast<unsigned long long>(owner->end), DeviceFaultRangePermissions(owner->permissions), static_cast<unsigned long long>(submitSerial), static_cast<unsigned long long>(spaceSerial));
        lines.emplace_back(line);
        return lines;
    }
    std::snprintf(line, sizeof(line), "[device-fault] %s at 0x%llx (precision 0x%llx) has no guest owner in batch %llu (internal or unmapped)", DeviceFaultAddressKind(type), static_cast<unsigned long long>(address), static_cast<unsigned long long>(precision), static_cast<unsigned long long>(submitSerial));
    lines.emplace_back(line);
    if (before != nullptr) {
        std::snprintf(line, sizeof(line), "[device-fault] previous guest range [0x%llx, 0x%llx) %s ends 0x%llx before the fault range", static_cast<unsigned long long>(before->begin), static_cast<unsigned long long>(before->end), DeviceFaultRangePermissions(before->permissions), static_cast<unsigned long long>(low - before->end));
        lines.emplace_back(line);
    }
    if (after != nullptr) {
        std::snprintf(line, sizeof(line), "[device-fault] next guest range [0x%llx, 0x%llx) %s starts 0x%llx after the fault range", static_cast<unsigned long long>(after->begin), static_cast<unsigned long long>(after->end), DeviceFaultRangePermissions(after->permissions), static_cast<unsigned long long>(after->begin - high));
        lines.emplace_back(line);
    }
    return lines;
}

inline std::vector<std::string> DeviceFaultBatchLines(VkDeviceFaultAddressTypeEXT type, std::uint64_t address, std::uint64_t precision, std::uint64_t submitSerial, const SubmissionSnapshots& snapshots) {
    if (submitSerial == 0) return {};
    const auto found = snapshots.Find(submitSerial);
    if (!found.has_value()) {
        char line[128];
        std::snprintf(line, sizeof(line), "[device-fault] no guest snapshot for batch %llu", static_cast<unsigned long long>(submitSerial));
        return {line};
    }
    return DeviceFaultRangeLines(type, address, precision, *found->ranges, found->submitSerial, found->spaceSerial);
}

inline constexpr std::uint32_t DeviceFaultBracketTokensListed = 8;
inline constexpr std::uint32_t DeviceFaultMaxInfos = 64;
inline constexpr std::uint32_t DeviceFaultMaxLiveResources = 4;
inline constexpr std::size_t DeviceFaultLiveResourceCapacity = 128;

class LiveResourceRing {
public:
    static LiveResourceRing& Instance() noexcept {
        static LiveResourceRing store;
        return store;
    }
    LiveResourceRing() = default;
    LiveResourceRing(const LiveResourceRing&) = delete;
    LiveResourceRing& operator=(const LiveResourceRing&) = delete;
    void Note(std::uint64_t address, std::uint64_t size, const char* kind) noexcept {
        try {
            if (address == 0 || size == 0 || kind == nullptr || *kind == '\0') return;
            std::lock_guard<std::mutex> lock(mutex);
            const std::uint64_t slot = next.fetch_add(1, std::memory_order_relaxed) % DeviceFaultLiveResourceCapacity;
            Entry& entry = entries[static_cast<std::size_t>(slot)];
            std::size_t length = 0;
            while (length + 1 < sizeof(entry.kind) && kind[length] != '\0') {
                entry.kind[length] = kind[length];
                ++length;
            }
            entry.kind[length] = '\0';
            entry.size.store(size, std::memory_order_relaxed);
            entry.address.store(address, std::memory_order_release);
        } catch (...) {
        }
    }
    std::vector<std::string> LinesFor(std::uint64_t va) const noexcept {
        std::vector<std::string> lines;
        try {
            struct Candidate {
                std::uint64_t distance;
                std::uint64_t address;
                std::uint64_t size;
                char kind[16];
            };
            Candidate found[DeviceFaultLiveResourceCapacity];
            std::size_t count = 0;
            {
                std::lock_guard<std::mutex> lock(mutex);
                for (std::size_t i = 0; i < DeviceFaultLiveResourceCapacity; ++i) {
                    const std::uint64_t address = entries[i].address.load(std::memory_order_acquire);
                    if (address == 0) continue;
                    const std::uint64_t size = entries[i].size.load(std::memory_order_relaxed);
                    if (size == 0) continue;
                    std::uint64_t distance = 0;
                    if (va < address) distance = address - va;
                    else if (va - address >= size) distance = va - address - size;
                    char kind[16]{};
                    const char* stored = entries[i].kind;
                    std::size_t length = 0;
                    while (length + 1 < sizeof(kind) && stored[length] != '\0') {
                        kind[length] = stored[length];
                        ++length;
                    }
                    kind[length] = '\0';
                    found[count].distance = distance;
                    found[count].address = address;
                    found[count].size = size;
                    for (std::size_t k = 0; k < sizeof(kind); ++k) found[count].kind[k] = kind[k];
                    ++count;
                }
            }
            for (std::size_t i = 0; i < count; ++i) {
                for (std::size_t j = i + 1; j < count; ++j) {
                    if (found[j].distance < found[i].distance) {
                        Candidate tmp = found[i];
                        found[i] = found[j];
                        found[j] = tmp;
                    }
                }
            }
            const std::size_t listed = count < DeviceFaultMaxLiveResources ? count : DeviceFaultMaxLiveResources;
            for (std::size_t i = 0; i < listed; ++i) {
                char line[128];
                std::snprintf(line, sizeof(line), "[device-fault] live resource 0x%llx size 0x%llx %s", static_cast<unsigned long long>(found[i].address), static_cast<unsigned long long>(found[i].size), found[i].kind);
                lines.emplace_back(line);
            }
        } catch (...) {
        }
        return lines;
    }
private:
    struct Entry {
        std::atomic<std::uint64_t> address{0};
        std::atomic<std::uint64_t> size{0};
        char kind[16]{};
    };
    std::atomic<std::uint64_t> next{0};
    Entry entries[DeviceFaultLiveResourceCapacity]{};
    mutable std::mutex mutex;
};

inline void DeviceFaultAppendStage(char* line, std::size_t size, std::uint32_t stage) noexcept {
    const std::size_t used = strnlen(line, size);
    if (used + 1 >= size) return;
    if (stage == static_cast<std::uint32_t>(VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT)) std::snprintf(line + used, size - used, " observed at bottom-of-pipe");
    else std::snprintf(line + used, size - used, " at stage 0x%x", static_cast<unsigned>(stage));
}

inline std::vector<std::string> DeviceFaultBracketLines(const SubmitBracket& bracket, const FaultTokenRing& ring, VkQueue queue = VK_NULL_HANDLE) noexcept {
    std::vector<std::string> lines;
    try {
        if (bracket.submitSerial == 0) return lines;
        char line[256];
        if (queue == VK_NULL_HANDLE) std::snprintf(line, sizeof(line), "[device-fault] bracket: last completed %llu, in flight %llu, first not started %llu", static_cast<unsigned long long>(bracket.lastCompleted), static_cast<unsigned long long>(bracket.submitSerial), static_cast<unsigned long long>(bracket.submitSerial + 1));
        else std::snprintf(line, sizeof(line), "[device-fault] bracket: last completed %llu, in flight %llu, first not started %llu, queue 0x%llx", static_cast<unsigned long long>(bracket.lastCompleted), static_cast<unsigned long long>(bracket.submitSerial), static_cast<unsigned long long>(bracket.submitSerial + 1), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(queue)));
        lines.emplace_back(line);
        if (bracket.firstToken == 0) return lines;
        if (bracket.lastToken < bracket.firstToken) return lines;
        const std::uint64_t tokens = static_cast<std::uint64_t>(bracket.lastToken) - static_cast<std::uint64_t>(bracket.firstToken) + 1;
        const std::uint64_t listed = tokens < DeviceFaultBracketTokensListed ? tokens : DeviceFaultBracketTokensListed;
        const std::uint64_t start = static_cast<std::uint64_t>(bracket.firstToken) + (tokens > listed ? tokens - listed : 0);
        for (std::uint64_t i = 0; i < listed; ++i) {
            const FaultToken token = static_cast<FaultToken>(start + i);
            const auto found = ring.Find(token);
            if (!found.has_value()) {
                if (queue == VK_NULL_HANDLE) std::snprintf(line, sizeof(line), "[device-fault] unknown token %u in batch %llu", static_cast<unsigned>(token), static_cast<unsigned long long>(bracket.submitSerial));
                else std::snprintf(line, sizeof(line), "[device-fault] unknown token %u in batch %llu queue 0x%llx", static_cast<unsigned>(token), static_cast<unsigned long long>(bracket.submitSerial), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(queue)));
                lines.emplace_back(line);
                continue;
            }
            const std::size_t descLength = strnlen(found->desc, sizeof(found->desc));
            if (queue == VK_NULL_HANDLE) {
                if (descLength == 0) std::snprintf(line, sizeof(line), "[device-fault] token %u in batch %llu: program 0x%llx variant %llu", static_cast<unsigned>(token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId));
                else std::snprintf(line, sizeof(line), "[device-fault] token %u in batch %llu: program 0x%llx variant %llu %.*s", static_cast<unsigned>(token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId), static_cast<int>(descLength), found->desc);
            } else if (descLength == 0) std::snprintf(line, sizeof(line), "[device-fault] token %u in batch %llu queue 0x%llx: program 0x%llx variant %llu", static_cast<unsigned>(token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(queue)), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId));
            else std::snprintf(line, sizeof(line), "[device-fault] token %u in batch %llu queue 0x%llx: program 0x%llx variant %llu %.*s", static_cast<unsigned>(token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(queue)), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId), static_cast<int>(descLength), found->desc);
            lines.emplace_back(line);
        }
        if (tokens > listed) {
            std::snprintf(line, sizeof(line), "[device-fault] %llu more bracket tokens", static_cast<unsigned long long>(tokens - listed));
            lines.emplace_back(line);
        }
    } catch (...) {
    }
    return lines;
}

inline std::vector<std::string> DeviceFaultBatchLines(VkDeviceFaultAddressTypeEXT type, std::uint64_t address, std::uint64_t precision, std::uint64_t submitSerial, const SubmissionSnapshots& snapshots, const SubmitBracket& bracket, const FaultTokenRing& ring, VkQueue queue = VK_NULL_HANDLE) noexcept {
    std::vector<std::string> lines;
    try {
        lines = DeviceFaultBatchLines(type, address, precision, submitSerial, snapshots);
        const auto window = DeviceFaultBracketLines(bracket, ring, queue);
        lines.insert(lines.end(), window.begin(), window.end());
    } catch (...) {
    }
    return lines;
}

inline std::vector<std::string> DeviceFaultReport(VkDevice device, PFN_vkGetDeviceFaultInfoEXT faultInfo, VkQueue queue, PFN_vkGetQueueCheckpointDataNV checkpointData, std::uint64_t submitSerial = 0, const SubmissionSnapshots* snapshots = nullptr) {
    std::vector<std::string> lines;
    char line[VK_MAX_DESCRIPTION_SIZE + 128];
    if (checkpointData != nullptr && queue != VK_NULL_HANDLE) {
        std::uint32_t count = 0;
        checkpointData(queue, &count, nullptr);
        std::vector<VkCheckpointDataNV> points(count, VkCheckpointDataNV{VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV});
        if (count != 0) checkpointData(queue, &count, points.data());
        points.resize(count);
        if (points.empty()) lines.emplace_back("[device-fault] no checkpoint reached");
        for (const auto& point : points) {
            std::snprintf(line, sizeof(line), "[device-fault] checkpoint: program 0x%llx at stage 0x%x", static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(point.pCheckpointMarker)), static_cast<unsigned>(point.stage));
            lines.emplace_back(line);
        }
    }
    if (faultInfo == nullptr || device == VK_NULL_HANDLE) return lines;
    VkDeviceFaultCountsEXT counts{VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
    if (faultInfo(device, &counts, nullptr) != VK_SUCCESS) {
        lines.emplace_back("[device-fault] vkGetDeviceFaultInfoEXT reported nothing");
        return lines;
    }
    std::vector<VkDeviceFaultAddressInfoEXT> addresses(counts.addressInfoCount);
    std::vector<VkDeviceFaultVendorInfoEXT> vendors(counts.vendorInfoCount);
    counts.vendorBinarySize = 0;
    VkDeviceFaultInfoEXT info{VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT};
    info.pAddressInfos = addresses.empty() ? nullptr : addresses.data();
    info.pVendorInfos = vendors.empty() ? nullptr : vendors.data();
    const auto result = faultInfo(device, &counts, &info);
    if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
        lines.emplace_back("[device-fault] vkGetDeviceFaultInfoEXT reported nothing");
        return lines;
    }
    addresses.resize(std::min<std::size_t>(addresses.size(), counts.addressInfoCount));
    vendors.resize(std::min<std::size_t>(vendors.size(), counts.vendorInfoCount));
    std::snprintf(line, sizeof(line), "[device-fault] result %d: %s", static_cast<int>(result), DeviceFaultText(info.description).c_str());
    lines.emplace_back(line);
    for (const auto& address : addresses) {
        std::snprintf(line, sizeof(line), "[device-fault] %s at 0x%llx (precision 0x%llx)", DeviceFaultAddressKind(address.addressType), static_cast<unsigned long long>(address.reportedAddress), static_cast<unsigned long long>(address.addressPrecision));
        lines.emplace_back(line);
        if (snapshots != nullptr) {
            const auto correlated = DeviceFaultBatchLines(address.addressType, address.reportedAddress, address.addressPrecision, submitSerial, *snapshots);
            lines.insert(lines.end(), correlated.begin(), correlated.end());
        }
    }
    for (const auto& vendor : vendors) {
        std::snprintf(line, sizeof(line), "[device-fault] vendor: %s (code 0x%llx, data 0x%llx)", DeviceFaultText(vendor.description).c_str(), static_cast<unsigned long long>(vendor.vendorFaultCode), static_cast<unsigned long long>(vendor.vendorFaultData));
        lines.emplace_back(line);
    }
    return lines;
}

inline std::vector<std::string> DeviceFaultReport(VkDevice device, PFN_vkGetDeviceFaultInfoEXT faultInfo, VkQueue queue, PFN_vkGetQueueCheckpointDataNV checkpointData, std::uint64_t submitSerial, const SubmissionSnapshots* snapshots, const SubmitBracket& bracket, const FaultTokenRing& ring) noexcept {
    std::vector<std::string> lines;
    try {
        std::vector<VkCheckpointDataNV> points;
        std::uint64_t tokenSerial = 0;
        if (checkpointData != nullptr && queue != VK_NULL_HANDLE) {
            std::uint32_t count = 0;
            checkpointData(queue, &count, nullptr);
            if (count > DeviceFaultMaxInfos) count = DeviceFaultMaxInfos;
            points.assign(count, VkCheckpointDataNV{VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV});
            if (count != 0) checkpointData(queue, &count, points.data());
            points.resize(count);
            for (const auto& point : points) {
                const FaultToken token = static_cast<FaultToken>(reinterpret_cast<std::uintptr_t>(point.pCheckpointMarker));
                const auto found = ring.Find(token);
                if (found.has_value() && found->submitSerial != 0) tokenSerial = found->submitSerial;
            }
        }
        const std::uint64_t effectiveSerial = submitSerial != 0 ? submitSerial : tokenSerial;
        SubmitBracket effectiveBracket = bracket;
        if (effectiveBracket.submitSerial == 0 && effectiveSerial != 0 && snapshots != nullptr) {
            if (const auto found = snapshots->FindBracket(effectiveSerial)) effectiveBracket = *found;
        }
        const auto window = DeviceFaultBracketLines(effectiveBracket, ring, queue);
        lines.insert(lines.end(), window.begin(), window.end());
        if (effectiveSerial == 0 && device != VK_NULL_HANDLE) lines.emplace_back("[device-fault] no per-submit marker (submit serial unknown; markers need APS5_GPU_CHECKPOINTS on the main queue)");
        if (checkpointData != nullptr && queue != VK_NULL_HANDLE) {
            if (points.empty()) lines.emplace_back("[device-fault] no checkpoint reached");
            char checkpoint[256];
            for (const auto& point : points) {
                const FaultToken token = static_cast<FaultToken>(reinterpret_cast<std::uintptr_t>(point.pCheckpointMarker));
                const auto found = ring.Find(token);
                if (!found.has_value()) {
                    std::snprintf(checkpoint, sizeof(checkpoint), "[device-fault] checkpoint: unknown token %u queue 0x%llx", static_cast<unsigned>(token), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(queue)));
                } else {
                    const std::size_t descLength = strnlen(found->desc, sizeof(found->desc));
                    if (descLength == 0) std::snprintf(checkpoint, sizeof(checkpoint), "[device-fault] checkpoint: token %u batch %llu queue 0x%llx program 0x%llx variant %llu", static_cast<unsigned>(token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(queue)), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId));
                    else std::snprintf(checkpoint, sizeof(checkpoint), "[device-fault] checkpoint: token %u batch %llu queue 0x%llx program 0x%llx variant %llu %.*s", static_cast<unsigned>(token), static_cast<unsigned long long>(found->submitSerial), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(queue)), static_cast<unsigned long long>(found->programAddress), static_cast<unsigned long long>(found->variantId), static_cast<int>(descLength), found->desc);
                }
                DeviceFaultAppendStage(checkpoint, sizeof(checkpoint), point.stage);
                lines.emplace_back(checkpoint);
            }
        }
        if (faultInfo == nullptr || device == VK_NULL_HANDLE) {
            if (points.empty() && effectiveBracket.submitSerial != 0) lines.emplace_back("[device-fault] no device-side fault data; CPU-side bracket only");
            return lines;
        }
        char line[VK_MAX_DESCRIPTION_SIZE + 128];
        VkDeviceFaultCountsEXT counts{VK_STRUCTURE_TYPE_DEVICE_FAULT_COUNTS_EXT};
        if (faultInfo(device, &counts, nullptr) != VK_SUCCESS) {
            lines.emplace_back("[device-fault] vkGetDeviceFaultInfoEXT reported nothing");
            return lines;
        }
        if (counts.addressInfoCount > DeviceFaultMaxInfos) counts.addressInfoCount = DeviceFaultMaxInfos;
        if (counts.vendorInfoCount > DeviceFaultMaxInfos) counts.vendorInfoCount = DeviceFaultMaxInfos;
        std::vector<VkDeviceFaultAddressInfoEXT> addresses(counts.addressInfoCount);
        std::vector<VkDeviceFaultVendorInfoEXT> vendors(counts.vendorInfoCount);
        counts.vendorBinarySize = 0;
        VkDeviceFaultInfoEXT info{VK_STRUCTURE_TYPE_DEVICE_FAULT_INFO_EXT};
        info.pAddressInfos = addresses.empty() ? nullptr : addresses.data();
        info.pVendorInfos = vendors.empty() ? nullptr : vendors.data();
        const auto result = faultInfo(device, &counts, &info);
        if (result != VK_SUCCESS && result != VK_INCOMPLETE) {
            lines.emplace_back("[device-fault] vkGetDeviceFaultInfoEXT reported nothing");
            return lines;
        }
        addresses.resize(std::min<std::size_t>(addresses.size(), counts.addressInfoCount));
        vendors.resize(std::min<std::size_t>(vendors.size(), counts.vendorInfoCount));
        std::snprintf(line, sizeof(line), "[device-fault] result %d: %s", static_cast<int>(result), DeviceFaultText(info.description).c_str());
        lines.emplace_back(line);
        for (const auto& address : addresses) {
            std::snprintf(line, sizeof(line), "[device-fault] %s at 0x%llx (precision 0x%llx)", DeviceFaultAddressKind(address.addressType), static_cast<unsigned long long>(address.reportedAddress), static_cast<unsigned long long>(address.addressPrecision));
            lines.emplace_back(line);
            if (snapshots != nullptr) {
                const auto correlated = DeviceFaultBatchLines(address.addressType, address.reportedAddress, address.addressPrecision, effectiveSerial, *snapshots);
                lines.insert(lines.end(), correlated.begin(), correlated.end());
            }
            const auto live = LiveResourceRing::Instance().LinesFor(address.reportedAddress);
            lines.insert(lines.end(), live.begin(), live.end());
        }
        for (const auto& vendor : vendors) {
            std::snprintf(line, sizeof(line), "[device-fault] vendor: %s (code 0x%llx, data 0x%llx)", DeviceFaultText(vendor.description).c_str(), static_cast<unsigned long long>(vendor.vendorFaultCode), static_cast<unsigned long long>(vendor.vendorFaultData));
            lines.emplace_back(line);
        }
        if (addresses.empty() && points.empty() && effectiveBracket.submitSerial != 0) lines.emplace_back("[device-fault] no device-side fault data; CPU-side bracket only");
    } catch (...) {
    }
    return lines;
}

}

#endif
