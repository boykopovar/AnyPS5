#include "ShaderDiskCache.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "CacheKey.hpp"
#include "ShaderCachePack.hpp"
#include "ShaderCacheDirectory.hpp"
#include "ThreadOwned.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#endif
#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#endif
#include "ShaderCacheSourceVersion.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>

namespace ShaderRecompiler {

std::filesystem::path ShaderCacheDirectory() {
    const char* disabled = std::getenv("ANYPS5_NO_SHADER_CACHE");
    if (disabled != nullptr && *disabled != '\0' && std::strcmp(disabled, "0") != 0) return {};
#ifdef _WIN32
    const wchar_t* directory = _wgetenv(L"ANYPS5_SHADER_CACHE_DIR");
    if (directory != nullptr && *directory != L'\0') return std::filesystem::path(directory);
    std::wstring executable(MAX_PATH, L'\0');
    for (;;) {
        const auto length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        if (length == 0) return {};
        if (length < executable.size()) {
            executable.resize(length);
            break;
        }
        executable.resize(executable.size() * 2);
    }
    return std::filesystem::path(executable).parent_path() / "shader_cache";
#else
    const char* directory = std::getenv("ANYPS5_SHADER_CACHE_DIR");
    if (directory != nullptr && *directory != '\0') return std::filesystem::path(directory);
    std::error_code error;
#ifdef __APPLE__
    std::uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) return {};
    const auto executable = std::filesystem::canonical(buffer.data(), error);
#else
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
#endif
    if (error) return {};
    return executable.parent_path() / "shader_cache";
#endif
}

}

namespace ShaderRecompiler::ShaderDiskCache {

#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBCXX__)
static_assert(sizeof(CompiledShaderArtifact) == 280, "CompiledShaderArtifact changed: update the artifact encoder");
static_assert(sizeof(ShaderInvocation) == 216, "ShaderInvocation changed: update the invocation encoder");
static_assert(sizeof(RecompileResult) == 544, "RecompileResult changed: update EncodeResult and DecodeResult");
static_assert(sizeof(DescriptorBinding) == 656, "DescriptorBinding changed: update the binding encoder");
static_assert(sizeof(VertexAttribute) == 32, "VertexAttribute changed: update the attribute encoder");
static_assert(sizeof(VertexInput) == 16, "VertexInput changed: update the vertex input encoder");
static_assert(sizeof(FragmentParameter) == 16, "FragmentParameter changed: update the parameter encoder");
static_assert(sizeof(CompiledShaderInfo) == 352, "CompiledShaderInfo changed: update the info encoder");
static_assert(sizeof(ShaderInfo) == 232, "ShaderInfo changed: update the info encoder");
static_assert(sizeof(BufferResource) == 32, "BufferResource changed: update the info encoder");
static_assert(sizeof(ImageResource) == 92, "ImageResource changed: update the info encoder");
static_assert(sizeof(SamplerResource) == 24, "SamplerResource changed: update the info encoder");
static_assert(sizeof(SampledResourcePair) == 12, "SampledResourcePair changed: update the info encoder");
static_assert(sizeof(StageInput) == 56, "StageInput changed: update the info encoder");
static_assert(sizeof(StageOutput) == 48, "StageOutput changed: update the info encoder");
static_assert(sizeof(IrBindingLayout) == 80, "IrBindingLayout changed: update the layout encoder");
static_assert(sizeof(IrDescriptorBinding) == 32, "IrDescriptorBinding changed: update the layout encoder");
static_assert(sizeof(BindingAllocationResult) == 184, "BindingAllocationResult changed: update the allocation encoder");
static_assert(sizeof(CompiledBindingLayout) == 88, "CompiledBindingLayout changed: update the allocation encoder");
static_assert(sizeof(BindingLayout) == 16, "BindingLayout changed: update BuildKey");
static_assert(sizeof(IrResourcePlan) == 728, "IrResourcePlan changed: update EncodePlan and DecodePlan");
static_assert(sizeof(IrValue) == 128, "IrValue changed: update the plan value encoder");
static_assert(sizeof(GuestRegister) == 8, "GuestRegister changed: update the plan value encoder");
static_assert(sizeof(MemoryInfo) == 80, "MemoryInfo changed: update the plan encoder");
static_assert(sizeof(DescriptorSource) == 192, "DescriptorSource changed: update the plan encoder");
static_assert(sizeof(TableColumn) == 112, "TableColumn changed: update the plan encoder");
static_assert(sizeof(KeyDomain) == 16, "KeyDomain changed: update the plan encoder");
static_assert(sizeof(ResourceBlock) == 104, "ResourceBlock changed: update the plan encoder");
static_assert(sizeof(SrtRead) == 16, "SrtRead changed: update the plan encoder");
static_assert(sizeof(UniformFill) == 28, "UniformFill changed: update the plan encoder");
static_assert(sizeof(UniformFillPlan) == 64, "UniformFillPlan changed: update the plan encoder");
#endif

namespace {

struct AnyField {
    template<typename TField>
    operator TField() const;
};

template<typename TStruct, std::size_t... Index>
constexpr bool bracedWith(std::index_sequence<Index...>) {
    return requires { TStruct{(static_cast<void>(Index), AnyField{})...}; };
}

template<typename TStruct, std::size_t Count = 0>
constexpr std::size_t fieldCount() {
    if constexpr (bracedWith<TStruct>(std::make_index_sequence<Count + 1>{})) {
        return fieldCount<TStruct, Count + 1>();
    } else {
        return Count;
    }
}

static_assert(fieldCount<ShaderInfo>() == 24, "ShaderInfo changed: update the info encoder");
static_assert(fieldCount<BufferResource>() == 12, "BufferResource changed: update the info encoder");
static_assert(fieldCount<ImageResource>() == 42, "ImageResource changed: update the info encoder");
static_assert(fieldCount<SamplerResource>() == 8, "SamplerResource changed: update the info encoder");
static_assert(fieldCount<SampledResourcePair>() == 3, "SampledResourcePair changed: update the info encoder");
static_assert(fieldCount<StageInput>() == 5, "StageInput changed: update the info encoder");
static_assert(fieldCount<StageOutput>() == 4, "StageOutput changed: update the info encoder");
static_assert(fieldCount<IrResourcePlan>() == 24, "IrResourcePlan changed: update EncodePlan and DecodePlan");
static_assert(fieldCount<GuestRegister>() == 2, "GuestRegister changed: update the plan value encoder");
static_assert(fieldCount<MemoryInfo>() == 31, "MemoryInfo changed: update the plan encoder");
static_assert(fieldCount<DescriptorSource>() == 3, "DescriptorSource changed: update the plan encoder");
static_assert(fieldCount<TableColumn>() == 19, "TableColumn changed: update the plan encoder");
static_assert(fieldCount<KeyDomain>() == 4, "KeyDomain changed: update the plan encoder");
static_assert(fieldCount<ResourceBlock>() == 5, "ResourceBlock changed: update the plan encoder");
static_assert(fieldCount<SrtRead>() == 2, "SrtRead changed: update the plan encoder");
static_assert(fieldCount<UniformFill>() == 5, "UniformFill changed: update the plan encoder");
static_assert(fieldCount<UniformFillPlan>() == 2, "UniformFillPlan changed: update the plan encoder");

constexpr std::uint32_t FileMagic = 0x43535041u;

struct FileHeader {
    std::uint32_t magic;
    std::uint32_t format;
    std::uint64_t sourceVersion;
    std::uint64_t keyBytes;
    std::uint64_t payloadBytes;
    std::uint64_t keyHash;
    std::uint64_t payloadHash;
};

std::size_t PayloadOffset(std::size_t keyBytes) {
    constexpr std::size_t alignment = 8;
    return (sizeof(FileHeader) + keyBytes + alignment - 1) / alignment * alignment;
}

class MappedFile {
public:
    MappedFile(const void* data, std::size_t size) : data(data), size(size) {}
    MappedFile(const MappedFile&) = delete;
    MappedFile& operator=(const MappedFile&) = delete;
    ~MappedFile() {
#ifdef _WIN32
        UnmapViewOfFile(data);
#else
        munmap(const_cast<void*>(data), size);
#endif
    }

