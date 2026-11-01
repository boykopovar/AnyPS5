#include "prx/libSceAgcDriver/Graphics/include/PipelineBinaryStore.hpp"
#include "ShaderCacheDirectory.hpp"
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

namespace AgcDriver::Graphics {
namespace {

constexpr std::uint64_t FileMagic = 0x3159425035535041ull;
constexpr std::size_t MaximumFileBytes = 256u * 1024u * 1024u;
constexpr std::uint32_t MaximumBinaries = 64;
constexpr std::uint64_t CaptureFailureMarker = 1;

std::uint64_t configured(const char* name, std::uint64_t fallback) {
    const auto* value = std::getenv(name);
    if (value == nullptr) return fallback;
    std::uint64_t result = 0;
    const auto end = value + std::strlen(value);
    const auto parsed = std::from_chars(value, end, result);
    return parsed.ec == std::errc{} && parsed.ptr == end ? result : fallback;
}

void number(std::vector<std::byte>& output, std::uint64_t value) {
    for (unsigned i = 0; i < 8; ++i) output.push_back(static_cast<std::byte>(value >> (i * 8u)));
}

void bytes(std::vector<std::byte>& output, std::span<const std::byte> value) {
    number(output, value.size());
    output.insert(output.end(), value.begin(), value.end());
}

bool validKey(const VkPipelineBinaryKeyKHR& key) {
    return key.keySize != 0 && key.keySize <= VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR;
}

void binaryKey(std::vector<std::byte>& output, const VkPipelineBinaryKeyKHR& key) {
    bytes(output, std::as_bytes(std::span(key.key, key.keySize)));
}

struct InvalidFile {};

class Reader {
public:
    explicit Reader(std::span<const std::byte> data) : data(data) {}
    std::uint64_t Number() {
        if (data.size() < 8) throw InvalidFile{};
        std::uint64_t value = 0;
        for (unsigned i = 0; i < 8; ++i) value |= static_cast<std::uint64_t>(data[i]) << (i * 8u);
        data = data.subspan(8);
        return value;
    }
    std::span<const std::byte> Bytes() {
        const auto length = Number();
        if (length > data.size()) throw InvalidFile{};
        const auto result = data.first(static_cast<std::size_t>(length));
        data = data.subspan(static_cast<std::size_t>(length));
        return result;
    }
    bool Empty() const { return data.empty(); }

private:
    std::span<const std::byte> data;
};

struct BinaryHandles {
    VkDevice device;
    PFN_vkDestroyPipelineBinaryKHR destroy;
    std::vector<VkPipelineBinaryKHR> handles;
    ~BinaryHandles() {
        for (const auto handle : handles) if (handle != VK_NULL_HANDLE) destroy(device, handle, nullptr);
    }
};

}

bool PipelineBinaryStore::LayersCompatible(VkPhysicalDevice physical, PFN_vkEnumerateDeviceLayerProperties enumerate) {
    if (enumerate == nullptr) {
        std::fprintf(stderr, "[pipeline-binary] disabled: active layer enumeration unavailable; using the driver pipeline cache\n");
        return false;
    }
    VkResult result = VK_INCOMPLETE;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        std::uint32_t count = 0;
        result = enumerate(physical, &count, nullptr);
        if (result == VK_INCOMPLETE) continue;
        if (result != VK_SUCCESS) break;
        if (count == 0) return true;
        std::vector<VkLayerProperties> layers(count);
        result = enumerate(physical, &count, layers.data());
        if (result == VK_INCOMPLETE) continue;
        if (result != VK_SUCCESS) break;
        layers.resize(count);
        for (const auto& layer : layers) {
            if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") != 0 && std::strcmp(layer.layerName, "VK_LAYER_LUNARG_gfxreconstruct") != 0) continue;
            if (layer.specVersion >= VK_MAKE_API_VERSION(0, 1, 3, 294)) continue;
            std::fprintf(stderr, "[pipeline-binary] disabled: active layer %s reports Vulkan %u.%u.%u (< 1.3.294); using the driver pipeline cache\n",
                layer.layerName, VK_API_VERSION_MAJOR(layer.specVersion), VK_API_VERSION_MINOR(layer.specVersion), VK_API_VERSION_PATCH(layer.specVersion));
            return false;
        }
        return true;
    }
    if (result == VK_INCOMPLETE) {
        std::fprintf(stderr, "[pipeline-binary] disabled: active layer enumeration remained VK_INCOMPLETE after 3 attempts; using the driver pipeline cache\n");
    } else {
        std::fprintf(stderr, "[pipeline-binary] disabled: active layer enumeration failed (VkResult %d); using the driver pipeline cache\n", static_cast<int>(result));
    }
    return false;
}

