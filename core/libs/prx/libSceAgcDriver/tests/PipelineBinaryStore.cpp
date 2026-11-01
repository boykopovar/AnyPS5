#include "prx/libSceAgcDriver/Graphics/include/PipelineBinaryStore.hpp"
#include "ShaderCacheDirectory.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <map>
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

void Check(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
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

VKAPI_ATTR VkResult VKAPI_CALL Key(VkDevice, const VkPipelineCreateInfoKHR* info, VkPipelineBinaryKeyKHR* key) {
    key->keySize = 1;
    key->key[0] = info == nullptr ? global : pipelineKey;
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
    Check(binaries.erase(binary) == 1, "unknown binary destroyed");
}

VKAPI_ATTR void VKAPI_CALL DestroyPipeline(VkDevice, VkPipeline, const VkAllocationCallbacks*) {}

VKAPI_ATTR VkResult VKAPI_CALL Data(VkDevice, const VkPipelineBinaryDataInfoKHR* info, VkPipelineBinaryKeyKHR* key, std::size_t* size, void* data) {
    key->keySize = 1;
    key->key[0] = binaries.at(info->pipelineBinary);
    *size = oversized ? 256u * 1024u * 1024u + 1u : 1;
    if (data != nullptr) *static_cast<std::uint8_t*>(data) = key->key[0];
    return VK_SUCCESS;
}

VKAPI_ATTR VkResult VKAPI_CALL Release(VkDevice, const VkReleaseCapturedPipelineDataInfoKHR*, const VkAllocationCallbacks*) {
    ++released;
    return VK_SUCCESS;
}

VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL Resolve(VkDevice, const char* name) {
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

int main() try {
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
    std::array<std::uint32_t, 5> spirv{0x07230203, 0x10000, 0, 1, 0};
    std::array<std::uint32_t, 4> layout{1, 2, 3, 4};
    VkComputePipelineCreateInfo info{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
    info.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, reinterpret_cast<VkShaderModule>(1), "main", nullptr};
    info.layout = reinterpret_cast<VkPipelineLayout>(1);
    PipelineBinaryStore store(context, properties);
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
        PipelineBinaryStore reopened(context, properties);
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
    PipelineBinaryStore incompatible(context, properties);
    Check(incompatible.Key(info, spirv, layout) != key, "global device key is absent");
    incompatible.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 4, "incompatible global key reused a binary");
    ++properties.driverVersion;
    PipelineBinaryStore newDriver(context, properties);
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
    PipelineBinaryStore disabled(context, properties);
    disabled.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 7 && binaries.empty(), "opt-out did not use legacy build");
    Environment("APS5_NO_PIPELINE_BINARIES", "");
    unsupported = true;
    PipelineBinaryStore unavailable(context, properties);
    unavailable.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 8 && binaries.empty(), "unsupported extension did not use legacy build");
    unsupported = false;
    Environment("APS5_PIPELINE_BINARY_MIN_WORDS", "100");
    PipelineBinaryStore small(context, properties);
    small.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 9 && released == 5, "size threshold captured a small pipeline");
    Environment("APS5_PIPELINE_BINARY_MIN_WORDS", "0");
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "100000");
    PipelineBinaryStore fast(context, properties);
    fast.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 10 && released == 6 && binaries.empty(), "time threshold did not release capture data");
    const auto filesBefore = std::distance(std::filesystem::recursive_directory_iterator(root), std::filesystem::recursive_directory_iterator{});
    const auto legacyBefore = legacyCreates;
    PipelineBinaryStore fastReopened(context, properties);
    fastReopened.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(legacyCreates == legacyBefore + 1 && released == 6, "time-threshold rejection did not survive recreation and use the driver cache");
    const auto filesAfter = std::distance(std::filesystem::recursive_directory_iterator(root), std::filesystem::recursive_directory_iterator{});
    Check(cold == 11 && filesBefore == filesAfter, "time threshold persisted a fast pipeline");
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "0");
    PipelineBinaryStore retryThreshold(context, properties);
    retryThreshold.CreateComputePipeline(info, spirv, layout, &pipeline);
    Check(cold == 12 && released == 7 && binaries.empty(), "lowering the time threshold did not retry capture");
    const auto beforePipelineKey = store.Key(info, spirv, layout);
    ++pipelineKey;
    Check(store.Key(info, spirv, layout) != beforePipelineKey, "Vulkan pipeline key is absent");
    properties.vendorID += 1;
    PipelineBinaryStore newVendor(context, properties);
    Check(newVendor.Key(info, spirv, layout) != fast.Key(info, spirv, layout), "vendor fingerprint is absent");
    properties.deviceID += 1;
    PipelineBinaryStore newDevice(context, properties);
    Check(newDevice.Key(info, spirv, layout) != newVendor.Key(info, spirv, layout), "device fingerprint is absent");
    properties.pipelineCacheUUID[0] = 1;
    PipelineBinaryStore newUuid(context, properties);
    Check(newUuid.Key(info, spirv, layout) != newDevice.Key(info, spirv, layout), "UUID fingerprint is absent");
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "100000");
    PipelineBinaryStore pendingMarker(context, properties);
    Environment("APS5_PIPELINE_BINARY_MIN_MS", "0");
    PipelineBinaryStore competingCapture(context, properties);
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
        PipelineBinaryStore afterMarker(context, properties);
        afterMarker.CreateComputePipeline(info, spirv, layout, &pipeline);
    }
    Check(restored == restoredBeforeMarker + 2 && cold == coldBeforeMarker && binaries.empty(), "pending threshold marker erased a competing captured binary");
    bool failuresRemembered = true;
    for (const auto failure : {"extraction failure", "oversized binary", "rejected capture build"}) {
        ++spirv[2];
        Environment("APS5_PIPELINE_BINARY_MIN_MS", "1");
        PipelineBinaryStore failing(context, properties);
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
        PipelineBinaryStore reopenedFailure(context, properties);
        reopenedFailure.CreateComputePipeline(info, spirv, layout, &pipeline);
        const bool ordinary = pipeline != VK_NULL_HANDLE && legacyCreates == legacyBeforeFailure + 1 && captures == capturesBefore + 1;
        Environment("APS5_PIPELINE_BINARY_MIN_MS", "0");
        PipelineBinaryStore loweredFailure(context, properties);
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