    [[nodiscard]] std::span<const std::byte> Bytes() const { return {static_cast<const std::byte*>(data), size}; }

private:
    const void* data;
    std::size_t size;
};

std::shared_ptr<const MappedFile> MapFile(const std::filesystem::path& path) {
#ifdef _WIN32
    std::error_code error;
    const auto filename = NativeCachePath_nid_no_patch(path, error);
    if (error) return nullptr;
    const HANDLE file = CreateFileW(filename.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return nullptr;
    LARGE_INTEGER length{};
    if (!GetFileSizeEx(file, &length) || length.QuadPart <= 0 || static_cast<unsigned long long>(length.QuadPart) > std::numeric_limits<std::size_t>::max()) {
        CloseHandle(file);
        return nullptr;
    }
    const HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(file);
    if (mapping == nullptr) return nullptr;
    const void* data = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(mapping);
    if (data == nullptr) return nullptr;
    try {
        return std::make_shared<const MappedFile>(data, static_cast<std::size_t>(length.QuadPart));
    } catch (...) {
        UnmapViewOfFile(data);
        throw;
    }
#else
    const int descriptor = open(path.c_str(), O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return nullptr;
    struct stat status{};
    void* data = MAP_FAILED;
    if (fstat(descriptor, &status) == 0 && status.st_size > 0) data = mmap(nullptr, static_cast<std::size_t>(status.st_size), PROT_READ, MAP_PRIVATE, descriptor, 0);
    close(descriptor);
    if (data == MAP_FAILED) return nullptr;
    try {
        return std::make_shared<const MappedFile>(data, static_cast<std::size_t>(status.st_size));
    } catch (...) {
        munmap(data, static_cast<std::size_t>(status.st_size));
        throw;
    }
#endif
}
static_assert(sizeof(FileHeader) == 48 && std::is_trivially_copyable_v<FileHeader>);

class Writer {
public:
    explicit Writer(std::vector<std::byte>& out) : out(out) {}

    template<typename TValue>
    void Value(TValue value) requires (std::is_integral_v<TValue> || std::is_enum_v<TValue>) {
        if constexpr (std::is_same_v<TValue, bool>) {
            Raw(static_cast<std::uint8_t>(value ? 1u : 0u));
        } else if constexpr (std::is_enum_v<TValue>) {
            Raw(static_cast<std::uint32_t>(value));
        } else {
            Raw(value);
        }
    }

    template<typename TValue>
    void Values(std::span<const TValue> values) requires std::is_integral_v<TValue> {
        Value<std::uint64_t>(values.size());
        const auto bytes = std::as_bytes(values);
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    void Flags(const std::vector<bool>& flags) {
        Value<std::uint64_t>(flags.size());
        for (const bool flag : flags) Value(flag);
    }

    void Compact(std::uint64_t value) {
        while (value >= 0x80u) {
            Raw(static_cast<std::uint8_t>(value | 0x80u));
            value >>= 7u;
        }
        Raw(static_cast<std::uint8_t>(value));
    }

    void Text(const std::string& text) {
        Value<std::uint64_t>(text.size());
        const auto bytes = std::as_bytes(std::span(text.data(), text.size()));
        out.insert(out.end(), bytes.begin(), bytes.end());
    }

    template<typename TValue, typename TEncode>
    void List(const std::vector<TValue>& values, TEncode&& encode) {
        Value<std::uint64_t>(values.size());
        for (const auto& value : values) encode(*this, value);
    }

private:
    template<typename TValue>
    void Raw(TValue value) {
        const auto offset = out.size();
        out.resize(offset + sizeof(value));
        std::memcpy(out.data() + offset, &value, sizeof(value));
    }

    std::vector<std::byte>& out;
};

class Reader {
public:
    explicit Reader(std::span<const std::byte> bytes, std::shared_ptr<const void> owner = nullptr) : bytes(bytes), owner(std::move(owner)) {}

    [[nodiscard]] bool Ok() const { return ok; }
    [[nodiscard]] bool Done() const { return ok && position == bytes.size(); }
    [[nodiscard]] std::size_t Remaining() const { return ok ? bytes.size() - position : 0; }
    void Fail() { ok = false; }

    [[nodiscard]] std::uint64_t Compact() {
        std::uint64_t value = 0;
        for (std::uint32_t shift = 0; shift < 64u; shift += 7u) {
            const auto byte = Raw<std::uint8_t>();
            if (!ok) return 0;
            value |= static_cast<std::uint64_t>(byte & 0x7fu) << shift;
            if ((byte & 0x80u) != 0u) continue;
            if ((byte == 0u && shift != 0u) || (shift == 63u && byte > 1u)) ok = false;
            return ok ? value : 0;
        }
        ok = false;
        return 0;
    }

    template<typename TValue>
    void Value(TValue& value) requires (std::is_integral_v<TValue> || std::is_enum_v<TValue>) {
        if constexpr (std::is_same_v<TValue, bool>) {
            const auto raw = Raw<std::uint8_t>();
            if (raw > 1u) ok = false;
            value = raw == 1u;
        } else if constexpr (std::is_enum_v<TValue>) {
            value = static_cast<TValue>(Raw<std::uint32_t>());
        } else {
            value = Raw<TValue>();
        }
    }

    template<typename TValue>
    [[nodiscard]] TValue Get() {
        TValue value{};
        Value(value);
        return value;
    }

    template<typename TValue>
    void Values(std::vector<TValue>& values) requires std::is_integral_v<TValue> {
        const auto count = Count(sizeof(TValue));
        values.resize(count);
        if (count != 0) {
            std::memcpy(values.data(), bytes.data() + position, count * sizeof(TValue));
            position += count * sizeof(TValue);
        }
    }

    SharedSpirv Spirv() {
        const auto count = Count(sizeof(std::uint32_t));
        const auto* first = bytes.data() + position;
        position += count * sizeof(std::uint32_t);
        if (owner != nullptr && reinterpret_cast<std::uintptr_t>(first) % alignof(std::uint32_t) == 0) {
            return SharedSpirv(owner, std::span(reinterpret_cast<const std::uint32_t*>(first), count));
        }
        std::vector<std::uint32_t> words(count);
        if (count != 0) std::memcpy(words.data(), first, count * sizeof(std::uint32_t));
        return SharedSpirv(std::move(words));
    }

    void Flags(std::vector<bool>& flags) {
        const auto count = Count(1);
        flags.assign(count, false);
        for (std::size_t i = 0; i < count; ++i) flags[i] = Get<bool>();
    }

    void Text(std::string& text) {
        const auto count = Count(1);
        text.assign(reinterpret_cast<const char*>(bytes.data() + position), count);
        position += count;
    }

    template<typename TValue, typename TDecode>
    void List(std::vector<TValue>& values, std::size_t minimum, TDecode&& decode) {
        const auto count = Count(minimum);
        values.assign(count, TValue{});
        for (auto& value : values) {
            if (!ok) break;
            decode(*this, value);
        }
    }

private:
    template<typename TValue>
    TValue Raw() {
        TValue value{};
        if (!ok || bytes.size() - position < sizeof(value)) {
            ok = false;
            return value;
        }
        std::memcpy(&value, bytes.data() + position, sizeof(value));
        position += sizeof(value);
        return value;
    }

    std::size_t Count(std::size_t elementBytes) {
        const auto count = Raw<std::uint64_t>();
        if (!ok || elementBytes == 0 || count > (bytes.size() - position) / elementBytes) {
            ok = false;
            return 0;
        }
        return static_cast<std::size_t>(count);
    }

    std::span<const std::byte> bytes;
    std::shared_ptr<const void> owner;
    std::size_t position = 0;
    bool ok = true;
};

void encodeBinding(Writer& writer, const DescriptorBinding& binding) {
    writer.Value(binding.kind);
    writer.Value(binding.role);
    writer.Value(binding.descriptorSet);
    writer.Value(binding.binding);
    writer.Value(binding.count);
    writer.Values(std::span<const std::uint32_t>(binding.guestDescriptor));
    writer.Value(binding.readOnly);
    writer.Value(binding.imageShape.has_value());
    writer.Value(binding.imageShape.value_or(DescriptorImageShape::Image1D));
    writer.Flags(binding.samplerDepthCompare);
    writer.Flags(binding.imageWritten);
    writer.Flags(binding.imageDepthCompare);
    writer.Flags(binding.imageAtomic);
    writer.Flags(binding.imageAtomic64);
    writer.List(binding.imageShapes, [](Writer& out, DescriptorImageShape shape) { out.Value(shape); });
    writer.Flags(binding.bufferAtomic);
    writer.Flags(binding.bufferWritten);
    writer.Flags(binding.samplerUnnormalized);
    writer.Flags(binding.imageUnnormalized);
    writer.Flags(binding.imageUnnormalizedBaseLevel);
    writer.Flags(binding.samplerIntegerBorder);
    writer.Flags(binding.samplerCoordinatesIgnored);
    writer.Values(std::span<const std::uint32_t>(binding.imageSamplers));
    writer.Flags(binding.bufferRead);
    writer.Values(std::span<const std::uint32_t>(binding.bufferExtent));
}

void decodeBinding(Reader& reader, DescriptorBinding& binding) {
    reader.Value(binding.kind);
    reader.Value(binding.role);
    reader.Value(binding.descriptorSet);
    reader.Value(binding.binding);
    reader.Value(binding.count);
    reader.Values(binding.guestDescriptor);
    reader.Value(binding.readOnly);
    const bool hasShape = reader.Get<bool>();
    const auto shape = reader.Get<DescriptorImageShape>();
    binding.imageShape = hasShape ? std::optional(shape) : std::nullopt;
    reader.Flags(binding.samplerDepthCompare);
    reader.Flags(binding.imageWritten);
    reader.Flags(binding.imageDepthCompare);
    reader.Flags(binding.imageAtomic);
    reader.Flags(binding.imageAtomic64);
    reader.List(binding.imageShapes, 4, [](Reader& in, DescriptorImageShape& shape) { in.Value(shape); });
    reader.Flags(binding.bufferAtomic);
    reader.Flags(binding.bufferWritten);
    reader.Flags(binding.samplerUnnormalized);
    reader.Flags(binding.imageUnnormalized);
    reader.Flags(binding.imageUnnormalizedBaseLevel);
    reader.Flags(binding.samplerIntegerBorder);
    reader.Flags(binding.samplerCoordinatesIgnored);
    reader.Values(binding.imageSamplers);
    reader.Flags(binding.bufferRead);
    reader.Values(binding.bufferExtent);
}

void encodeArtifact(Writer& writer, const CompiledShaderArtifact& result) {
    writer.Value(result.runtimeAbiVersion);
    writer.Values(std::span<const std::uint32_t>(result.spirv.Words()));
    writer.Value(result.bdaAbiVersion);
    writer.Value(result.windowLaneGuards);
    writer.Value(result.memoryOffsetDword);
    writer.Value(result.shaderDataDwords);
    writer.Value(result.imageMetadataDword);
    writer.Value(result.runtimeImageCount);
    writer.Values(std::span<const std::uint32_t>(result.runtimeImageResources));
    writer.Value(result.registrationModule);
    writer.List(result.vertexInputPatches, [](Writer& out, const VertexInputPatch& patch) {
        out.Value(patch.location);
        out.Value(patch.word);
        for (const auto value : patch.values) out.Value(value);
    });
    writer.List(result.vertexInputs, [](Writer& out, const VertexInput& attribute) {
        out.Value(attribute.location);
        out.Value(attribute.components);
        out.Value(attribute.fetchIndex);
        out.Value(attribute.outputMask);
    });
    writer.Value(result.vertexOffsetSgpr);
    writer.Value(result.instanceOffsetSgpr);
    writer.Value(result.vertexOffsetShared);
    writer.Value(result.instanceOffsetShared);
    writer.Value(result.vertexOffsetConflict);
    writer.Value(result.instanceOffsetConflict);
    writer.Value(result.hostSubgroupSize);
    writer.Value(result.fullSubgroups);
    writer.Value(result.functionLdsBuffer);
    writer.Values(std::span<const std::uint32_t>(result.parameterExports));
    writer.List(result.fragmentParameters, [](Writer& out, const FragmentParameter& parameter) {
        out.Value(parameter.location);
        out.Value(parameter.sourceLocation);
        out.Value(parameter.flat);
        out.Value(parameter.perVertex);
        out.Value(parameter.custom);
        out.Value(parameter.flatP10);
        out.Value(parameter.vertexOrder);
        out.Value(parameter.flatP20);
    });
    writer.Value(result.barycentricEmulation.active);
    writer.Value(result.barycentricEmulation.smooth);
    writer.Value(result.barycentricEmulation.linear);
    writer.Value(result.barycentricEmulation.smoothOffset);
    writer.Value(result.meshBarycentrics.has_value());
    if (result.meshBarycentrics) {
        writer.Value(result.meshBarycentrics->emulation.active);
        writer.Value(result.meshBarycentrics->emulation.smooth);
        writer.Value(result.meshBarycentrics->emulation.linear);
        writer.List(result.meshBarycentrics->parameters, [](Writer& out, const FragmentParameter& parameter) {
            out.Value(parameter.location);
            out.Value(parameter.sourceLocation);
            out.Value(parameter.perVertex);
        });
    }
    writer.Values(std::span<const std::uint32_t>(result.stencilReferences));
    writer.Value(result.stencilReplaySafe);
    writer.Value(result.bdaWrites);
    writer.Value(result.stencilReferenceDword);
}

void decodeArtifact(Reader& reader, CompiledShaderArtifact& result) {
    reader.Value(result.runtimeAbiVersion);
    result.spirv = reader.Spirv();
    reader.Value(result.bdaAbiVersion);
    reader.Value(result.windowLaneGuards);
    reader.Value(result.memoryOffsetDword);
    reader.Value(result.shaderDataDwords);
    reader.Value(result.imageMetadataDword);
    reader.Value(result.runtimeImageCount);
    reader.Values(result.runtimeImageResources);
    reader.Value(result.registrationModule);
    reader.List(result.vertexInputPatches, 20, [](Reader& in, VertexInputPatch& patch) {
        in.Value(patch.location);
        in.Value(patch.word);
        for (auto& value : patch.values) in.Value(value);
    });
    reader.List(result.vertexInputs, 16, [](Reader& in, VertexInput& attribute) {
        in.Value(attribute.location);
        in.Value(attribute.components);
        in.Value(attribute.fetchIndex);
        in.Value(attribute.outputMask);
    });
    reader.Value(result.vertexOffsetSgpr);
    reader.Value(result.instanceOffsetSgpr);
    reader.Value(result.vertexOffsetShared);
    reader.Value(result.instanceOffsetShared);
    reader.Value(result.vertexOffsetConflict);
    reader.Value(result.instanceOffsetConflict);
    reader.Value(result.hostSubgroupSize);
    reader.Value(result.fullSubgroups);
    reader.Value(result.functionLdsBuffer);
    reader.Values(result.parameterExports);
    reader.List(result.fragmentParameters, 10, [](Reader& in, FragmentParameter& parameter) {
        in.Value(parameter.location);
        in.Value(parameter.sourceLocation);
        in.Value(parameter.flat);
        in.Value(parameter.perVertex);
        in.Value(parameter.custom);
        in.Value(parameter.flatP10);
        in.Value(parameter.vertexOrder);
        in.Value(parameter.flatP20);
    });
    reader.Value(result.barycentricEmulation.active);
    reader.Value(result.barycentricEmulation.smooth);
    reader.Value(result.barycentricEmulation.linear);
    reader.Value(result.barycentricEmulation.smoothOffset);
    bool hasMeshBarycentrics = false;
    reader.Value(hasMeshBarycentrics);
    result.meshBarycentrics.reset();
    if (hasMeshBarycentrics) {
        MeshBarycentricLayout layout;
        reader.Value(layout.emulation.active);
        reader.Value(layout.emulation.smooth);
        reader.Value(layout.emulation.linear);
        reader.List(layout.parameters, 9, [](Reader& in, FragmentParameter& parameter) {
            in.Value(parameter.location);
            in.Value(parameter.sourceLocation);
            in.Value(parameter.perVertex);
        });
        result.meshBarycentrics = std::move(layout);
    }
    reader.Values(result.stencilReferences);
    reader.Value(result.stencilReplaySafe);
    reader.Value(result.bdaWrites);
    reader.Value(result.stencilReferenceDword);
    result.variantId = 0;
}

void encodeInvocation(Writer& writer, const ShaderInvocation& invocation) {
    writer.List(invocation.specialization, [](Writer& out, const PipelineSpecializationConstant& constant) {
        out.Value(constant.id);
        out.Value(constant.value);
    });
    writer.List(invocation.bindings, encodeBinding);
    writer.Value<std::uint64_t>(invocation.pushConstants.size());
    for (const auto byte : invocation.pushConstants) writer.Value(static_cast<std::uint8_t>(byte));
    writer.Value(invocation.readsDepthRemapPush);
    writer.List(invocation.vertexAttributes, [](Writer& out, const VertexAttribute& attribute) {
        out.Value(attribute.location);
        out.Value(attribute.components);
        for (const auto field : attribute.resource.fields) out.Value(field);
        out.Value(attribute.fetchIndex);
        out.Value(attribute.formatComponents);
    });
    writer.Value(invocation.userDataRegisterReads);
    writer.Value(invocation.poisonedSrtReads);
    writer.List(invocation.imageTablePoison, [](Writer& out, const ImageTableEntryPoison& poison) {
        for (const auto word : poison.words) out.Value(word);
        out.Value(poison.dwordCount);
        out.Value(poison.resource);
        out.Value(poison.reason);
    });
    writer.List(invocation.imageTableRanges, [](Writer& out, const std::pair<std::uint64_t, std::uint64_t>& range) {
        out.Value(range.first);
        out.Value(range.second);
    });
    writer.List(invocation.imageTableReadRanges, [](Writer& out, const auto& ranges) {
        out.List(ranges, [](Writer& rangeWriter, const auto& range) {
            rangeWriter.Value(range.first);
            rangeWriter.Value(range.second);
        });
    });
    writer.Value(invocation.imageTableShader);
    writer.Value(invocation.imageTableFaults);
}

void decodeInvocation(Reader& reader, ShaderInvocation& invocation) {
    invocation.specializationId = 0;
    reader.List(invocation.specialization, 8, [](Reader& in, PipelineSpecializationConstant& constant) {
        in.Value(constant.id);
        in.Value(constant.value);
    });
    reader.List(invocation.bindings, 8, decodeBinding);
    std::vector<std::uint8_t> pushConstants;
    reader.Values(pushConstants);
    invocation.pushConstants.resize(pushConstants.size());
    if (!pushConstants.empty()) std::memcpy(invocation.pushConstants.data(), pushConstants.data(), pushConstants.size());
    reader.Value(invocation.readsDepthRemapPush);
    reader.List(invocation.vertexAttributes, 32, [](Reader& in, VertexAttribute& attribute) {
        in.Value(attribute.location);
        in.Value(attribute.components);
        for (auto& field : attribute.resource.fields) in.Value(field);
        in.Value(attribute.fetchIndex);
        in.Value(attribute.formatComponents);
    });
    reader.Value(invocation.userDataRegisterReads);
    reader.Value(invocation.poisonedSrtReads);
    reader.List(invocation.imageTablePoison, 44, [](Reader& in, ImageTableEntryPoison& poison) {
        for (auto& word : poison.words) in.Value(word);
        in.Value(poison.dwordCount);
        in.Value(poison.resource);
        in.Value(poison.reason);
    });
    reader.List(invocation.imageTableRanges, 16, [](Reader& in, std::pair<std::uint64_t, std::uint64_t>& range) {
        in.Value(range.first);
        in.Value(range.second);
    });
    reader.List(invocation.imageTableReadRanges, 8, [](Reader& in, auto& ranges) {
        in.List(ranges, 16, [](Reader& rangeReader, auto& range) {
            rangeReader.Value(range.first);
            rangeReader.Value(range.second);
        });
    });
    reader.Value(invocation.imageTableShader);
    reader.Value(invocation.imageTableFaults);
}

void encodeLayout(Writer& writer, const IrBindingLayout& layout) {
    writer.Value(layout.pushDataStartDword);
    writer.Value(layout.stencilReferenceDword);
    writer.Value(layout.memoryOffsetDword);
    writer.Value(layout.memoryOffsetCount);
    writer.Value(layout.dispatchThreadLimit);
    writer.Value(layout.runtimeImageCount);
    writer.Value(layout.runtimeStateDwords);
    writer.Values(std::span<const std::uint32_t>(layout.userDataRegisters));
    writer.List(layout.descriptors, [](Writer& out, const IrDescriptorBinding& descriptor) {
        out.Value(descriptor.kind);
        out.Values(std::span<const std::uint32_t>(descriptor.resources));
    });
}

void decodeLayout(Reader& reader, IrBindingLayout& layout) {
    reader.Value(layout.pushDataStartDword);
    reader.Value(layout.stencilReferenceDword);
    reader.Value(layout.memoryOffsetDword);
    reader.Value(layout.memoryOffsetCount);
    reader.Value(layout.dispatchThreadLimit);
    reader.Value(layout.runtimeImageCount);
    reader.Value(layout.runtimeStateDwords);
    reader.Values(layout.userDataRegisters);
    reader.List(layout.descriptors, 12, [](Reader& in, IrDescriptorBinding& descriptor) {
        in.Value(descriptor.kind);
        in.Values(descriptor.resources);
    });
}

void encodeShaderInfo(Writer& writer, const ShaderInfo& info) {
    writer.Value(info.scratchDwords);
    writer.Value(info.sharedMemoryBytes);
    writer.List(info.buffers, [](Writer& out, const BufferResource& buffer) {
        out.Value(buffer.source);
        out.Value(buffer.firstUsePc);
        out.Value(buffer.maxByteExtent);
        out.Value(buffer.imageAlias);
        out.Value(buffer.read);
        out.Value(buffer.written);
        out.Value(buffer.atomic);
        out.Value(buffer.formatted);
        out.Value(buffer.descriptorFormatted);
        out.Value(buffer.formattedReadMask);
        out.Value(buffer.scalar);
        out.Value(buffer.typedAlignment);
    });
    writer.List(info.images, [](Writer& out, const ImageResource& image) {
        out.Value(image.source);
        out.Value(image.firstUsePc);
        out.Value(image.resourceClass);
        out.Value(image.numericClass);
        out.Value(image.dimension);
        out.Value(image.mipMode);
        out.Value(image.mipCount);
        out.Value(image.conversionFormat);
        out.Value(image.shaderSwizzle);
        out.Value(image.read);
        out.Value(image.written);
        out.Value(image.atomic);
        out.Value(image.atomic64);
        out.Value(image.depthCompare);
        out.Value(image.cube);
        out.Value(image.r128);
        out.Value(image.srgbDecode);
        out.Value(image.srgbDecodeCompatible);
        out.Value(image.srgbDecodeFormats);
        out.Value(image.depthBits);
        out.Value(image.depthUnorm16);
        out.Value(image.packed);
        out.Value(image.fmaskCompatible);
        out.Value(image.depthBitsCompatible);
        out.Value(image.constantSwizzle);
        out.Value(image.constantSwizzleCompatible);
        out.Value(image.cubeFaceCompatible);
        out.Value(image.flatVolumeCompatible);
        out.Value(image.singleMipPlaneBiasCompatible);
        out.Value(image.volumeArraySampleCompatible);
        out.Value(image.flatLineCompatible);
        out.Value(image.flatVolumeLineCompatible);
        out.Value(image.foldTexelOffsetsOnly);
        out.Value(image.convertedSamples);
        out.Value(image.remappedDepth1D);
        out.Value(image.byElements);
        out.Value(image.byComponents);
        out.Value(image.packedFormat);
        out.Value(image.emulatedCompare);
        out.Value(image.table);
        out.Value(image.tableOperation);
        out.Value(image.targetMerge);
    });
    writer.Value(!info.runtimeImageModes.empty());
    writer.List(info.samplers, [](Writer& out, const SamplerResource& sampler) {
        out.Value(sampler.source);
        out.Value(sampler.firstUsePc);
        out.Value(sampler.copyOf);
        out.Value(sampler.forcePointFiltering);
        out.Value(sampler.depthCompare);
        out.Value(sampler.uses);
        out.Value(sampler.foldTexelOffsetsOnly);
        out.Value(sampler.table);
    });
    writer.List(info.sampledPairs, [](Writer& out, const SampledResourcePair& pair) {
        out.Value(pair.image);
        out.Value(pair.sampler);
        out.Value(pair.firstUsePc);
    });
    writer.List(info.inputs, [](Writer& out, const StageInput& input) {
        out.Value(input.kind);
        out.Value(input.location);
        out.Value(input.componentCount);
        out.Text(input.debugName);
        out.Value(input.perVertex);
    });
    writer.List(info.outputs, [](Writer& out, const StageOutput& output) {
        out.Value(output.kind);
        out.Value(output.index);
        out.Value(output.location);
        out.Text(output.debugName);
    });
    for (const auto components : info.vertexFetchComponents) writer.Value(components);
    writer.Value(info.vertexOffsetSgpr);
    writer.Value(info.instanceOffsetSgpr);
    writer.Value(info.vertexOffsetShared);
    writer.Value(info.instanceOffsetShared);
    writer.Value(info.vertexOffsetConflict);
    writer.Value(info.instanceOffsetConflict);
    writer.Value(info.hasBitwiseXor);
    writer.Value(info.usesDma);
    writer.Value(info.bdaWrites);
    writer.Value(info.usesFaultBuffer);
    writer.Value(info.dispatchThreadLimit);
    writer.Value(info.rasterPrimitive);
    writer.Value(info.layerExported);
    writer.Value(info.smoothOffsetBarycentrics);
}

void decodeShaderInfo(Reader& reader, ShaderInfo& info) {
    reader.Value(info.scratchDwords);
    reader.Value(info.sharedMemoryBytes);
    reader.List(info.buffers, 27, [](Reader& in, BufferResource& buffer) {
        in.Value(buffer.source);
        in.Value(buffer.firstUsePc);
        in.Value(buffer.maxByteExtent);
        in.Value(buffer.imageAlias);
        in.Value(buffer.read);
        in.Value(buffer.written);
        in.Value(buffer.atomic);
        in.Value(buffer.formatted);
        in.Value(buffer.descriptorFormatted);
        in.Value(buffer.formattedReadMask);
        in.Value(buffer.scalar);
        in.Value(buffer.typedAlignment);
    });
    reader.List(info.images, 77, [](Reader& in, ImageResource& image) {
        in.Value(image.source);
        in.Value(image.firstUsePc);
        in.Value(image.resourceClass);
        in.Value(image.numericClass);
        in.Value(image.dimension);
        in.Value(image.mipMode);
        in.Value(image.mipCount);
        in.Value(image.conversionFormat);
        in.Value(image.shaderSwizzle);
        in.Value(image.read);
        in.Value(image.written);
        in.Value(image.atomic);
        in.Value(image.atomic64);
        in.Value(image.depthCompare);
        in.Value(image.cube);
        in.Value(image.r128);
        in.Value(image.srgbDecode);
        in.Value(image.srgbDecodeCompatible);
        in.Value(image.srgbDecodeFormats);
        in.Value(image.depthBits);
        in.Value(image.depthUnorm16);
        in.Value(image.packed);
        in.Value(image.fmaskCompatible);
        in.Value(image.depthBitsCompatible);
        in.Value(image.constantSwizzle);
        in.Value(image.constantSwizzleCompatible);
        in.Value(image.cubeFaceCompatible);
        in.Value(image.flatVolumeCompatible);
        in.Value(image.singleMipPlaneBiasCompatible);
        in.Value(image.volumeArraySampleCompatible);
        in.Value(image.flatLineCompatible);
        in.Value(image.flatVolumeLineCompatible);
        in.Value(image.foldTexelOffsetsOnly);
        in.Value(image.convertedSamples);
        in.Value(image.remappedDepth1D);
        in.Value(image.byElements);
        in.Value(image.byComponents);
        in.Value(image.packedFormat);
        in.Value(image.emulatedCompare);
        in.Value(image.table);
        in.Value(image.tableOperation);
        in.Value(image.targetMerge);
    });
    bool preparedImageModes = false;
    reader.Value(preparedImageModes);
    if (preparedImageModes) ResourceMaterializer::PrepareImageModes(info);
    reader.List(info.samplers, 21, [](Reader& in, SamplerResource& sampler) {
        in.Value(sampler.source);
        in.Value(sampler.firstUsePc);
        in.Value(sampler.copyOf);
        in.Value(sampler.forcePointFiltering);
        in.Value(sampler.depthCompare);
        in.Value(sampler.uses);
        in.Value(sampler.foldTexelOffsetsOnly);
        in.Value(sampler.table);
    });
    reader.List(info.sampledPairs, 12, [](Reader& in, SampledResourcePair& pair) {
        in.Value(pair.image);
        in.Value(pair.sampler);
        in.Value(pair.firstUsePc);
    });
    reader.List(info.inputs, 21, [](Reader& in, StageInput& input) {
        in.Value(input.kind);
        in.Value(input.location);
        in.Value(input.componentCount);
        in.Text(input.debugName);
        in.Value(input.perVertex);
    });
    reader.List(info.outputs, 20, [](Reader& in, StageOutput& output) {
        in.Value(output.kind);
        in.Value(output.index);
        in.Value(output.location);
        in.Text(output.debugName);
    });
    for (auto& components : info.vertexFetchComponents) reader.Value(components);
    reader.Value(info.vertexOffsetSgpr);
    reader.Value(info.instanceOffsetSgpr);
    reader.Value(info.vertexOffsetShared);
    reader.Value(info.instanceOffsetShared);
    reader.Value(info.vertexOffsetConflict);
    reader.Value(info.instanceOffsetConflict);
    reader.Value(info.hasBitwiseXor);
    reader.Value(info.usesDma);
    reader.Value(info.bdaWrites);
    reader.Value(info.usesFaultBuffer);
    reader.Value(info.dispatchThreadLimit);
    reader.Value(info.rasterPrimitive);
    reader.Value(info.layerExported);
    reader.Value(info.smoothOffsetBarycentrics);
}

void encodeInfo(Writer& writer, const CompiledShaderInfo& compiled) {
    writer.Value(compiled.stage);
    writer.Value(compiled.shaderHash);
    writer.Value(compiled.waveSize);
    writer.Value(compiled.userDataBase);
    writer.Value(compiled.userDataCount);
    writer.Value(compiled.scratchDwords);
    writer.Value(compiled.paramExportMask);
    encodeShaderInfo(writer, compiled.info);
    encodeLayout(writer, compiled.bindings);
}

void decodeInfo(Reader& reader, CompiledShaderInfo& compiled) {
    reader.Value(compiled.stage);
    reader.Value(compiled.shaderHash);
    reader.Value(compiled.waveSize);
    reader.Value(compiled.userDataBase);
    reader.Value(compiled.userDataCount);
    reader.Value(compiled.scratchDwords);
    reader.Value(compiled.paramExportMask);
    decodeShaderInfo(reader, compiled.info);
    decodeLayout(reader, compiled.bindings);
}

void encodeAllocation(Writer& writer, const CompiledBindingLayout& allocation) {
    encodeLayout(writer, allocation.layout);
    writer.Value(allocation.pushConstantOffsetBytes);
    writer.Value(allocation.pushConstantSizeBytes);
}

void decodeAllocation(Reader& reader, CompiledBindingLayout& allocation) {
    decodeLayout(reader, allocation.layout);
    reader.Value(allocation.pushConstantOffsetBytes);
    reader.Value(allocation.pushConstantSizeBytes);
}

constexpr std::uint32_t PlanEntryKind = 0x4e414c50u;
constexpr std::uint32_t NoPlanIndex = std::numeric_limits<std::uint32_t>::max();

void encodeMemoryInfo(Writer& out, const MemoryInfo& memory) {
    out.Value(memory.kind);
    out.Value(memory.resource);
    out.Value(memory.sampler);
    out.Value(memory.offset);
    out.Value(memory.secondaryOffset);
    out.Value(memory.dmask);
    out.Value(memory.dataDwords);
    out.Value(memory.dataBits);
    out.Value(memory.componentIndex);
    out.Value(memory.componentCount);
    out.Value(memory.dataFormat);
    out.Value(memory.numberFormat);
    out.Value(memory.imageSampleFlags);
    out.Value(memory.imageDimension);
    out.Value(memory.imageAddressComponents);
    out.Value(memory.addressIsFull);
    out.Value(memory.dataSigned);
    out.Value(memory.typed);
    out.Value(memory.formatted);
    out.Value(memory.d16);
    out.Value(memory.imageHasMip);
    out.Value(memory.imageR128);
    out.Value(memory.imagePacked);
    out.Value(memory.imageByElements);
    out.Value(memory.idxen);
    out.Value(memory.offen);
    out.Value(memory.planningOnly);
    out.Value(memory.coherent);
    out.Value(memory.gpuDescriptor);
    out.Value(memory.flushDenormals);
    out.Value(memory.flushDenormalResults);
}

void decodeMemoryInfo(Reader& in, MemoryInfo& memory) {
    in.Value(memory.kind);
    in.Value(memory.resource);
    in.Value(memory.sampler);
    in.Value(memory.offset);
    in.Value(memory.secondaryOffset);
    in.Value(memory.dmask);
    in.Value(memory.dataDwords);
    in.Value(memory.dataBits);
    in.Value(memory.componentIndex);
    in.Value(memory.componentCount);
    in.Value(memory.dataFormat);
    in.Value(memory.numberFormat);
    in.Value(memory.imageSampleFlags);
    in.Value(memory.imageDimension);
    in.Value(memory.imageAddressComponents);
    in.Value(memory.addressIsFull);
    in.Value(memory.dataSigned);
    in.Value(memory.typed);
    in.Value(memory.formatted);
    in.Value(memory.d16);
    in.Value(memory.imageHasMip);
    in.Value(memory.imageR128);
    in.Value(memory.imagePacked);
    in.Value(memory.imageByElements);
    in.Value(memory.idxen);
    in.Value(memory.offen);
    in.Value(memory.planningOnly);
    in.Value(memory.coherent);
    in.Value(memory.gpuDescriptor);
    in.Value(memory.flushDenormals);
    in.Value(memory.flushDenormalResults);
}

bool encodePlan(Writer& writer, const IrResourcePlan& plan) {
    if (std::ranges::any_of(plan.blockStorage, [](const auto& block) { return block == nullptr; })) return false;
    std::unordered_map<const IrValue*, std::uint32_t> values;
    std::unordered_map<const IrBlock*, std::uint32_t> blocks;
    values.reserve(plan.valueStorage.Size());
    blocks.reserve(plan.blockStorage.size());
    for (std::size_t index = 0; index < plan.valueStorage.Size(); ++index) values.emplace(&plan.valueStorage[index], static_cast<std::uint32_t>(index));
    for (std::size_t index = 0; index < plan.blockStorage.size(); ++index) blocks.emplace(plan.blockStorage[index].get(), static_cast<std::uint32_t>(index));
    bool owned = values.size() == plan.valueStorage.Size() && blocks.size() == plan.blockStorage.size();
    std::uint32_t current = 0;
    const auto valueIndex = [&](const IrValue* value) {
        if (value == nullptr) return NoPlanIndex;
        const auto found = values.find(value);
        if (found == values.end()) {
            owned = false;
            return NoPlanIndex;
        }
        return found->second;
    };
    writer.Value(plan.stage);
    writer.Value(plan.shaderHash);
    writer.Value(plan.userDataBase);
    writer.Value(plan.userDataCount);
    writer.Value(plan.srgbDecodeFormats);
    writer.List(plan.blockStorage, [](Writer& out, const std::unique_ptr<IrBlock>& block) { out.Value(block->Id()); });
    writer.Value<std::uint64_t>(plan.valueStorage.Size());
    for (const auto& stored : plan.valueStorage) {
        const auto* value = &stored;
        const auto bank = static_cast<std::uint32_t>(value->Register().bank);
        if (bank > 0xfu) owned = false;
        writer.Compact(static_cast<std::uint32_t>(value->Opcode()));
        writer.Compact(static_cast<std::uint32_t>(value->Type()));
        writer.Compact(value->Id());
        writer.Compact(value->Flags<std::uint64_t>());
        writer.Compact((static_cast<std::uint64_t>(value->Register().index) << 4u) | (bank & 0xfu));
        writer.Compact((static_cast<std::uint64_t>(value->ArgumentCount()) << 1u) | (value->HasImmediate() ? 1u : 0u));
        if (value->HasImmediate()) writer.Compact(value->ImmediateU64());
        for (const auto* argument : value->Arguments()) {
            const auto index = valueIndex(argument);
            const auto delta = static_cast<std::int64_t>(index) - static_cast<std::int64_t>(current);
            writer.Compact(argument == nullptr ? 0u : ((static_cast<std::uint64_t>(delta) << 1u) ^ static_cast<std::uint64_t>(delta >> 63)) + 1u);
        }
        if (value->IsPhi() && value->PhiBlockCount() != value->ArgumentCount()) owned = false;
        for (std::size_t operand = 0; value->IsPhi() && operand < value->ArgumentCount(); ++operand) {
            const auto found = operand < value->PhiBlockCount() ? blocks.find(value->PhiBlock(operand)) : blocks.end();
            if (found == blocks.end()) owned = false;
            writer.Compact(found != blocks.end() ? found->second : 0u);
        }
        ++current;
    }
    writer.List(plan.memoryInfo, encodeMemoryInfo);
    writer.List(plan.descriptorSources, [&](Writer& out, const DescriptorSource& source) {
        for (const auto* dword : source.dwords) out.Value(valueIndex(dword));
        out.Value(source.dwordCount);
        const auto column = source.tableColumn.value_or(TableColumn{});
        out.Value(source.tableColumn.has_value());
        out.Value(column.heapSource);
        out.Value(column.stride);
        out.Value(column.addend);
        out.Value(column.offset);
        out.Value(column.dwordCount);
        out.Value(column.maxKey);
        out.Value(column.sampler);
        out.Value(column.address);
        out.Value(column.buffer);
        out.List(column.keyDomains, [](Writer& domains, const KeyDomain& domain) {
            domains.Value(domain.source);
            domains.Value(domain.offset);
            domains.Value(domain.stride);
            domains.Value(domain.value);
        });
        out.Value(column.unkeyed);
        out.Value(column.indirect);
        out.Value(column.indirectOffset);
        out.Value(column.indirectImmediate);
        out.List(column.pointerHops, [](Writer& hops, const PointerHop& hop) {
            hops.Value(hop.offset);
            hops.Value(hop.immediate);
        });
        out.Value(column.foldedAddend);
        out.Value(column.foldedCarry);
        out.Value(column.selected);
        out.Value(column.selection);
    });
    writer.List(plan.controlFlow, [&](Writer& out, const ResourceBlock& block) {
        out.Value(valueIndex(block.condition));
        out.Values(std::span<const std::uint32_t>(block.successors));
        out.Values(std::span<const std::uint32_t>(block.sources));
        out.Values(std::span<const std::uint32_t>(block.images));
        out.Values(std::span<const std::uint32_t>(block.flatSlots));
    });
    writer.Values(std::span<const std::uint32_t>(plan.materializationSources));
    writer.List(plan.srtReads, [&](Writer& out, const SrtRead& read) {
        out.Value(valueIndex(read.value));
        out.Value(read.flatOffset);
    });
    writer.Values(std::span<const std::uint32_t>(plan.guardedSrtSlots));
    writer.Value(plan.srtGuardOffset);
    writer.Values(std::span<const std::uint8_t>(plan.cleanFlatSlots));
    writer.Values(std::span<const std::uint8_t>(plan.pureFlatSlots));
    writer.Value(plan.denseValueIds);
    writer.Value(plan.requiresSpecializationMemory);
    writer.Value(plan.srtPlanComplete);
    writer.Value(plan.resourceTrackingComplete);
    encodeShaderInfo(writer, plan.info);
    const auto& fill = plan.uniformFill.fill;
    writer.Value(fill.kind);
    writer.Value(fill.resource);
    for (const auto stride : fill.groupStride) writer.Value(stride);
    writer.Value(fill.words);
    writer.Value(fill.value);
    for (const auto* value : plan.uniformFill.values) writer.Value(valueIndex(value));
    return owned;
}

struct PlanValueRecord {
    IrOpcode opcode = IrOpcode::Identity;
    IrType type = IrType::Void;
    std::uint32_t id = 0;
    std::uint64_t flags = 0;
    bool hasImmediate = false;
    std::uint64_t immediate = 0;
    GuestRegister registerValue{};
    std::vector<std::uint32_t> arguments;
    std::vector<std::uint32_t> phiBlocks;
};

bool decodePlan(Reader& reader, IrResourcePlan& plan) {
    reader.Value(plan.stage);
    reader.Value(plan.shaderHash);
    reader.Value(plan.userDataBase);
    reader.Value(plan.userDataCount);
    reader.Value(plan.srgbDecodeFormats);
    std::vector<std::uint32_t> blockIds;
    reader.Values(blockIds);
    std::vector<PlanValueRecord> records;
    std::uint32_t current = 0;
    reader.List(records, 6, [&](Reader& in, PlanValueRecord& record) {
        constexpr std::uint64_t Word = std::numeric_limits<std::uint32_t>::max();
        const auto opcode = in.Compact();
        const auto type = in.Compact();
        const auto id = in.Compact();
        record.flags = in.Compact();
        const auto registerValue = in.Compact();
        const auto shape = in.Compact();
        const auto arguments = shape >> 1u;
        if (opcode >= static_cast<std::uint64_t>(IrOpcode::Count) || type > Word || id > Word || (registerValue >> 4u) > Word || arguments > in.Remaining()) {
            in.Fail();
            return;
        }
        record.opcode = static_cast<IrOpcode>(opcode);
        record.type = static_cast<IrType>(type);
        record.id = static_cast<std::uint32_t>(id);
        record.registerValue = {static_cast<RegisterBank>(registerValue & 0xfu), static_cast<std::uint32_t>(registerValue >> 4u)};
        record.hasImmediate = (shape & 1u) != 0u;
        if (record.hasImmediate) record.immediate = in.Compact();
        record.arguments.resize(static_cast<std::size_t>(arguments));
        for (auto& argument : record.arguments) {
            const auto code = in.Compact();
            if (code == 0u) {
                argument = NoPlanIndex;
                continue;
            }
            const auto zigzag = code - 1u;
            const auto delta = static_cast<std::int64_t>(zigzag >> 1u) ^ -static_cast<std::int64_t>(zigzag & 1u);
            const auto index = static_cast<std::int64_t>(current) + delta;
            if (index < 0 || index >= static_cast<std::int64_t>(records.size())) {
                in.Fail();
                return;
            }
            argument = static_cast<std::uint32_t>(index);
        }
        if (record.opcode == IrOpcode::Phi) {
            record.phiBlocks.resize(record.arguments.size());
            for (auto& block : record.phiBlocks) {
                const auto index = in.Compact();
                block = index < blockIds.size() ? static_cast<std::uint32_t>(index) : NoPlanIndex;
                if (block == NoPlanIndex) in.Fail();
            }
        }
        ++current;
    });
    if (!reader.Ok()) return false;
    plan.blockStorage.reserve(blockIds.size());
    for (const auto id : blockIds) plan.blockStorage.push_back(std::make_unique<IrBlock>(id));
    plan.valueStorage.Reserve(records.size());
    for (const auto& record : records) {
        auto* value = plan.valueStorage.Create(record.opcode, record.type, record.id);
        value->SetFlags(record.flags);
        if (record.hasImmediate) value->SetImmediateU64(record.immediate);
        value->SetRegister(record.registerValue);
    }
    bool valid = true;
    const auto valueAt = [&](std::uint32_t index) -> IrValue* {
        if (index == NoPlanIndex) return nullptr;
        if (index >= plan.valueStorage.Size()) {
            valid = false;
            return nullptr;
        }
        return &plan.valueStorage[index];
    };
    for (std::size_t index = 0; index < records.size(); ++index) {
        const auto& record = records[index];
        auto& value = plan.valueStorage[index];
        std::vector<IrValue*> arguments;
        std::vector<IrBlock*> phiBlocks;
        arguments.reserve(record.arguments.size());
        for (std::size_t operand = 0; operand < record.arguments.size(); ++operand) {
            arguments.push_back(valueAt(record.arguments[operand]));
            if (value.IsPhi()) phiBlocks.push_back(plan.blockStorage[record.phiBlocks[operand]].get());
        }
        value.SetDetachedOperands(std::move(arguments), std::move(phiBlocks));
    }
    reader.List(plan.memoryInfo, 79, decodeMemoryInfo);
    reader.List(plan.descriptorSources, 73, [&](Reader& in, DescriptorSource& source) {
        for (auto& dword : source.dwords) dword = valueAt(in.Get<std::uint32_t>());
        in.Value(source.dwordCount);
        const bool table = in.Get<bool>();
        TableColumn column;
        in.Value(column.heapSource);
        in.Value(column.stride);
        in.Value(column.addend);
        in.Value(column.offset);
        in.Value(column.dwordCount);
        in.Value(column.maxKey);
        in.Value(column.sampler);
        in.Value(column.address);
        in.Value(column.buffer);
        in.List(column.keyDomains, 13, [](Reader& domains, KeyDomain& domain) {
            domains.Value(domain.source);
            domains.Value(domain.offset);
            domains.Value(domain.stride);
            domains.Value(domain.value);
        });
        in.Value(column.unkeyed);
        in.Value(column.indirect);
        in.Value(column.indirectOffset);
        in.Value(column.indirectImmediate);
        in.List(column.pointerHops, 8, [](Reader& hops, PointerHop& hop) {
            hops.Value(hop.offset);
            hops.Value(hop.immediate);
        });
        in.Value(column.foldedAddend);
        in.Value(column.foldedCarry);
        in.Value(column.selected);
        in.Value(column.selection);
        source.tableColumn = table ? std::optional(std::move(column)) : std::nullopt;
    });
    reader.List(plan.controlFlow, 36, [&](Reader& in, ResourceBlock& block) {
        block.condition = valueAt(in.Get<std::uint32_t>());
        in.Values(block.successors);
        in.Values(block.sources);
        in.Values(block.images);
        in.Values(block.flatSlots);
    });
    reader.Values(plan.materializationSources);
    reader.List(plan.srtReads, 8, [&](Reader& in, SrtRead& read) {
        read.value = valueAt(in.Get<std::uint32_t>());
        in.Value(read.flatOffset);
    });
    reader.Values(plan.guardedSrtSlots);
    reader.Value(plan.srtGuardOffset);
    reader.Values(plan.cleanFlatSlots);
    reader.Values(plan.pureFlatSlots);
    reader.Value(plan.denseValueIds);
    reader.Value(plan.requiresSpecializationMemory);
    reader.Value(plan.srtPlanComplete);
    reader.Value(plan.resourceTrackingComplete);
    decodeShaderInfo(reader, plan.info);
    auto& fill = plan.uniformFill.fill;
    reader.Value(fill.kind);
    reader.Value(fill.resource);
    for (auto& stride : fill.groupStride) reader.Value(stride);
    reader.Value(fill.words);
    reader.Value(fill.value);
    for (auto& value : plan.uniformFill.values) value = valueAt(reader.Get<std::uint32_t>());
    if (!valid || !reader.Done() || fill.words > plan.uniformFill.values.size()) return false;
    const auto knownSource = [&](std::uint32_t source) { return source < plan.descriptorSources.size(); };
    if (!std::ranges::all_of(plan.materializationSources, knownSource)) return false;
    if (!std::ranges::all_of(plan.descriptorSources, [&](const DescriptorSource& source) { return source.dwordCount <= source.dwords.size() && (!source.tableColumn.has_value() || (knownSource(source.tableColumn->heapSource) && (!source.tableColumn->selected || knownSource(source.tableColumn->selection)) && std::ranges::all_of(source.tableColumn->keyDomains, [&](const KeyDomain& domain) { return knownSource(domain.source); }))); })) return false;
    for (const auto& source : plan.descriptorSources) {
        if (!source.tableColumn.has_value()) continue;
        const auto& column = *source.tableColumn;
        if (column.buffer && (column.sampler || column.dwordCount != 4u)) return false;
        const auto& parent = plan.descriptorSources[column.heapSource].tableColumn;
        if (parent.has_value() && (column.buffer || column.sampler || column.address || column.indirect || !parent->buffer || !parent->indirect || plan.descriptorSources[parent->heapSource].tableColumn.has_value())) return false;
    }
    if (!std::ranges::all_of(plan.info.buffers, [&](const BufferResource& buffer) { return knownSource(buffer.source); })) return false;
    if (!std::ranges::all_of(plan.info.images, [&](const ImageResource& image) { return knownSource(image.source); })) return false;
    if (!std::ranges::all_of(plan.info.samplers, [&](const SamplerResource& sampler) { return knownSource(sampler.source); })) return false;
    const bool blocksValid = std::ranges::all_of(plan.controlFlow, [&](const ResourceBlock& block) {
        return (block.condition == nullptr || block.successors.size() == 2u) &&
               std::ranges::all_of(block.successors, [&](std::uint32_t successor) { return successor < plan.controlFlow.size(); }) &&
               std::ranges::all_of(block.sources, knownSource) &&
               std::ranges::all_of(block.images, [&](std::uint32_t image) { return image < plan.info.images.size(); }) &&
               std::ranges::all_of(block.flatSlots, [&](std::uint32_t slot) { return slot < plan.srtReads.size(); });
    });
    if (!blocksValid) return false;
    ResourceMaterializer::CompactPlan(plan);
    return true;
}

constexpr std::string_view NeutralSwitches[] = {
    "APS5_PROFILE_DRAW",
    "APS5_DUMP_IR",
    "APS5_NO_CODE_HASH_KEY",
    "APS5_NO_FAILURE_MEMO",
    "APS5_NO_PERF_FRONTEND_PAIR",
    "APS5_NO_PERF_DISK_PLANS",
    "APS5_NO_PERF_DOM_INTERSECTION",
    "APS5_NO_PERF_ITERATION_SELECTION",
    "APS5_NO_PERF_LAYOUT_PAIR",
    "APS5_NO_RESULT_MEMO",
    "APS5_NO_TABLE_RESOLUTION_REUSE",
    "APS5_NO_TABLE_SNAPSHOT_REUSE",
};

const std::vector<std::byte>& switchKey() {
    static const std::vector<std::byte> key = [] {
        std::vector<std::byte> bytes;
        Writer writer(bytes);
        for (const auto name : Generated::RecompilerSwitches) {
            if (std::find(std::begin(NeutralSwitches), std::end(NeutralSwitches), name) != std::end(NeutralSwitches)) continue;
            const char* value = std::getenv(std::string(name).c_str());
            if (value == nullptr) continue;
            writer.Text(std::string(name));
            writer.Text(value);
        }
        return bytes;
    }();
    return key;
}

std::string hex(std::uint64_t value) {
    char text[17];
    std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(value));
    return text;
}

std::vector<std::byte> wrapEntry(std::span<const std::byte> key, std::span<const std::byte> payload) {
    const FileHeader header{FileMagic, FormatVersion, SourceVersion(), key.size(), payload.size(), HashBytes(key), HashBytes(payload)};
    const auto payloadOffset = PayloadOffset(key.size());
    std::vector<std::byte> file(payloadOffset + payload.size());
    std::memcpy(file.data(), &header, sizeof(header));
    std::memcpy(file.data() + sizeof(header), key.data(), key.size());
    if (!payload.empty()) std::memcpy(file.data() + payloadOffset, payload.data(), payload.size());
    return file;
}

LoadStatus unwrapEntry(std::span<const std::byte> file, std::span<const std::byte> key, std::span<const std::byte>& payload) {
    if (file.size() < sizeof(FileHeader)) return LoadStatus::Rejected;
    FileHeader header;
    std::memcpy(&header, file.data(), sizeof(header));
    if (header.magic != FileMagic || header.format != FormatVersion || header.sourceVersion != SourceVersion()) return LoadStatus::Rejected;
    const auto body = file.size() - sizeof(header);
    if (header.keyBytes > body) return LoadStatus::Rejected;
    const auto payloadOffset = PayloadOffset(static_cast<std::size_t>(header.keyBytes));
    if (payloadOffset > file.size() || header.payloadBytes != file.size() - payloadOffset) return LoadStatus::Rejected;
    const auto storedKey = file.subspan(sizeof(header), static_cast<std::size_t>(header.keyBytes));
    const auto padding = file.subspan(sizeof(header) + storedKey.size(), payloadOffset - sizeof(header) - storedKey.size());
    if (std::ranges::any_of(padding, [](std::byte value) { return value != std::byte{0}; })) return LoadStatus::Rejected;
    payload = file.subspan(payloadOffset);
    if (HashBytes(storedKey) != header.keyHash) return LoadStatus::Rejected;
    if (storedKey.size() != key.size() || !std::equal(storedKey.begin(), storedKey.end(), key.begin())) return LoadStatus::KeyMismatch;
    if (HashBytes(payload) != header.payloadHash) return LoadStatus::Rejected;
    return LoadStatus::Loaded;
}

std::atomic<unsigned> runningWriters{0};

class DiskStore {
public:
    explicit DiskStore(bool stopped) {
        const auto root = ShaderCacheDirectory();
        if (root.empty()) return;
        this->root = root;
        directory = root / hex(SourceVersion());
        discoverPrebuilt();
        stopping = stopped;
        if (stopping) return;
        writer = std::thread([this] { run(); });
        runningWriters.fetch_add(1, std::memory_order_acq_rel);
    }

    [[nodiscard]] bool Enabled() const { return !directory.empty(); }
    [[nodiscard]] const std::filesystem::path& Directory() const { return directory; }

    bool Load(std::span<const std::byte> key, CompiledVariant& variant) {
        if (loadPrebuilt(key, variant)) return true;
        return load(key, [&](std::span<const std::byte> file, std::shared_ptr<const void> owner) { return DecodeEntry(file, key, variant, std::move(owner)); }, hits, misses, loadFailures);
    }

    bool LoadPlan(std::span<const std::byte> key, IrResourcePlan& plan) {
        return load(key, [&](std::span<const std::byte> file, std::shared_ptr<const void>) { return DecodePlanEntry(file, key, plan); }, planHits, planMisses, planLoadFailures);
    }

    void Store(std::vector<std::byte> key, std::shared_ptr<const CompiledVariant> variant) {
        enqueue({std::move(key), std::move(variant), nullptr});
    }

    void StorePlan(std::vector<std::byte> key, std::shared_ptr<const IrResourcePlan> plan) {
        enqueue({std::move(key), nullptr, std::move(plan)});
    }

    std::shared_ptr<const CompiledVariant> Persist(std::span<const std::byte> key, const CompiledVariant& variant) {
        const auto path = directory / EntryName(key);
        const auto file = EncodeEntry(key, variant);
        if (!WriteFileAtomically(path, file)) return nullptr;
        writes.fetch_add(1, std::memory_order_relaxed);
        bytesWritten.fetch_add(file.size(), std::memory_order_relaxed);
        auto mapped = MapFile(path);
        if (mapped == nullptr) return nullptr;
        auto persisted = std::make_shared<CompiledVariant>();
        const auto bytes = mapped->Bytes();
        if (DecodeEntry(bytes, key, *persisted, std::move(mapped)) != LoadStatus::Loaded) return nullptr;
        persisted->layout = variant.layout;
        persisted->artifact.variantId = variant.artifact.variantId;
        return persisted;
    }

    void Flush() {
        std::unique_lock lock(mutex);
        idle.wait(lock, [&] { return jobs.empty() && !writing; });
    }

    void Shutdown() {
        std::thread stopped;
        {
            std::lock_guard lock(mutex);
            stopping = true;
            stopped = std::move(writer);
        }
        wake.notify_all();
        if (!stopped.joinable()) return;
        if (stopped.get_id() == std::this_thread::get_id()) stopped.detach();
        else stopped.join();
    }

    Counters Totals() const {
        Counters counters;
        counters.hits = hits.load(std::memory_order_relaxed);
        counters.misses = misses.load(std::memory_order_relaxed);
        counters.writes = writes.load(std::memory_order_relaxed);
        counters.loadFailures = loadFailures.load(std::memory_order_relaxed);
        counters.writeFailures = writeFailures.load(std::memory_order_relaxed);
        counters.bytesRead = bytesRead.load(std::memory_order_relaxed);
        counters.bytesWritten = bytesWritten.load(std::memory_order_relaxed);
        counters.planHits = planHits.load(std::memory_order_relaxed);
        counters.planMisses = planMisses.load(std::memory_order_relaxed);
        counters.planWrites = planWrites.load(std::memory_order_relaxed);
        counters.planLoadFailures = planLoadFailures.load(std::memory_order_relaxed);
        counters.prebuiltHits = prebuiltHits.load(std::memory_order_relaxed);
        counters.prebuiltMisses = prebuiltMisses.load(std::memory_order_relaxed);
        counters.prebuiltRejected = prebuiltRejected.load(std::memory_order_relaxed);
        counters.lookupNanoseconds = lookupNanoseconds.load(std::memory_order_relaxed);
        counters.readNanoseconds = readNanoseconds.load(std::memory_order_relaxed);
        counters.decodeNanoseconds = decodeNanoseconds.load(std::memory_order_relaxed);
        counters.loadNanoseconds = loadNanoseconds.load(std::memory_order_relaxed);
        counters.maxLoadNanoseconds = maxLoadNanoseconds.load(std::memory_order_relaxed);
        counters.prebuiltBytesRead = prebuiltBytesRead.load(std::memory_order_relaxed);
        counters.prebuiltLoadNanoseconds = prebuiltLoadNanoseconds.load(std::memory_order_relaxed);
        return counters;
    }

private:
    void discoverPrebuilt() {
        const auto start = std::chrono::steady_clock::now();
        std::size_t entries = 0;
        const auto addPack = [&](const std::filesystem::path& path) {
            auto pack = std::make_unique<CachePack>(path);
            if (!pack->Valid()) {
                std::fprintf(stderr, "[shader-prebuilt] ignored %s: %s\n", path.string().c_str(), pack->Error().c_str());
                return;
            }
            entries += pack->Count();
            packs.push_back(std::move(pack));
        };
        for (const auto& candidate : PrebuiltCachePaths(root)) {
            std::error_code error;
#ifdef _WIN32
            const auto path = NativeCachePath_nid_no_patch(candidate, error);
            if (error) continue;
#else
            const auto& path = candidate;
#endif
            if (std::filesystem::is_regular_file(path, error)) { addPack(path); continue; }
            if (!std::filesystem::is_directory(path, error)) continue;
            bool loose = false;
            std::vector<std::filesystem::path> packPaths;
            const auto collect = [&](const std::filesystem::path& entry) {
                const auto extension = entry.extension();
                if (extension == ".pack") packPaths.push_back(entry);
                if (extension == ".bin" && entry.stem().string().size() == 32) loose = true;
            };
#ifdef _WIN32
            WIN32_FIND_DATAW data{};
            const auto handle = FindFirstFileW((path / L"*").c_str(), &data);
            if (handle != INVALID_HANDLE_VALUE) {
                struct Search { HANDLE handle; ~Search() { FindClose(handle); } } search{handle};
                do { collect(path / data.cFileName); } while (FindNextFileW(handle, &data));
            }
#else
            for (std::filesystem::directory_iterator it(path, error), end; !error && it != end; it.increment(error)) collect(it->path());
#endif
            std::sort(packPaths.begin(), packPaths.end());
            for (const auto& item : packPaths) addPack(item);
            if (loose) looseDirectories.push_back(path);
            const auto versioned = path / hex(SourceVersion());
            if (std::filesystem::is_directory(versioned, error)) looseDirectories.push_back(versioned);
        }
        if (!packs.empty() || !looseDirectories.empty()) std::fprintf(stderr, "[shader-prebuilt] mapped %zu packs, %zu entries, %zu loose folders in %.3f ms; exact source/ABI/request keys required\n", packs.size(), entries, looseDirectories.size(), std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count());
    }

    struct Job {
        std::vector<std::byte> key;
        std::shared_ptr<const CompiledVariant> variant;
        std::shared_ptr<const IrResourcePlan> plan;
    };

    bool loadPrebuilt(std::span<const std::byte> key, CompiledVariant& variant) {
        if (packs.empty() && looseDirectories.empty()) return false;
        const auto started = std::chrono::steady_clock::now();
        const auto elapsed = [](auto start) { return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - start).count()); };
        bool loaded = false;
        PackLoadTimings timing;
        for (const auto& pack : packs) {
            const auto status = pack->Load(key, variant, timing);
            if (status == LoadStatus::Loaded) { loaded = true; break; }
            if (status == LoadStatus::Rejected) prebuiltRejected.fetch_add(1, std::memory_order_relaxed);
        }
        lookupNanoseconds.fetch_add(timing.lookupNanoseconds, std::memory_order_relaxed);
        decodeNanoseconds.fetch_add(timing.decodeNanoseconds, std::memory_order_relaxed);
        prebuiltBytesRead.fetch_add(timing.bytes, std::memory_order_relaxed);
        if (!loaded) {
            const auto name = EntryName(key);
            thread_local std::vector<std::byte>* fileSlot = nullptr;
            auto& file = ThreadOwned(fileSlot);
            for (const auto& path : looseDirectories) {
                const auto start = std::chrono::steady_clock::now();
                const auto read = ReadWholeFile(path / name, file);
                readNanoseconds.fetch_add(elapsed(start), std::memory_order_relaxed);
                if (!read) continue;
                const auto decode = std::chrono::steady_clock::now();
                const auto status = DecodeEntry(file, key, variant);
                decodeNanoseconds.fetch_add(elapsed(decode), std::memory_order_relaxed);
                prebuiltBytesRead.fetch_add(file.size(), std::memory_order_relaxed);
                if (status == LoadStatus::Rejected) {
                    const auto failures = prebuiltRejected.fetch_add(1, std::memory_order_relaxed);
                    if (failures < 8) std::fprintf(stderr, "[shader-prebuilt] rejected %s (%zu bytes): truncated, corrupt or another format; recompiling\n", name.c_str(), file.size());
                }
                if (status == LoadStatus::Loaded) { loaded = true; break; }
            }
            if (file.capacity() > (4u << 20u)) std::vector<std::byte>().swap(file);
        }
        if (loaded) {
            const auto previous = prebuiltHits.fetch_add(1, std::memory_order_relaxed);
            if (previous == 0) std::fprintf(stderr, "[shader-prebuilt] first exact-key hit loaded in %.3f ms\n", elapsed(started) / 1e6);
        } else prebuiltMisses.fetch_add(1, std::memory_order_relaxed);
        if (loaded) {
            const auto duration = elapsed(started);
            prebuiltLoadNanoseconds.fetch_add(duration, std::memory_order_relaxed);
            auto maximum = maxLoadNanoseconds.load(std::memory_order_relaxed);
            while (maximum < duration && !maxLoadNanoseconds.compare_exchange_weak(maximum, duration, std::memory_order_relaxed)) {}
            report(false);
        }
        return loaded;
    }

    template<typename TDecode>
    bool load(std::span<const std::byte> key, TDecode&& decode, std::atomic<std::uint64_t>& hitCount, std::atomic<std::uint64_t>& missCount, std::atomic<std::uint64_t>& failureCount) {
        const auto started = std::chrono::steady_clock::now();
        const auto name = EntryName(key);
        thread_local std::vector<std::byte>* fileSlot = nullptr;
        auto& file = ThreadOwned(fileSlot);
        bool loaded = false;
        std::span<const std::byte> bytes;
        std::shared_ptr<const void> owner;
        if (auto mapped = MapFile(directory / name)) {
            bytes = mapped->Bytes();
            owner = std::move(mapped);
        }
        const bool present = owner != nullptr || ReadWholeFile(directory / name, file);
        if (owner == nullptr) bytes = file;
        if (!present) {
            missCount.fetch_add(1, std::memory_order_relaxed);
        } else {
            switch (decode(bytes, std::move(owner))) {
                case LoadStatus::Loaded:
                    hitCount.fetch_add(1, std::memory_order_relaxed);
                    bytesRead.fetch_add(bytes.size(), std::memory_order_relaxed);
                    loaded = true;
                    break;
                case LoadStatus::Absent:
                case LoadStatus::KeyMismatch:
                    missCount.fetch_add(1, std::memory_order_relaxed);
                    break;
                case LoadStatus::Rejected: {
                    const auto failures = failureCount.fetch_add(1, std::memory_order_relaxed);
                    if (failures < 8) std::fprintf(stderr, "[shader-disk-cache] rejected %s (%zu bytes): truncated, corrupt or another format; recompiling\n", name.c_str(), bytes.size());
                    break;
                }
            }
        }
        if (file.capacity() > (4u << 20u)) std::vector<std::byte>().swap(file);
        loadNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
        report(false);
        return loaded;
    }

    void enqueue(Job job) {
        {
            std::unique_lock lock(mutex);
            if (stopping) {
                lock.unlock();
                write(job);
                return;
            }
            jobs.push_back(std::move(job));
        }
        wake.notify_one();
    }

    void run() {
        std::fprintf(stderr, "[shader-disk-cache] %s (source version %s, format %u)\n", directory.string().c_str(), hex(SourceVersion()).c_str(), FormatVersion);
        housekeeping();
        std::unique_lock lock(mutex);
        while (true) {
            wake.wait_for(lock, std::chrono::seconds(10), [&] { return !jobs.empty() || stopping; });
            while (!jobs.empty()) {
                auto job = std::move(jobs.front());
                jobs.pop_front();
                writing = true;
                lock.unlock();
                write(job);
                job = {};
                lock.lock();
                writing = false;
            }
            idle.notify_all();
            if (stopping) break;
            lock.unlock();
            report(false);
            lock.lock();
        }
        runningWriters.fetch_sub(1, std::memory_order_acq_rel);
    }

    void write(const Job& job) {
        const auto file = job.plan != nullptr ? EncodePlanEntry(job.key, *job.plan) : EncodeEntry(job.key, *job.variant);
        if (!file.empty() && WriteFileAtomically(directory / EntryName(job.key), file)) {
            (job.plan != nullptr ? planWrites : writes).fetch_add(1, std::memory_order_relaxed);
            bytesWritten.fetch_add(file.size(), std::memory_order_relaxed);
        } else {
            const auto failures = writeFailures.fetch_add(1, std::memory_order_relaxed);
            if (failures < 4) std::fprintf(stderr, "[shader-disk-cache] cannot write %s\n", (directory / EntryName(job.key)).string().c_str());
        }
    }

    void housekeeping() {
        std::error_code error;
        std::filesystem::create_directories(directory, error);
        const auto stamp = directory / "last-used";
        std::ofstream(stamp, std::ios::binary | std::ios::trunc).close();
        std::filesystem::last_write_time(stamp, std::filesystem::file_time_type::clock::now(), error);
#ifdef _WIN32
        auto nativeRoot = root;
        nativeRoot.make_preferred();
        if (nativeRoot.native().starts_with(L"\\\\?\\") || nativeRoot.native().starts_with(L"\\\\.\\")) return;
#endif
        const auto now = std::filesystem::file_time_type::clock::now();
        for (std::filesystem::directory_iterator it(root, error), end; !error && it != end; it.increment(error)) {
            const auto name = it->path().filename().string();
            if (it->path() == directory || name.size() != 16 || name.find_first_not_of("0123456789abcdef") != std::string::npos || !it->is_directory(error)) continue;
            std::error_code timeError;
            auto used = std::filesystem::last_write_time(it->path() / "last-used", timeError);
            if (timeError) used = std::filesystem::last_write_time(it->path(), timeError);
            if (!timeError && now - used > std::chrono::hours(24 * 14)) {
                std::error_code removeError;
                std::filesystem::remove_all(it->path(), removeError);
            }
        }
        error.clear();
        for (std::filesystem::directory_iterator it(directory, error), end; !error && it != end; it.increment(error)) {
            if (it->path().filename().string().find(".tmp.") == std::string::npos) continue;
            std::error_code timeError;
            const auto written = std::filesystem::last_write_time(it->path(), timeError);
            if (!timeError && now - written > std::chrono::hours(1)) {
                std::error_code removeError;
                std::filesystem::remove(it->path(), removeError);
            }
        }
    }

    void report(bool force) {
        const auto now = std::chrono::steady_clock::now().time_since_epoch().count();
        auto last = lastReport.load(std::memory_order_relaxed);
        if (last == 0) {
            lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed);
            return;
        }
        if (!force && (now - last < std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::seconds(10)).count() || !lastReport.compare_exchange_strong(last, now, std::memory_order_relaxed))) return;
        std::lock_guard lock(reportMutex);
        const auto totals = Totals();
        const auto loadNs = loadNanoseconds.load(std::memory_order_relaxed);
        const auto delta = [](std::uint64_t after, std::uint64_t before) { return static_cast<unsigned long long>(after - before); };
        const auto baseChanged = totals.hits != reported.hits || totals.misses != reported.misses || totals.writes != reported.writes || totals.loadFailures != reported.loadFailures || totals.writeFailures != reported.writeFailures || totals.planHits != reported.planHits || totals.planMisses != reported.planMisses || totals.planWrites != reported.planWrites || totals.planLoadFailures != reported.planLoadFailures;
        const auto prebuiltChanged = totals.prebuiltHits != reported.prebuiltHits || totals.prebuiltMisses != reported.prebuiltMisses || totals.prebuiltRejected != reported.prebuiltRejected;
        if (!baseChanged && !prebuiltChanged) return;
        if (baseChanged) std::fprintf(stderr, "[shader-disk-cache] (10 s): %llu hits, %llu misses, %llu writes (%.1f MiB), %llu load failures, %llu write failures; resource plans: %llu hits, %llu misses, %llu writes, %llu load failures; %.1f MiB read in %.1f ms (totals: %llu hits, %llu misses, %llu writes; plans %llu hits, %llu misses)\n",
                     delta(totals.hits, reported.hits), delta(totals.misses, reported.misses), delta(totals.writes, reported.writes), static_cast<double>(totals.bytesWritten - reported.bytesWritten) / (1024.0 * 1024.0),
                     delta(totals.loadFailures, reported.loadFailures), delta(totals.writeFailures, reported.writeFailures),
                     delta(totals.planHits, reported.planHits), delta(totals.planMisses, reported.planMisses), delta(totals.planWrites, reported.planWrites), delta(totals.planLoadFailures, reported.planLoadFailures),
                     static_cast<double>(totals.bytesRead - reported.bytesRead) / (1024.0 * 1024.0), static_cast<double>(loadNs - reportedLoadNs) / 1e6,
                     static_cast<unsigned long long>(totals.hits), static_cast<unsigned long long>(totals.misses), static_cast<unsigned long long>(totals.writes), static_cast<unsigned long long>(totals.planHits), static_cast<unsigned long long>(totals.planMisses));
        if (prebuiltChanged && (!packs.empty() || !looseDirectories.empty())) std::fprintf(stderr, "[shader-prebuilt] interval: %llu hits, %llu misses, %llu rejected; lookup %.3f ms, file read %.3f ms, decode %.3f ms; total load %.3f ms, max %.3f ms (lifetime hits %llu)\n",
                     delta(totals.prebuiltHits, reported.prebuiltHits), delta(totals.prebuiltMisses, reported.prebuiltMisses), delta(totals.prebuiltRejected, reported.prebuiltRejected),
                     (totals.lookupNanoseconds - reported.lookupNanoseconds) / 1e6, (totals.readNanoseconds - reported.readNanoseconds) / 1e6, (totals.decodeNanoseconds - reported.decodeNanoseconds) / 1e6,
                     (totals.prebuiltLoadNanoseconds - reported.prebuiltLoadNanoseconds) / 1e6, totals.maxLoadNanoseconds / 1e6, static_cast<unsigned long long>(totals.prebuiltHits));
        reported = totals;
        reportedLoadNs = loadNs;
    }