PipelineBinaryStore::PipelineBinaryStore(const Context& context, const VkPhysicalDeviceProperties& properties) : context(context), properties(properties) {
    if (std::getenv("APS5_NO_PIPELINE_BINARIES") != nullptr) return;
    minimumWords = static_cast<std::size_t>(std::min<std::uint64_t>(configured("APS5_PIPELINE_BINARY_MIN_WORDS", minimumWords), std::numeric_limits<std::size_t>::max()));
    minimumMilliseconds = configured("APS5_PIPELINE_BINARY_MIN_MS", minimumMilliseconds);
    const auto root = ShaderRecompiler::ShaderCacheDirectory();
    if (root.empty()) return;
    getKey = reinterpret_cast<PFN_vkGetPipelineKeyKHR>(context.deviceProc(context.device, "vkGetPipelineKeyKHR"));
    createBinaries = reinterpret_cast<PFN_vkCreatePipelineBinariesKHR>(context.deviceProc(context.device, "vkCreatePipelineBinariesKHR"));
    destroyBinary = reinterpret_cast<PFN_vkDestroyPipelineBinaryKHR>(context.deviceProc(context.device, "vkDestroyPipelineBinaryKHR"));
    getData = reinterpret_cast<PFN_vkGetPipelineBinaryDataKHR>(context.deviceProc(context.device, "vkGetPipelineBinaryDataKHR"));
    releaseData = reinterpret_cast<PFN_vkReleaseCapturedPipelineDataKHR>(context.deviceProc(context.device, "vkReleaseCapturedPipelineDataKHR"));
    if (getKey == nullptr || createBinaries == nullptr || destroyBinary == nullptr || getData == nullptr || releaseData == nullptr) return;
    if (getKey(context.device, nullptr, &globalKey) != VK_SUCCESS || !validKey(globalKey)) return;
    directory = root / "driver" / "binaries";
}

std::vector<std::byte> PipelineBinaryStore::Key(const VkComputePipelineCreateInfo& info, std::span<const std::uint32_t> spirv, std::span<const std::uint32_t> layout) const {
    if (directory.empty() || spirv.empty() || layout.empty() || info.pNext != nullptr || info.stage.pName == nullptr || info.basePipelineHandle != VK_NULL_HANDLE ||
        (info.flags & (VK_PIPELINE_CREATE_DERIVATIVE_BIT | VK_PIPELINE_CREATE_ALLOW_DERIVATIVES_BIT)) != 0) return {};
    std::uint32_t subgroupSize = 0;
    if (info.stage.pNext != nullptr) {
        const auto* subgroup = static_cast<const VkPipelineShaderStageRequiredSubgroupSizeCreateInfo*>(info.stage.pNext);
        if (subgroup->sType != VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_REQUIRED_SUBGROUP_SIZE_CREATE_INFO || subgroup->pNext != nullptr) return {};
        subgroupSize = subgroup->requiredSubgroupSize;
    }
    VkPipelineCreateInfoKHR pipelineInfo{VK_STRUCTURE_TYPE_PIPELINE_CREATE_INFO_KHR, const_cast<VkComputePipelineCreateInfo*>(&info)};
    VkPipelineBinaryKeyKHR pipelineKey{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
    if (getKey(context.device, &pipelineInfo, &pipelineKey) != VK_SUCCESS || !validKey(pipelineKey)) return {};
    std::vector<std::byte> key;
    number(key, 1);
    number(key, properties.vendorID);
    number(key, properties.deviceID);
    number(key, properties.driverVersion);
    bytes(key, std::as_bytes(std::span(properties.pipelineCacheUUID)));
    binaryKey(key, globalKey);
    binaryKey(key, pipelineKey);
    bytes(key, std::as_bytes(spirv));
    bytes(key, std::as_bytes(layout));
    number(key, info.flags);
    number(key, static_cast<std::uint32_t>(info.basePipelineIndex));
    number(key, info.stage.flags);
    number(key, info.stage.stage);
    number(key, subgroupSize);
    bytes(key, std::as_bytes(std::span(info.stage.pName, std::strlen(info.stage.pName))));
    const auto* specialization = info.stage.pSpecializationInfo;
    number(key, specialization != nullptr);
    if (specialization != nullptr) {
        if ((specialization->mapEntryCount != 0 && specialization->pMapEntries == nullptr) || (specialization->dataSize != 0 && specialization->pData == nullptr)) return {};
        number(key, specialization->mapEntryCount);
        for (std::uint32_t i = 0; i < specialization->mapEntryCount; ++i) {
            const auto& entry = specialization->pMapEntries[i];
            if (entry.offset > specialization->dataSize || entry.size > specialization->dataSize - entry.offset) return {};
            number(key, entry.constantID);
            number(key, entry.offset);
            number(key, entry.size);
        }
        bytes(key, {static_cast<const std::byte*>(specialization->pData), specialization->dataSize});
    }
    return key;
}

std::filesystem::path PipelineBinaryStore::filePath(std::span<const std::byte> key) const {
    char name[32];
    std::snprintf(name, sizeof(name), "%016llx.bin", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)));
    return directory / name;
}

