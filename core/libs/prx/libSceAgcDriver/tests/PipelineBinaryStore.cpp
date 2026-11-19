#include "prx/libSceAgcDriver/Graphics/include/PipelineBinaryStore.hpp"
#include "ShaderCacheDirectory.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <random>
#include <thread>
#include <vector>

using namespace AgcDriver::Graphics;

namespace {

unsigned cold = 0;
unsigned restored = 0;
unsigned released = 0;
unsigned legacyCreates = 0;
unsigned captures = 0;
bool reject = false;
bool captureFailure = false;
bool missing = false;
bool unsupported = false;
bool oversized = false;
std::uint8_t pipelineKey = 7;
std::uint8_t global = 1;
std::uintptr_t next = 1;
std::map<VkPipelineBinaryKHR, std::uint8_t> binaries;
std::function<void()> duringCapture;
std::function<void(const VkPipelineCreateInfoKHR*, VkPipelineBinaryKeyKHR*)> keyOutput;
unsigned keyQueries = 0;
unsigned globalQueries = 0;
unsigned pipelineQueries = 0;
unsigned otherBinaryCalls = 0;
unsigned binaryLookups = 0;
unsigned driverQueries = 0;
VkDriverId queriedDriver = VK_DRIVER_ID_NVIDIA_PROPRIETARY;

void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

VKAPI_ATTR void VKAPI_CALL QueryDriver(VkPhysicalDevice physical, VkPhysicalDeviceProperties2* properties) {
    Check(physical == reinterpret_cast<VkPhysicalDevice>(1), "driver identity lost the physical device");
    Check(properties->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, "driver identity used the wrong properties structure");
    auto* driver = static_cast<VkPhysicalDeviceDriverProperties*>(properties->pNext);
    Check(driver != nullptr && driver->sType == VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES && driver->pNext == nullptr, "driver identity used the wrong query chain");
    ++driverQueries;
    driver->driverID = queriedDriver;
}

void DriverIdentityTests() {
    const auto physical = reinterpret_cast<VkPhysicalDevice>(1);
    for (const auto version : {VK_API_VERSION_1_0, VK_API_VERSION_1_1, VK_API_VERSION_1_2, VK_API_VERSION_1_3}) {
        for (const bool extension : {false, true}) {
            driverQueries = 0;
            const bool supported = extension || version >= VK_API_VERSION_1_2;
            const auto identity = PipelineBinaryStore::DriverIdentity(physical, version, extension, QueryDriver);
            Check(identity == (supported ? queriedDriver : VK_DRIVER_ID_MAX_ENUM), "wrong policy driver identity");
            Check(driverQueries == (supported ? 1u : 0u), "unsupported driver properties were queried");
            Check(PipelineBinaryStore::PolicyForDriver(identity) == (supported ? PipelineBinaryPolicy::DisabledByDriver : PipelineBinaryPolicy::Enabled), "discovered identity did not reach the policy");
        }
    }
    queriedDriver = VK_DRIVER_ID_MESA_NVK;
    Check(PipelineBinaryStore::PolicyForDriver(PipelineBinaryStore::DriverIdentity(physical, VK_API_VERSION_1_2, false, QueryDriver)) == PipelineBinaryPolicy::Enabled, "core-only NVK identity was gated");
    Check(PipelineBinaryStore::DriverIdentity(physical, VK_API_VERSION_1_2, false, nullptr) == VK_DRIVER_ID_MAX_ENUM, "missing properties query invented an identity");
    Check(globalQueries == 0 && pipelineQueries == 0 && otherBinaryCalls == 0 && binaryLookups == 0, "identity discovery reached the binary extension");
    std::printf("PASS driver identity: extension/core-only/unknown; no binary calls\n");
}

std::vector<VkLayerProperties> activeLayers;
VkResult layerCountResult = VK_SUCCESS;
VkResult layerDataResult = VK_SUCCESS;
unsigned incompleteLayerReads = 0;

VKAPI_ATTR VkResult VKAPI_CALL EnumerateLayers(VkPhysicalDevice physical, std::uint32_t* count, VkLayerProperties* properties) {
    Check(physical == reinterpret_cast<VkPhysicalDevice>(1), "layer guard lost the physical device");
    if (properties == nullptr) {
        *count = static_cast<std::uint32_t>(activeLayers.size());
        return layerCountResult;
    }
    const auto written = std::min(*count, static_cast<std::uint32_t>(activeLayers.size()));
    std::copy_n(activeLayers.begin(), written, properties);
    *count = written;
    if (incompleteLayerReads != 0) {
        --incompleteLayerReads;
        return VK_INCOMPLETE;
    }
    return layerDataResult;
}

void LayerCompatibilityTests() {
    const auto physical = reinterpret_cast<VkPhysicalDevice>(1);
    Check(PipelineBinaryStore::LayersCompatible(physical, EnumerateLayers), "no active layers disabled pipeline binaries");
    struct Case {
        const char* name;
        std::uint32_t version;
        bool allowed;
    };
    const Case cases[]{
        {"VK_LAYER_KHRONOS_validation", VK_MAKE_API_VERSION(0, 1, 3, 275), false},
        {"VK_LAYER_KHRONOS_validation", VK_MAKE_API_VERSION(0, 1, 3, 293), false},
        {"VK_LAYER_KHRONOS_validation", VK_MAKE_API_VERSION(0, 1, 3, 294), true},
        {"VK_LAYER_KHRONOS_validation", VK_MAKE_API_VERSION(0, 1, 4, 363), true},
        {"VK_LAYER_LUNARG_gfxreconstruct", VK_MAKE_API_VERSION(0, 1, 3, 280), false},
        {"VK_LAYER_VALVE_steam_overlay_64", VK_MAKE_API_VERSION(0, 1, 3, 207), true},
    };
    for (const auto& test : cases) {
        VkLayerProperties layer{};
        std::strcpy(layer.layerName, test.name);
        layer.specVersion = test.version;
        activeLayers = {layer};
        Check(PipelineBinaryStore::LayersCompatible(physical, EnumerateLayers) == test.allowed, test.name);
    }
    activeLayers[0].specVersion = VK_MAKE_API_VERSION(0, 1, 4, 363);
    VkLayerProperties oldValidation{};
    std::strcpy(oldValidation.layerName, "VK_LAYER_KHRONOS_validation");
    oldValidation.specVersion = VK_MAKE_API_VERSION(0, 1, 3, 275);
    activeLayers.push_back(oldValidation);
    Check(!PipelineBinaryStore::LayersCompatible(physical, EnumerateLayers), "mixed layers hid old validation");
    activeLayers.pop_back();
    for (const auto result : {VK_ERROR_INITIALIZATION_FAILED, VK_INCOMPLETE}) {
        layerCountResult = result;
        Check(!PipelineBinaryStore::LayersCompatible(physical, EnumerateLayers), "failed or incomplete layer count was accepted");
        layerCountResult = VK_SUCCESS;
        layerDataResult = result;
        Check(!PipelineBinaryStore::LayersCompatible(physical, EnumerateLayers), "failed or incomplete layer data was accepted");
        layerDataResult = VK_SUCCESS;
    }
    activeLayers.push_back(oldValidation);
    incompleteLayerReads = 1;
    Check(!PipelineBinaryStore::LayersCompatible(physical, EnumerateLayers), "incomplete read hid old validation");
    activeLayers.pop_back();
    incompleteLayerReads = 1;
    Check(PipelineBinaryStore::LayersCompatible(physical, EnumerateLayers), "transient incomplete read disabled compatible layers");
    Check(!PipelineBinaryStore::LayersCompatible(physical, nullptr), "missing layer enumerator was accepted");
}

VKAPI_ATTR VkResult VKAPI_CALL Key(VkDevice device, const VkPipelineCreateInfoKHR* info, VkPipelineBinaryKeyKHR* key) {
    Check(device == reinterpret_cast<VkDevice>(1), "key query lost the device");
    if (info == nullptr) ++globalQueries;
    else ++pipelineQueries;
    if (keyQueries++ == 0) Check(info == nullptr, "first key query must have null create-info");
    Check(key->sType == VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR && key->pNext == nullptr && key->keySize == 0, "key query output was not initialized");
    key->keySize = 1;
    key->key[0] = info == nullptr ? global : pipelineKey;
    if (keyOutput) keyOutput(info, key);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL Create(VkDevice, VkPipelineCache cache, std::uint32_t count, const VkComputePipelineCreateInfo* info, const VkAllocationCallbacks*, VkPipeline* pipeline) {
    Check(count == 1, "expected one pipeline");
    const auto* chain = static_cast<const VkBaseInStructure*>(info->pNext);
    bool capture = false;
    bool restore = false;
    while (chain != nullptr) {
        if (chain->sType == VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR) capture = (reinterpret_cast<const VkPipelineCreateFlags2CreateInfoKHR*>(chain)->flags & VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR) != 0;
        if (chain->sType == VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR) {
            const auto& binary = *reinterpret_cast<const VkPipelineBinaryInfoKHR*>(chain);
            Check(binary.binaryCount == 2, "lost pipeline binary count");
            Check(binaries.at(binary.pPipelineBinaries[0]) == 3 && binaries.at(binary.pPipelineBinaries[1]) == 4, "lost pipeline binary order");
            restore = true;
        }
        chain = chain->pNext;
    }
    if (capture || restore) Check(cache == VK_NULL_HANDLE, "binary pipeline used a legacy cache");
    else {
        Check(cache == reinterpret_cast<VkPipelineCache>(2), "legacy build lost its driver cache");
        ++legacyCreates;
    }
    if (restore && reject) return VK_ERROR_UNKNOWN;
    if (capture) ++captures;
    if (capture && duringCapture) {
        auto callback = std::move(duringCapture);
        duringCapture = {};
        callback();
    }
    if (capture && captureFailure) return VK_ERROR_UNKNOWN;
    if (restore) ++restored;
    else ++cold;
    *pipeline = reinterpret_cast<VkPipeline>(next++);
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL Binary(VkDevice, const VkPipelineBinaryCreateInfoKHR* info, const VkAllocationCallbacks*, VkPipelineBinaryHandlesInfoKHR* output) {
    ++otherBinaryCalls;
    if (missing) return VK_PIPELINE_BINARY_MISSING_KHR;
    if (output->pPipelineBinaries == nullptr) {
        output->pipelineBinaryCount = 2;
        return VK_SUCCESS;
    }
    Check(output->pipelineBinaryCount == 2, "wrong binary capacity");
    for (unsigned i = 0; i < 2; ++i) {
        const auto value = static_cast<std::uint8_t>(3 + i);
        if (info->pKeysAndDataInfo != nullptr) {
            const auto& data = *info->pKeysAndDataInfo;
            Check(data.binaryCount == 2 && data.pPipelineBinaryKeys[i].keySize == 1 && data.pPipelineBinaryKeys[i].key[0] == value, "wrong imported binary key");
            Check(data.pPipelineBinaryData[i].dataSize == 1 && *static_cast<const std::uint8_t*>(data.pPipelineBinaryData[i].pData) == value, "wrong imported binary data");
        }
        output->pPipelineBinaries[i] = reinterpret_cast<VkPipelineBinaryKHR>(next++);
        binaries.emplace(output->pPipelineBinaries[i], value);
    }
    return VK_SUCCESS;
}

VKAPI_ATTR void VKAPI_CALL DestroyBinary(VkDevice, VkPipelineBinaryKHR binary, const VkAllocationCallbacks*) {
    ++otherBinaryCalls;
    Check(binaries.erase(binary) == 1, "unknown binary destroyed");
}

VKAPI_ATTR void VKAPI_CALL DestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL Data(VkDevice, const VkPipelineBinaryDataInfoKHR* info, VkPipelineBinaryKeyKHR* key, std::size_t* size, void* data) {
    key->keySize = 1;
    ++otherBinaryCalls;
    key->key[0] = binaries.at(info->pipelineBinary);
    *size = oversized ? 256u * 1024u * 1024u + 1u : 1;
    if (data != nullptr) *static_cast<std::uint8_t*>(data) = key->key[0];
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL Release(VkDevice, const VkReleaseCapturedPipelineDataInfoKHR*, const VkAllocationCallbacks*) {
    ++otherBinaryCalls;
    ++released;
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice, const char* name) {
    if (std::strstr(name, "KHR") != nullptr) ++binaryLookups;
    if (unsupported && std::strcmp(name, "vkGetPipelineKeyKHR") == 0) return nullptr;
    if (std::strcmp(name, "vkGetPipelineKeyKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Key);
    if (std::strcmp(name, "vkCreateComputePipelines") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Create);
    if (std::strcmp(name, "vkCreatePipelineBinariesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Binary);
    if (std::strcmp(name, "vkDestroyPipelineBinaryKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyBinary);
    if (std::strcmp(name, "vkDestroyPipeline") == 0) return reinterpret_cast<PFN_vkVoidFunction>(DestroyPipeline);
    if (std::strcmp(name, "vkGetPipelineBinaryDataKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Data);
    if (std::strcmp(name, "vkReleaseCapturedPipelineDataKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(Release);
    return nullptr;
}

void Environment(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    if (*value == 0) unsetenv(name);
    else setenv(name, value, 1);
#endif
}

}

int main(int argc, char** argv) try {
    LayerCompatibilityTests();
    const auto root = std::filesystem::temp_directory_path() / ("aps5-binary-test-" + std::to_string(std::random_device{}()));
    std::filesystem::create_directories(root);
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() { std::error_code error; std::filesystem::remove_all(path, error); }
    } cleanup{root};
    Environment("ANYPS5_SHADER_CACHE_DIR", root.string().c_str());
    Environment("ANYPS5_NO_SHADER_CACHE", "0");
    Environment("APS5_NO_PIPELINE_BINARIES", "");
    Environment("APS5_UNSAFE_PIPELINE_BINARIES", "");
    Environment("APS5_PIPELINE_BINARY_MIN_WORDS", "0");
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "0");
    Context context{};
    context.device = reinterpret_cast<VkDevice>(1);
    context.deviceProc = Resolve;
    context.pipelineCache = reinterpret_cast<VkPipelineCache>(2);
    VkPhysicalDeviceProperties properties{};
    properties.vendorID = 1;
    properties.deviceID = 2;
    properties.driverVersion = 3;
    VkDriverId driverId = VK_DRIVER_ID_MESA_RADV;
    std::array<std::uint32_t, 5> spirv{0x07230203, 0x10000, 0, 1, 0};
    std::array<std::uint32_t, 4> layout{1, 2, 3, 4};
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, reinterpret_cast<VkShaderModule>(1), "main", nullptr};
    info.layout = reinterpret_cast<VkPipelineLayout>(1);
    if (argc > 1 && std::string_view(argv[1]) == "--driver-identity") {
        DriverIdentityTests();
        return 0;
    }
    if (argc > 1 && std::string_view(argv[1]).starts_with("--driver-")) {
        const std::string_view mode = argv[1];
        properties.vendorID = 0x10de;
        driverId = mode == "--driver-nvk" ? VK_DRIVER_ID_MESA_NVK : VK_DRIVER_ID_NVIDIA_PROPRIETARY;
        if (mode == "--driver-unsafe" || mode == "--driver-optout") Environment("APS5_UNSAFE_PIPELINE_BINARIES", "1");
        if (mode == "--driver-optout") Environment("APS5_NO_PIPELINE_BINARIES", "1");
        const bool enabled = mode == "--driver-unsafe" || mode == "--driver-nvk";
        const auto exercise = [&] {
            PipelineBinaryStore tested(context, properties, driverId);
            VkPipeline pipeline = VK_NULL_HANDLE;
            tested.CreateComputePipeline(info, spirv, layout, &pipeline);
            Check(pipeline != VK_NULL_HANDLE, "policy fallback did not create a pipeline");
            if (!enabled) {
                std::printf("policy counters: global=%u pipeline=%u binary=%u lookups=%u legacy=%u\n", globalQueries, pipelineQueries, otherBinaryCalls, binaryLookups, legacyCreates);
                Check(globalQueries == 0 && pipelineQueries == 0 && otherBinaryCalls == 0 && binaryLookups == 0, "disabled driver reached the binary extension before the gate");
                Check(!tested.IsReady() && legacyCreates == cold && captures == 0, "disabled driver lost the original pipeline cache");
                const auto expected = mode == "--driver-optout" ? PipelineBinaryPolicy::Disabled : PipelineBinaryPolicy::DisabledByDriver;
                Check(tested.PolicyStatus() == expected, "wrong driver disable status or opt-out precedence");
                Check(PipelineBinaryStore::PolicyForDriver(driverId) == expected, "device enable policy disagrees with the store");
            } else Check(tested.IsReady() && tested.PolicyStatus() == PipelineBinaryPolicy::Enabled, "allowed driver was gated");
        };
        if (mode == "--driver-invalid-optin") {
            for (const auto* value : {"0", "true", "yes", "01", "1 ", " 1", "2", "-1"}) {
                Environment("APS5_UNSAFE_PIPELINE_BINARIES", value);
                exercise();
            }
        } else exercise();
        if (enabled) {
            exercise();
            Check(globalQueries == 2 && pipelineQueries == 2 && otherBinaryCalls != 0 && binaryLookups != 0 && captures == 1 && restored == 1 && released == 1 && binaries.empty(), "allowed driver did not capture and restore through the existing key path");
        }
        std::printf("PASS %s: global=%u pipeline=%u binary=%u lookups=%u legacy=%u\n", argv[1], globalQueries, pipelineQueries, otherBinaryCalls, binaryLookups, legacyCreates);
        return 0;
    }
    if (argc > 1) {
        const std::string mode = argv[1];
        const bool perPipeline = mode.starts_with("--pipeline-");
        alignas(PipelineBinaryStore) std::byte storage[sizeof(PipelineBinaryStore)];
        keyOutput = [&](const VkPipelineCreateInfoKHR* query, VkPipelineBinaryKeyKHR* output) {
            if ((query != nullptr) != perPipeline) return;
            if (mode.ends_with("zero")) output->keySize = 0;
            else if (mode.ends_with("oversized")) output->keySize = 33;
            else if (mode.ends_with("maximum")) output->keySize = VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR;
            else if (mode.ends_with("stype")) output->sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            else if (mode.ends_with("pnext")) output->pNext = output;
            else if (mode == "--global-guard") {
                const auto address = reinterpret_cast<std::uintptr_t>(output);
                const auto begin = reinterpret_cast<std::uintptr_t>(storage);
                Check(address < begin || address >= begin + sizeof(storage), "global key output aliases the store instead of scratch");
                auto& scratch = *reinterpret_cast<PipelineBinaryKeyOutput*>(output);
                scratch.guard[0] = 0;
                scratch.guard.back() = 0;
            } else Check(false, "unknown key query test");
        };
        const auto destroy = [](PipelineBinaryStore* store) { std::destroy_at(store); };
        std::unique_ptr<PipelineBinaryStore, decltype(destroy)> tested(std::construct_at(reinterpret_cast<PipelineBinaryStore*>(storage), context, properties, driverId), destroy);
        const auto key = tested->Key(info, spirv, layout);
        const bool maximum = mode.ends_with("maximum");
        Check(maximum ? !key.empty() : key.empty(), maximum ? "maximum key was rejected" : "unusable key was accepted");
        if (!perPipeline && !maximum) {
            VkPipeline pipeline = VK_NULL_HANDLE;
            tested->CreateComputePipeline(info, spirv, layout, &pipeline);
            Check(pipeline != VK_NULL_HANDLE && legacyCreates == 1 && captures == 0, "rejected global key did not use the driver pipeline cache");
        }
        std::printf("PASS %s: %u initialized key queries; %s\n", argv[1], keyQueries, maximum ? "maximum key accepted" : "unusable key rejected");
        return 0;
    }
    PipelineBinaryStore store(context, properties, driverId);
    const auto key = store.Key(info, spirv, layout);
    Check(!key.empty() && store.Key(info, spirv, layout) == key, "unstable recipe key");
    ++spirv[2];
    Check(store.Key(info, spirv, layout) != key, "SPIR-V is absent from key");
    --spirv[2];
    ++layout[0];
    Check(store.Key(info, spirv, layout) != key, "layout is absent from key");
    --layout[0];
    info.flags = VK_PIPELINE_CREATE_DISABLE_OPTIMIZATION_BIT;
    Check(store.Key(info, spirv, layout) != key, "flags are absent from key");
    info.flags = 0;
    info.stage.flags = VK_PIPELINE_SHADER_STAGE_CREATE_REQUIRE_FULL_SUBGROUPS_BIT;
    Check(store.Key(info, spirv, layout) != key, "stage flags are absent from key");
    info.stage.flags = 0;
    info.stage.pName = "other";
    Check(store.Key(info, spirv, layout) != key, "entry point is absent from key");
    info.stage.pName = "main";
    VkSpecializationMapEntry entry{7, 0, 4};
    std::uint32_t value = 1;
    VkSpecializationInfo spec{1, &entry, sizeof(value), &value};
    info.stage.pSpecializationInfo = &spec;
    const auto specialized = store.Key(info, spirv, layout);
    Check(specialized != key, "specialization is absent from key");
    ++value;
    Check(store.Key(info, spirv, layout) != specialized, "specialization data is absent from key");
    --value;
    ++entry.constantID;
    Check(store.Key(info, spirv, layout) != specialized, "specialization ID is absent from key");
    info.stage.pSpecializationInfo = nullptr;
    VkPipelineShaderStageRequiredSubgroupSizeCreateInfo subgroup{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO, nullptr, 32};
    info.stage.pNext = &subgroup;
    const auto grouped = store.Key(info, spirv, layout);
    Check(grouped != key, "subgroup size is absent from key");
    subgroup.requiredSubgroupSize = 64;
    Check(store.Key(info, spirv, layout) != grouped, "subgroup size change is absent from key");
    info.stage.pNext = nullptr;
    VkBaseInStructure unknown{VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr};
    info.pNext = &unknown;
    Check(store.Key(info, spirv, layout).empty(), "unknown pipeline chain accepted");
    info.pNext = nullptr;
    info.stage.pNext = &unknown;
    Check(store.Key(info, spirv, layout).empty(), "unknown stage chain accepted");
    info.stage.pNext = nullptr;
    VkPipeline pipeline = VK_NULL_HANDLE;
    store.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 1 && restored == 0 && released == 1 && binaries.empty(), "cold capture failed or leaked");
    {
        PipelineBinaryStore reopened(context, properties, driverId);
        reopened.CreateComputePipeline(info, spirv, layout, &pipeline);
    }
    Check(cold == 1 && restored == 1 && binaries.empty(), "store did not survive recreation");
    reject = true;
    store.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 2 && binaries.empty(), "rejected binary did not fall back");
    reject = false;
    missing = true;
    store.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 3 && binaries.empty(), "missing binary did not fall back");
    missing = false;
    ++global;
    PipelineBinaryStore incompatible(context, properties, driverId);
    Check(incompatible.Key(info, spirv, layout) != key, "global device key is absent");
    incompatible.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 4, "incompatible global key reused a binary");
    ++properties.driverVersion;
    PipelineBinaryStore newDriver(context, properties, driverId);
    Check(newDriver.Key(info, spirv, layout) != incompatible.Key(info, spirv, layout), "driver fingerprint is absent");
    for (const auto& file : std::filesystem::recursive_directory_iterator(root)) {
        if (!file.is_regular_file()) continue;
        std::fstream stream(file.path(), std::ios::in | std::ios::out | std::ios::binary);
        stream.put('X');
    }
    store.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 5, "corrupt binary did not fall back");
    captureFailure = true;
    ++spirv[2];
    store.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 6, "failed capture did not retry without capture");
    captureFailure = false;
    ++spirv[2];
    Environment("APS5_NO_PIPELINE_BINARIES", "1");
    PipelineBinaryStore disabled(context, properties, driverId);
    disabled.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 7 && binaries.empty(), "opt-out did not use legacy build");
    Environment("APS5_NO_PIPELINE_BINARIES", "");
    unsupported = true;
    PipelineBinaryStore unavailable(context, properties, driverId);
    unavailable.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 8 && binaries.empty(), "unsupported extension did not use legacy build");
    unsupported = false;
    Environment("APS5_PIPELINE_BINARY_MIN_WORDS", "100");
    PipelineBinaryStore small(context, properties, driverId);
    small.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 9 && released == 5, "size threshold captured a small pipeline");
    Environment("APS5_PIPELINE_BINARY_MIN_WORDS", "0");
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "100000");
    PipelineBinaryStore fast(context, properties, driverId);
    fast.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 10 && released == 6 && binaries.empty(), "time threshold did not release capture data");
    const auto filesBefore = std::distance(std::filesystem::recursive_directory_iterator(root), std::filesystem::recursive_directory_iterator{});
    const auto legacyBefore = legacyCreates;
    PipelineBinaryStore fastReopened(context, properties, driverId);
    fastReopened.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(legacyCreates == legacyBefore + 1 && released == 6, "time-threshold rejection did not survive recreation and use the driver cache");
    const auto filesAfter = std::distance(std::filesystem::recursive_directory_iterator(root), std::filesystem::recursive_directory_iterator{});
    Check(cold == 11 && filesBefore == filesAfter, "time threshold persisted a fast pipeline");
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "0");
    PipelineBinaryStore retryThreshold(context, properties, driverId);
    retryThreshold.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 12 && released == 7 && binaries.empty(), "lowering the time threshold did not retry capture");
    const auto beforePipelineKey = store.Key(info, spirv, layout);
    ++pipelineKey;
    Check(store.Key(info, spirv, layout) != beforePipelineKey, "Vulkan pipeline key is absent");
    properties.vendorID += 1;
    PipelineBinaryStore newVendor(context, properties, driverId);
    Check(newVendor.Key(info, spirv, layout) != fast.Key(info, spirv, layout), "vendor fingerprint is absent");
    properties.deviceID += 1;
    PipelineBinaryStore newDevice(context, properties, driverId);
    Check(newDevice.Key(info, spirv, layout) != newVendor.Key(info, spirv, layout), "device fingerprint is absent");
    properties.pipelineCacheUUID[0] = 1;
    PipelineBinaryStore newUuid(context, properties, driverId);
    Check(newUuid.Key(info, spirv, layout) != newDevice.Key(info, spirv, layout), "UUID fingerprint is absent");
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "100000");
    PipelineBinaryStore pendingMarker(context, properties, driverId);
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "0");
    PipelineBinaryStore competingCapture(context, properties, driverId);
    duringCapture = [&] {
        VkPipeline competingPipeline = VK_NULL_HANDLE;
        competingCapture.CreateComputePipeline(info, spirv, layout, &competingPipeline);
        Check(competingPipeline != VK_NULL_HANDLE, "competing capture did not create a pipeline");
    };
    pendingMarker.CreateComputePipeline(info, spirv, layout, &pipeline);
    const auto restoredBeforeMarker = restored;
    const auto coldBeforeMarker = cold;
    for (const auto threshold : {"100000", "200000"}) {
        Environment("APS5_PIPELINE_BINARY_MIN_MS", threshold);
        PipelineBinaryStore afterMarker(context, properties, driverId);
        afterMarker.CreateComputePipeline(info, spirv, layout, &pipeline);
    }
    Check(restored == restoredBeforeMarker + 2 && cold == coldBeforeMarker && binaries.empty(), "pending threshold marker erased a competing captured binary");
    bool failuresRemembered = true;
    for (const auto failure : {"extraction failure", "oversized binary", "rejected capture build"}) {
        ++spirv[2];
        Environment("APS5_PIPELINE_BINARY_MIN_MS", "1");
        PipelineBinaryStore failing(context, properties, driverId);
        const auto recipe = failing.Key(info, spirv, layout);
        char name[32];
        std::snprintf(name, sizeof(name), "%016llx.bin", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(recipe)));
        const auto binaryFile = root / "driver" / "binaries" / name;
        missing = std::strcmp(failure, "extraction failure") == 0;
        oversized = std::strcmp(failure, "oversized binary") == 0;
        captureFailure = std::strcmp(failure, "rejected capture build") == 0;
        const auto capturesBefore = captures;
        duringCapture = [] { std::this_thread::sleep_for(std::chrono::milliseconds(5)); };
        failing.CreateComputePipeline(info, spirv, layout, &pipeline);
        Check(pipeline != VK_NULL_HANDLE && captures == capturesBefore + 1 && !std::filesystem::exists(binaryFile) && binaries.empty(), "failure case did not build without a stored binary");
        missing = false;
        oversized = false;
        captureFailure = false;
        const auto legacyBeforeFailure = legacyCreates;
        PipelineBinaryStore reopenedFailure(context, properties, driverId);
        reopenedFailure.CreateComputePipeline(info, spirv, layout, &pipeline);
        const bool ordinary = pipeline != VK_NULL_HANDLE && legacyCreates == legacyBeforeFailure + 1 && captures == capturesBefore + 1;
        Environment("APS5_PIPELINE_BINARY_MIN_MS", "0");
        PipelineBinaryStore loweredFailure(context, properties, driverId);
        loweredFailure.CreateComputePipeline(info, spirv, layout, &pipeline);
        const bool loweredOrdinary = pipeline != VK_NULL_HANDLE && legacyCreates == legacyBeforeFailure + 2 && captures == capturesBefore + 1;
        std::fprintf(stderr, "%s: %s survives recreation and a lowered time threshold\n", ordinary && loweredOrdinary ? "PASS" : "FAIL", failure);
        failuresRemembered = failuresRemembered && ordinary && loweredOrdinary;
    }
    Check(failuresRemembered, "capture failures did not retain the ordinary driver cache path");
    std::puts("pipeline binary keys, round-trip, compatibility, corruption, rejection and fallback passed");
} catch (const std::exception& error) {
    std::fprintf(stderr, "%s\n", error.what());
    return 1;
}