    std::filesystem::path root;
    std::filesystem::path directory;
    std::vector<std::unique_ptr<CachePack>> packs;
    std::vector<std::filesystem::path> looseDirectories;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    std::deque<Job> jobs;
    bool writing = false;
    bool stopping = false;
    std::thread writer;
    std::atomic<std::uint64_t> hits{0}, misses{0}, writes{0}, loadFailures{0}, writeFailures{0}, bytesRead{0}, bytesWritten{0}, loadNanoseconds{0};
    std::atomic<std::uint64_t> planHits{0}, planMisses{0}, planWrites{0}, planLoadFailures{0};
    std::atomic<std::uint64_t> prebuiltHits{0}, prebuiltMisses{0}, prebuiltRejected{0}, lookupNanoseconds{0}, readNanoseconds{0}, decodeNanoseconds{0}, maxLoadNanoseconds{0}, prebuiltBytesRead{0}, prebuiltLoadNanoseconds{0};
    std::atomic<std::int64_t> lastReport{0};
    std::mutex reportMutex;
    Counters reported;
    std::uint64_t reportedLoadNs = 0;
};

struct StoreLifecycle {
    std::mutex mutex;
    DiskStore* created = nullptr;
    bool stopped = false;
};

StoreLifecycle& lifecycle() {
    static auto* state = new StoreLifecycle();
    return *state;
}