PipelineBinaryStore::RestoreResult PipelineBinaryStore::restore(const VkComputePipelineCreateInfo& info, std::span<const std::byte> key, VkPipeline* pipeline) const {
    auto path = filePath(key);
    const auto result = restoreFile(info, key, pipeline, path);
    if (result != RestoreResult::Missing) return result;
    path += ".threshold";
    return restoreFile(info, key, pipeline, path);
}

PipelineBinaryStore::RestoreResult PipelineBinaryStore::restoreFile(const VkComputePipelineCreateInfo& info, std::span<const std::byte> key, VkPipeline* pipeline, const std::filesystem::path& path) const try {
    const auto lookupStarted = std::chrono::steady_clock::now();
    std::error_code error;
#ifdef _WIN32
    const auto native = ShaderRecompiler::NativeCachePath_nid_no_patch(path, error);
    if (error) return RestoreResult::Missing;
#else
    const auto& native = path;
#endif
    const auto size = std::filesystem::file_size(native, error);
    if (error || size > MaximumFileBytes) return RestoreResult::Missing;
    std::vector<std::byte> file;
    if (!ShaderRecompiler::ReadWholeFile(path, file) || file.size() < 8) return RestoreResult::Missing;
    const auto payload = std::span(file).first(file.size() - 8);
    Reader checksum{std::span(file).last(8)};
    if (checksum.Number() != ShaderRecompiler::HashBytes(payload)) throw InvalidFile{};
    Reader reader(payload);
    if (reader.Number() != FileMagic || !std::ranges::equal(reader.Bytes(), key)) throw InvalidFile{};
    const auto count = reader.Number();
    if (count == 0) {
        const auto threshold = reader.Number();
        const auto kind = reader.Empty() ? 0 : reader.Number();
        if (!reader.Empty()) throw InvalidFile{};
        if (kind == CaptureFailureMarker) return RestoreResult::CaptureFailed;
        return minimumMilliseconds >= threshold ? RestoreResult::BelowThreshold : RestoreResult::Missing;
    }
    if (count > MaximumBinaries) throw InvalidFile{};
    std::vector<VkPipelineBinaryKeyKHR> keys(static_cast<std::size_t>(count), {VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR});
    std::vector<VkPipelineBinaryDataKHR> data(static_cast<std::size_t>(count));
    for (std::size_t i = 0; i < count; ++i) {
        const auto storedKey = reader.Bytes();
        if (storedKey.empty() || storedKey.size() > VK_MAX_PIPELINE_BINARY_KEY_SIZE_KHR) throw InvalidFile{};
        keys[i].keySize = static_cast<std::uint32_t>(storedKey.size());
        std::memcpy(keys[i].key, storedKey.data(), storedKey.size());
        const auto storedData = reader.Bytes();
        if (storedData.empty()) throw InvalidFile{};
        data[i] = {storedData.size(), const_cast<std::byte*>(storedData.data())};
    }
    if (!reader.Empty()) throw InvalidFile{};
    BinaryHandles binaries{context.device, destroyBinary, std::vector<VkPipelineBinaryKHR>(static_cast<std::size_t>(count))};
    VkPipelineBinaryKeysAndDataKHR keysAndData{static_cast<std::uint32_t>(count), keys.data(), data.data()};
    VkPipelineBinaryCreateInfoKHR binaryInfo{VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR};
    binaryInfo.pKeysAndDataInfo = &keysAndData;
    VkPipelineBinaryHandlesInfoKHR handles{VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR, nullptr, static_cast<std::uint32_t>(count), binaries.handles.data()};
    const auto imported = createBinaries(context.device, &binaryInfo, nullptr, &handles);
    if (imported != VK_SUCCESS || handles.pipelineBinaryCount != count || std::ranges::find(binaries.handles, VK_NULL_HANDLE) != binaries.handles.end()) {
        std::fprintf(stderr, "[pipeline-binary] import rejected key=%016llx result=%d; rebuilding\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)), static_cast<int>(imported));
        return RestoreResult::Missing;
    }
    VkPipelineBinaryInfoKHR binary{VK_STRUCTURE_TYPE_PIPELINE_BINARY_INFO_KHR, nullptr, static_cast<std::uint32_t>(count), binaries.handles.data()};
    auto restoredInfo = info;
    restoredInfo.pNext = &binary;
    const auto started = std::chrono::steady_clock::now();
    const auto result = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &restoredInfo, nullptr, pipeline);
    if (result != VK_SUCCESS) {
        if (*pipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, *pipeline, nullptr);
        *pipeline = VK_NULL_HANDLE;
        std::fprintf(stderr, "[pipeline-binary] restore rejected key=%016llx result=%d; rebuilding\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)), static_cast<int>(result));
        return RestoreResult::Missing;
    }
    std::fprintf(stderr, "[pipeline-binary] loaded key=%016llx bytes=%zu create_ms=%.3f total_ms=%.3f\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)), file.size(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - lookupStarted).count());
    return RestoreResult::Restored;
} catch (const InvalidFile&) {
    std::fprintf(stderr, "[pipeline-binary] invalid store key=%016llx; rebuilding\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)));
    return RestoreResult::Missing;
}

bool PipelineBinaryStore::capture(VkPipeline pipeline, std::span<const std::byte> key) const {
    VkPipelineBinaryCreateInfoKHR info{VK_STRUCTURE_TYPE_PIPELINE_BINARY_CREATE_INFO_KHR};
    info.pipeline = pipeline;
    VkPipelineBinaryHandlesInfoKHR output{VK_STRUCTURE_TYPE_PIPELINE_BINARY_HANDLES_INFO_KHR};
    if (createBinaries(context.device, &info, nullptr, &output) != VK_SUCCESS || output.pipelineBinaryCount == 0 || output.pipelineBinaryCount > MaximumBinaries) return false;
    BinaryHandles binaries{context.device, destroyBinary, std::vector<VkPipelineBinaryKHR>(output.pipelineBinaryCount)};
    const auto count = output.pipelineBinaryCount;
    output.pPipelineBinaries = binaries.handles.data();
    if (createBinaries(context.device, &info, nullptr, &output) != VK_SUCCESS || output.pipelineBinaryCount != count || std::ranges::find(binaries.handles, VK_NULL_HANDLE) != binaries.handles.end()) return false;
    std::vector<std::byte> file;
    number(file, FileMagic);
    bytes(file, key);
    number(file, count);
    for (const auto binary : binaries.handles) {
        VkPipelineBinaryDataInfoKHR dataInfo{VK_STRUCTURE_TYPE_PIPELINE_BINARY_DATA_INFO_KHR, nullptr, binary};
        VkPipelineBinaryKeyKHR binaryId{VK_STRUCTURE_TYPE_PIPELINE_BINARY_KEY_KHR};
        std::size_t size = 0;
        if (getData(context.device, &dataInfo, &binaryId, &size, nullptr) != VK_SUCCESS || !validKey(binaryId) || size == 0 || size > MaximumFileBytes) return false;
        std::vector<std::byte> data(size);
        if (getData(context.device, &dataInfo, &binaryId, &size, data.data()) != VK_SUCCESS || !validKey(binaryId) || size == 0 || size > data.size()) return false;
        data.resize(size);
        if (file.size() > MaximumFileBytes - data.size() || MaximumFileBytes - file.size() - data.size() < 64) return false;
        binaryKey(file, binaryId);
        bytes(file, data);
    }
    number(file, ShaderRecompiler::HashBytes(file));
    const auto path = filePath(key);
    const bool stored = ShaderRecompiler::WriteFileAtomically(path, file);
    if (stored) std::fprintf(stderr, "[pipeline-binary] saved key=%016llx bytes=%zu path=%s\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)), file.size(), path.string().c_str());
    else std::fprintf(stderr, "[pipeline-binary] cannot save key=%016llx path=%s\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)), path.string().c_str());
    return stored;
}

void PipelineBinaryStore::rememberThreshold(std::span<const std::byte> key, bool captureFailed) const try {
    std::vector<std::byte> file;
    number(file, FileMagic);
    bytes(file, key);
    number(file, 0);
    number(file, minimumMilliseconds);
    if (captureFailed) number(file, CaptureFailureMarker);
    number(file, ShaderRecompiler::HashBytes(file));
    auto path = filePath(key);
    path += ".threshold";
    if (file.size() <= MaximumFileBytes && ShaderRecompiler::WriteFileAtomically(path, file) && captureFailed) {
        std::fprintf(stderr, "[pipeline-binary] capture failed key=%016llx; marked for driver cache builds\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)));
    }
} catch (const std::bad_alloc&) {
    std::fputs("[pipeline-binary] marker skipped: host memory exhausted\n", stderr);
}

void PipelineBinaryStore::CreateComputePipeline(const VkComputePipelineCreateInfo& info, std::span<const std::uint32_t> spirv, std::span<const std::uint32_t> layout, VkPipeline* pipeline) const {
    *pipeline = VK_NULL_HANDLE;
    const auto create = context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines");
    std::vector<std::byte> key;
    try {
        if (spirv.size() >= minimumWords) key = Key(info, spirv, layout);
        if (!key.empty()) {
            const auto result = restore(info, key, pipeline);
            if (result == RestoreResult::Restored) return;
            if (result == RestoreResult::BelowThreshold || result == RestoreResult::CaptureFailed) key.clear();
        }
    } catch (const std::bad_alloc&) {
        key.clear();
        std::fputs("[pipeline-binary] lookup skipped: host memory exhausted\n", stderr);
    }
    if (!key.empty()) {
        auto captureInfo = info;
        VkPipelineCreateFlags2CreateInfoKHR flags{VK_STRUCTURE_TYPE_PIPELINE_CREATE_FLAGS_2_CREATE_INFO_KHR, nullptr, static_cast<VkPipelineCreateFlags2KHR>(info.flags) | VK_PIPELINE_CREATE_2_CAPTURE_DATA_BIT_KHR};
        captureInfo.pNext = &flags;
        const auto started = std::chrono::steady_clock::now();
        const auto result = create(context.device, VK_NULL_HANDLE, 1, &captureInfo, nullptr, pipeline);
        const auto milliseconds = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
        if (result == VK_SUCCESS) {
            struct Release {
                const PipelineBinaryStore& store;
                VkPipeline pipeline;
                ~Release() {
                    VkReleaseCapturedPipelineDataInfoKHR release{VK_STRUCTURE_TYPE_RELEASE_CAPTURED_PIPELINE_DATA_INFO_KHR, nullptr, pipeline};
                    store.releaseData(store.context.device, &release, nullptr);
                }
            } release{*this, *pipeline};
            std::fprintf(stderr, "[pipeline-binary] built key=%016llx words=%zu build_ms=%.3f\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)), spirv.size(), milliseconds);
            try {
                if (milliseconds >= static_cast<double>(minimumMilliseconds)) {
                    if (!capture(*pipeline, key)) rememberThreshold(key, true);
                }
                else rememberThreshold(key);
            } catch (const std::bad_alloc&) {
                std::fputs("[pipeline-binary] capture skipped: host memory exhausted\n", stderr);
                if (milliseconds >= static_cast<double>(minimumMilliseconds)) rememberThreshold(key, true);
            }
            return;
        }
        if (*pipeline != VK_NULL_HANDLE) context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, *pipeline, nullptr);
        *pipeline = VK_NULL_HANDLE;
        std::fprintf(stderr, "[pipeline-binary] capture build rejected key=%016llx result=%d; rebuilding without capture\n", static_cast<unsigned long long>(ShaderRecompiler::HashBytes(key)), static_cast<int>(result));
        rememberThreshold(key, true);
    }
    auto lock = context.pipelineCacheSynchronization != nullptr ? context.pipelineCacheSynchronization->LockForPipelineUse() : PipelineCacheSynchronization::PipelineUseLock{};
    Check(create(context.device, context.pipelineCache, 1, &info, nullptr, pipeline), "vkCreateComputePipelines");
}

}
