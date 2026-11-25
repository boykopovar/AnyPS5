#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libSceResourceArbitrator/include/InventoryExports.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr int kErrorUnavailable = static_cast<int>(0x819b0001u);
constexpr int kErrorNotInitialized = static_cast<int>(0x819b0002u);
constexpr int kErrorInvalidArg = static_cast<int>(0x819b0003u);
constexpr std::uint32_t kMaxFeatureEntries = 5u;
constexpr std::size_t kFeatureCountOffset = 4u;
constexpr std::size_t kFeatureEntriesOffset = 0x18u;
constexpr std::size_t kFeatureEntryStride = 0x20u;

void* g_ipmiClient = nullptr;
void* g_sharedMemory = nullptr;

int RequireIpmiClient(const char* funcName) {
    if (g_ipmiClient == nullptr) {
        return kErrorNotInitialized;
    }
    NotImplemented_nid_no_patch(funcName);
}

int RequireSharedMemory(const char* funcName) {
    if (g_sharedMemory == nullptr) {
        return kErrorUnavailable;
    }
    NotImplemented_nid_no_patch(funcName);
}

} // namespace

extern "C" {

APS5_DUMMY_FUN

}

extern "C" {

int APS5_VABI sceResourceArbitratorChangeState(std::uint32_t id, const char* name, void* arg, std::uint32_t flags) {
    (void)id;
    (void)name;
    (void)arg;
    (void)flags;
    return RequireIpmiClient(__func__);
}

int APS5_VABI sceResourceArbitratorClearCallback(std::uint32_t id) {
    (void)id;
    return RequireIpmiClient(__func__);
}

int APS5_VABI sceResourceArbitratorGetFeatureIdList(void* outList) {
    (void)outList;
    return RequireSharedMemory(__func__);
}

int APS5_VABI sceResourceArbitratorIsExecutable(std::uint32_t id, const char* name) {
    (void)id;
    (void)name;
    return RequireIpmiClient(__func__);
}

int APS5_VABI sceResourceArbitratorSetCallback(std::uint32_t id, void* callback, void* userArg) {
    (void)id;
    (void)callback;
    (void)userArg;
    return RequireIpmiClient(__func__);
}

int APS5_VABI libSceResourceArbitratorUnknown_3xRoVp7fPPY_nid_no_patch() {
    return RequireIpmiClient(__func__);
}
APS5_EXPORT("3xRoVp7fPPY", libSceResourceArbitratorUnknown_3xRoVp7fPPY_nid_no_patch);

int APS5_VABI sceResourceArbitratorPostTelemetry(std::uint32_t id, std::uint32_t type, const char* data, std::uint32_t size) {
    (void)id;
    (void)type;
    (void)data;
    (void)size;
    return RequireIpmiClient(__func__);
}

int APS5_VABI libSceResourceArbitratorUnknown_OM6SXhDZdiU_nid_no_patch(std::uint32_t value, std::uint32_t id) {
    (void)value;
    (void)id;
    return RequireSharedMemory(__func__);
}
APS5_EXPORT("OM6SXhDZdiU", libSceResourceArbitratorUnknown_OM6SXhDZdiU_nid_no_patch);

int APS5_VABI libSceResourceArbitratorUnknown_TYiIrKTzQ8s_nid_no_patch(std::uint8_t flag) {
    (void)flag;
    return 0;
}
APS5_EXPORT("TYiIrKTzQ8s", libSceResourceArbitratorUnknown_TYiIrKTzQ8s_nid_no_patch);

int APS5_VABI libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch(const void* featureList, const char* featureId) {
    if (featureList == nullptr || featureId == nullptr) {
        return 0;
    }
    const auto* base = static_cast<const unsigned char*>(featureList);
    std::uint32_t count = 0;
    std::memcpy(&count, base + kFeatureCountOffset, sizeof(count));
    if (count == 0u) {
        return 0;
    }
    const std::uint32_t limit = count < kMaxFeatureEntries ? count : kMaxFeatureEntries;
    for (std::uint32_t index = 0; index < limit; ++index) {
        const char* entry = reinterpret_cast<const char*>(
            base + kFeatureEntriesOffset + static_cast<std::size_t>(index) * kFeatureEntryStride);
        if (std::strncmp(entry, featureId, kFeatureEntryStride) == 0) {
            return 1;
        }
    }
    return 0;
}
APS5_EXPORT("cT1vA0DTMcs", libSceResourceArbitratorUnknown_cT1vA0DTMcs_nid_no_patch);

int APS5_VABI libSceResourceArbitratorUnknown_jzKwHzhEBIQ_nid_no_patch(std::uint32_t id) {
    (void)id;
    return RequireIpmiClient(__func__);
}
APS5_EXPORT("jzKwHzhEBIQ", libSceResourceArbitratorUnknown_jzKwHzhEBIQ_nid_no_patch);

int APS5_VABI sceResourceArbitratorSendDebugFunction(std::uint32_t command) {
    (void)command;
    return RequireIpmiClient(__func__);
}

int APS5_VABI libSceResourceArbitratorUnknown_rMJnXXj6GIo_nid_no_patch(std::int32_t id, std::int32_t* outIndex, std::int32_t* outValue) {
    (void)outIndex;
    (void)outValue;
    if (id == -1) {
        return kErrorInvalidArg;
    }
    return RequireSharedMemory(__func__);
}
APS5_EXPORT("rMJnXXj6GIo", libSceResourceArbitratorUnknown_rMJnXXj6GIo_nid_no_patch);

int APS5_VABI libSceResourceArbitratorUnknown_ru9qkt9UCLQ_nid_no_patch(std::uint32_t id, void* outBuffer) {
    (void)id;
    (void)outBuffer;
    return RequireSharedMemory(__func__);
}
APS5_EXPORT("ru9qkt9UCLQ", libSceResourceArbitratorUnknown_ru9qkt9UCLQ_nid_no_patch);

}