DiskStore& store() {
    static DiskStore* instance = [] {
        auto& state = lifecycle();
        std::lock_guard lock(state.mutex);
        state.created = new DiskStore(state.stopped);
        return state.created;
    }();
    return *instance;
}

}

std::uint64_t SourceVersion() {
    return Generated::SourceVersion;
}

void BuildKey(const RecompileRequest& request, std::uint32_t hostSubgroupSize, std::vector<std::byte>& key) {
    key.clear();
    Writer writer(key);
    writer.Value(FileMagic);
    writer.Value(FormatVersion);
    writer.Value(SourceVersion());
    writer.Value(static_cast<std::uint8_t>(GcnGuestShaders()));
    thread_local std::vector<std::uint64_t>* memoryKeySlot = nullptr;
    auto& memoryKey = ThreadOwned(memoryKeySlot);
    RecompileCacheKey::Build(request, memoryKey);
    writer.Values(std::span<const std::uint64_t>(memoryKey));
    writer.Values(request.shader.code);
    writer.Value(hostSubgroupSize);
    writer.Value(request.layout.descriptorSet);
    writer.Value(request.layout.firstBinding);
    writer.Value(request.layout.pushConstantOffsetBytes);
    writer.Value(request.layout.pushConstantSizeBytes);
    const auto& switches = switchKey();
    key.insert(key.end(), switches.begin(), switches.end());
}

void BuildPlanKey(const RecompileRequest& request, std::uint32_t hostSubgroupSize, std::vector<std::byte>& key) {
    key.clear();
    Writer writer(key);
    writer.Value(FileMagic);
    writer.Value(FormatVersion);
    writer.Value(SourceVersion());
    writer.Value(static_cast<std::uint8_t>(GcnGuestShaders()));
    writer.Value(PlanEntryKind);
    thread_local std::vector<std::uint64_t>* memoryKeySlot = nullptr;
    auto& memoryKey = ThreadOwned(memoryKeySlot);
    RecompileCacheKey::Build(request, memoryKey);
    writer.Values(std::span<const std::uint64_t>(memoryKey));
    writer.Values(request.shader.code);
    writer.Value(hostSubgroupSize);
    const auto& switches = switchKey();
    key.insert(key.end(), switches.begin(), switches.end());
}

std::string EntryName(std::span<const std::byte> key) {
    return hex(HashBytes(key, 0x5eed0001ull)) + hex(HashBytes(key, 0x5eed0002ull)) + ".bin";
}

void EncodeResult(const RecompileResult& result, std::vector<std::byte>& out) {
    Writer writer(out);
    encodeArtifact(writer, result);
    encodeInvocation(writer, result);
    writer.Value(result.workgroupMemoryDwords);
    writer.Values(std::span<const std::uint32_t>(result.shaderDataUserWords));
}

bool DecodeResult(std::span<const std::byte> bytes, RecompileResult& result, std::shared_ptr<const void> owner) {
    Reader reader(bytes, std::move(owner));
    decodeArtifact(reader, result);
    decodeInvocation(reader, result);
    reader.Value(result.workgroupMemoryDwords);
    reader.Values(result.shaderDataUserWords);
    result.cacheHit = false;
    return reader.Done() && result.runtimeAbiVersion == RuntimeAbi::Version;
}

std::vector<std::byte> EncodeEntry(std::span<const std::byte> key, const CompiledVariant& variant) {
    std::vector<std::byte> payload;
    payload.reserve(variant.artifact.spirv.size() * sizeof(std::uint32_t) + 4096);
    Writer writer(payload);
    encodeArtifact(writer, variant.artifact);
    encodeInfo(writer, variant.info);
    encodeAllocation(writer, variant.bindings);
    return wrapEntry(key, payload);
}

LoadStatus DecodeEntry(std::span<const std::byte> file, std::span<const std::byte> key, CompiledVariant& variant, std::shared_ptr<const void> owner) {
    std::span<const std::byte> payload;
    const auto status = unwrapEntry(file, key, payload);
    if (status != LoadStatus::Loaded) return status;
    Reader reader(payload, std::move(owner));
    CompiledVariant decoded;
    decodeArtifact(reader, decoded.artifact);
    decodeInfo(reader, decoded.info);
    decodeAllocation(reader, decoded.bindings);
    if (!reader.Done() || decoded.artifact.runtimeAbiVersion != RuntimeAbi::Version) return LoadStatus::Rejected;
    variant.info = std::move(decoded.info);
    variant.bindings = std::move(decoded.bindings);
    variant.artifact = std::move(decoded.artifact);
    return LoadStatus::Loaded;
}

bool EncodePlan(const IrResourcePlan& plan, std::vector<std::byte>& out) {
    Writer writer(out);
    return encodePlan(writer, plan);
}

bool DecodePlan(std::span<const std::byte> bytes, IrResourcePlan& plan) {
    Reader reader(bytes);
    IrResourcePlan decoded;
    if (!decodePlan(reader, decoded)) return false;
    plan = std::move(decoded);
    return true;
}

std::vector<std::byte> EncodePlanEntry(std::span<const std::byte> key, const IrResourcePlan& plan) {
    std::vector<std::byte> payload;
    if (!EncodePlan(plan, payload)) return {};
    return wrapEntry(key, payload);
}

LoadStatus DecodePlanEntry(std::span<const std::byte> file, std::span<const std::byte> key, IrResourcePlan& plan) {
    std::span<const std::byte> payload;
    const auto status = unwrapEntry(file, key, payload);
    if (status != LoadStatus::Loaded) return status;
    return DecodePlan(payload, plan) ? LoadStatus::Loaded : LoadStatus::Rejected;
}

bool Enabled() {
    return store().Enabled();
}

std::filesystem::path EntryDirectory() {
    return store().Directory();
}

bool Load(std::span<const std::byte> key, CompiledVariant& variant) {
    auto& instance = store();
    return instance.Enabled() && instance.Load(key, variant);
}

void Store(std::vector<std::byte> key, std::shared_ptr<const CompiledVariant> variant) {
    auto& instance = store();
    if (instance.Enabled()) instance.Store(std::move(key), std::move(variant));
}

bool LoadPlan(std::span<const std::byte> key, IrResourcePlan& plan) {
    auto& instance = store();
    return instance.Enabled() && instance.LoadPlan(key, plan);
}

void StorePlan(std::vector<std::byte> key, std::shared_ptr<const IrResourcePlan> plan) {
    auto& instance = store();
    if (instance.Enabled() && plan != nullptr) instance.StorePlan(std::move(key), std::move(plan));
}

std::shared_ptr<const CompiledVariant> Persist(std::span<const std::byte> key, const CompiledVariant& variant) {
    auto& instance = store();
    return instance.Enabled() ? instance.Persist(key, variant) : nullptr;
}

void Flush() {
    auto& instance = store();
    if (instance.Enabled()) instance.Flush();
}

void Shutdown() {
    DiskStore* instance = nullptr;
    {
        auto& state = lifecycle();
        std::lock_guard lock(state.mutex);
        state.stopped = true;
        instance = state.created;
    }
    if (instance != nullptr) instance->Shutdown();
}

unsigned Writers() {
    return runningWriters.load(std::memory_order_acquire);
}

Counters Totals() {
    return store().Totals();
}

}
