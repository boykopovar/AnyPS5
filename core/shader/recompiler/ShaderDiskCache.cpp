#include "ShaderDiskCache.hpp"
#include "CacheKey.hpp"
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
#endif
#include "ShaderCacheVersion.hpp"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <type_traits>

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
    const auto executable = std::filesystem::read_symlink("/proc/self/exe", error);
    if (error) return {};
    return executable.parent_path() / "shader_cache";
#endif
}

}

namespace ShaderRecompiler::ShaderDiskCache {

#if defined(__linux__) && defined(__x86_64__) && defined(__GLIBCXX__)
static_assert(sizeof(CompiledShaderArtifact) == 184, "CompiledShaderArtifact changed: update the artifact encoder");
static_assert(sizeof(ShaderInvocation) == 104, "ShaderInvocation changed: update the invocation encoder");
static_assert(sizeof(RecompileResult) == 296, "RecompileResult changed: update EncodeResult and DecodeResult");
static_assert(sizeof(DescriptorBinding) == 448, "DescriptorBinding changed: update the binding encoder");
static_assert(sizeof(VertexAttribute) == 32, "VertexAttribute changed: update the attribute encoder");
static_assert(sizeof(VertexInput) == 16, "VertexInput changed: update the vertex input encoder");
static_assert(sizeof(FragmentParameter) == 12, "FragmentParameter changed: update the parameter encoder");
static_assert(sizeof(CompiledShaderInfo) == 336, "CompiledShaderInfo changed: update the info encoder");
static_assert(sizeof(ShaderInfo) == 224, "ShaderInfo changed: update the info encoder");
static_assert(sizeof(BufferResource) == 32, "BufferResource changed: update the info encoder");
static_assert(sizeof(ImageResource) == 104, "ImageResource changed: update the info encoder");
static_assert(sizeof(SamplerResource) == 16, "SamplerResource changed: update the info encoder");
static_assert(sizeof(SampledResourcePair) == 12, "SampledResourcePair changed: update the info encoder");
static_assert(sizeof(StageInput) == 56, "StageInput changed: update the info encoder");
static_assert(sizeof(StageOutput) == 48, "StageOutput changed: update the info encoder");
static_assert(sizeof(IrBindingLayout) == 72, "IrBindingLayout changed: update the layout encoder");
static_assert(sizeof(IrDescriptorBinding) == 32, "IrDescriptorBinding changed: update the layout encoder");
static_assert(sizeof(BindingAllocationResult) == 152, "BindingAllocationResult changed: update the allocation encoder");
static_assert(sizeof(CompiledBindingLayout) == 80, "CompiledBindingLayout changed: update the allocation encoder");
static_assert(sizeof(BindingLayout) == 16, "BindingLayout changed: update BuildKey");
#endif

namespace {

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
    explicit Reader(std::span<const std::byte> bytes) : bytes(bytes) {}

    [[nodiscard]] bool Ok() const { return ok; }
    [[nodiscard]] bool Done() const { return ok && position == bytes.size(); }

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
    writer.Flags(binding.bufferAtomic);
    writer.Flags(binding.bufferWritten);
    writer.Flags(binding.samplerUnnormalized);
    writer.Flags(binding.imageUnnormalized);
    writer.Values(std::span<const std::uint32_t>(binding.imageSamplers));
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
    reader.Flags(binding.bufferAtomic);
    reader.Flags(binding.bufferWritten);
    reader.Flags(binding.samplerUnnormalized);
    reader.Flags(binding.imageUnnormalized);
    reader.Values(binding.imageSamplers);
}

void encodeArtifact(Writer& writer, const CompiledShaderArtifact& result) {
    writer.Value(result.runtimeAbiVersion);
    writer.Values(std::span<const std::uint32_t>(result.spirv.Words()));
    writer.Value(result.bdaAbiVersion);
    writer.Value(result.memoryOffsetDword);
    writer.Value(result.shaderDataDwords);
    writer.Value(result.imageMetadataDword);
    writer.Value(result.runtimeImageCount);
    writer.Values(std::span<const std::uint32_t>(result.runtimeImageResources));
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
    writer.Values(std::span<const std::uint32_t>(result.parameterExports));
    writer.List(result.fragmentParameters, [](Writer& out, const FragmentParameter& parameter) {
        out.Value(parameter.location);
        out.Value(parameter.sourceLocation);
        out.Value(parameter.flat);
        out.Value(parameter.perVertex);
        out.Value(parameter.custom);
    });
}

void decodeArtifact(Reader& reader, CompiledShaderArtifact& result) {
    reader.Value(result.runtimeAbiVersion);
    std::vector<std::uint32_t> words;
    reader.Values(words);
    result.spirv = std::move(words);
    reader.Value(result.bdaAbiVersion);
    reader.Value(result.memoryOffsetDword);
    reader.Value(result.shaderDataDwords);
    reader.Value(result.imageMetadataDword);
    reader.Value(result.runtimeImageCount);
    reader.Values(result.runtimeImageResources);
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
    reader.Values(result.parameterExports);
    reader.List(result.fragmentParameters, 10, [](Reader& in, FragmentParameter& parameter) {
        in.Value(parameter.location);
        in.Value(parameter.sourceLocation);
        in.Value(parameter.flat);
        in.Value(parameter.perVertex);
        in.Value(parameter.custom);
    });
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
    writer.List(invocation.vertexAttributes, [](Writer& out, const VertexAttribute& attribute) {
        out.Value(attribute.location);
        out.Value(attribute.components);
        for (const auto field : attribute.resource.fields) out.Value(field);
        out.Value(attribute.fetchIndex);
        out.Value(attribute.formatComponents);
    });
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
    reader.List(invocation.vertexAttributes, 32, [](Reader& in, VertexAttribute& attribute) {
        in.Value(attribute.location);
        in.Value(attribute.components);
        for (auto& field : attribute.resource.fields) in.Value(field);
        in.Value(attribute.fetchIndex);
        in.Value(attribute.formatComponents);
    });
}

void encodeLayout(Writer& writer, const IrBindingLayout& layout) {
    writer.Value(layout.pushDataStartDword);
    writer.Value(layout.memoryOffsetDword);
    writer.Value(layout.memoryOffsetCount);
    writer.Value(layout.dispatchThreadLimit);
    writer.Value(layout.runtimeImageCount);
    writer.Values(std::span<const std::uint32_t>(layout.userDataRegisters));
    writer.List(layout.descriptors, [](Writer& out, const IrDescriptorBinding& descriptor) {
        out.Value(descriptor.kind);
        out.Values(std::span<const std::uint32_t>(descriptor.resources));
    });
}

void decodeLayout(Reader& reader, IrBindingLayout& layout) {
    reader.Value(layout.pushDataStartDword);
    reader.Value(layout.memoryOffsetDword);
    reader.Value(layout.memoryOffsetCount);
    reader.Value(layout.dispatchThreadLimit);
    reader.Value(layout.runtimeImageCount);
    reader.Values(layout.userDataRegisters);
    reader.List(layout.descriptors, 12, [](Reader& in, IrDescriptorBinding& descriptor) {
        in.Value(descriptor.kind);
        in.Values(descriptor.resources);
    });
}

void encodeInfo(Writer& writer, const CompiledShaderInfo& compiled) {
    writer.Value(compiled.stage);
    writer.Value(compiled.shaderHash);
    writer.Value(compiled.waveSize);
    writer.Value(compiled.userDataBase);
    writer.Value(compiled.userDataCount);
    writer.Value(compiled.scratchDwords);
    writer.Value(compiled.paramExportMask);
    const auto& info = compiled.info;
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
        out.Value(image.packedFormat);
        out.Value(image.emulatedCompare);
        out.Value(image.indirectRoot);
        out.Value(image.indirectMappingOffset);
        out.Value(image.indirectSearchIterations);
        out.Values(std::span<const std::uint32_t>(image.indirectResources));
    });
    writer.Value(!info.runtimeImageModes.empty());
    writer.List(info.samplers, [](Writer& out, const SamplerResource& sampler) {
        out.Value(sampler.source);
        out.Value(sampler.firstUsePc);
        out.Value(sampler.copyOf);
        out.Value(sampler.forcePointFiltering);
        out.Value(sampler.depthCompare);
        out.Value(sampler.uses);
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
    writer.Value(info.dispatchThreadLimit);
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
    auto& info = compiled.info;
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
    reader.List(info.images, 72, [](Reader& in, ImageResource& image) {
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
        in.Value(image.packedFormat);
        in.Value(image.emulatedCompare);
        in.Value(image.indirectRoot);
        in.Value(image.indirectMappingOffset);
        in.Value(image.indirectSearchIterations);
        in.Values(image.indirectResources);
    });
    bool preparedImageModes = false;
    reader.Value(preparedImageModes);
    if (preparedImageModes) ResourceMaterializer::PrepareImageModes(info);
    reader.List(info.samplers, 15, [](Reader& in, SamplerResource& sampler) {
        in.Value(sampler.source);
        in.Value(sampler.firstUsePc);
        in.Value(sampler.copyOf);
        in.Value(sampler.forcePointFiltering);
        in.Value(sampler.depthCompare);
        in.Value(sampler.uses);
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
    reader.Value(info.dispatchThreadLimit);
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

constexpr std::string_view NeutralSwitches[] = {
    "APS5_PROFILE_DRAW",
    "APS5_DUMP_IR",
    "APS5_NO_CODE_HASH_KEY",
    "APS5_NO_FAILURE_MEMO",
    "APS5_NO_RESULT_MEMO",
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

class DiskStore {
public:
    DiskStore() {
        const auto root = ShaderCacheDirectory();
        if (root.empty()) return;
        this->root = root;
        directory = root / hex(SourceVersion());
        std::thread([this] { run(); }).detach();
    }

    [[nodiscard]] bool Enabled() const { return !directory.empty(); }
    [[nodiscard]] const std::filesystem::path& Directory() const { return directory; }

    bool Load(std::span<const std::byte> key, CompiledVariant& variant) {
        const auto started = std::chrono::steady_clock::now();
        const auto name = EntryName(key);
        thread_local std::vector<std::byte>* fileSlot = nullptr;
        auto& file = ThreadOwned(fileSlot);
        bool loaded = false;
        if (!ReadWholeFile(directory / name, file)) {
            misses.fetch_add(1, std::memory_order_relaxed);
        } else {
            switch (DecodeEntry(file, key, variant)) {
                case LoadStatus::Loaded:
                    hits.fetch_add(1, std::memory_order_relaxed);
                    bytesRead.fetch_add(file.size(), std::memory_order_relaxed);
                    loaded = true;
                    break;
                case LoadStatus::Absent:
                case LoadStatus::KeyMismatch:
                    misses.fetch_add(1, std::memory_order_relaxed);
                    break;
                case LoadStatus::Rejected: {
                    const auto failures = loadFailures.fetch_add(1, std::memory_order_relaxed);
                    if (failures < 8) std::fprintf(stderr, "[shader-disk-cache] rejected %s (%zu bytes): truncated, corrupt or another format; recompiling\n", name.c_str(), file.size());
                    break;
                }
            }
        }
        if (file.capacity() > (4u << 20u)) std::vector<std::byte>().swap(file);
        loadNanoseconds.fetch_add(static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::steady_clock::now() - started).count()), std::memory_order_relaxed);
        report(false);
        return loaded;
    }

    void Store(std::vector<std::byte> key, std::shared_ptr<const CompiledVariant> variant) {
        {
            std::lock_guard lock(mutex);
            jobs.push_back({std::move(key), std::move(variant)});
        }
        wake.notify_one();
    }

    void Flush() {
        std::unique_lock lock(mutex);
        idle.wait(lock, [&] { return jobs.empty() && !writing; });
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
        return counters;
    }

private:
    struct Job {
        std::vector<std::byte> key;
        std::shared_ptr<const CompiledVariant> variant;
    };

    void run() {
        std::fprintf(stderr, "[shader-disk-cache] %s (source version %s, format %u)\n", directory.string().c_str(), hex(SourceVersion()).c_str(), FormatVersion);
        housekeeping();
        std::unique_lock lock(mutex);
        while (true) {
            wake.wait_for(lock, std::chrono::seconds(10), [&] { return !jobs.empty(); });
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
            lock.unlock();
            report(false);
            lock.lock();
        }
    }

    void write(const Job& job) {
        const auto file = EncodeEntry(job.key, *job.variant);
        if (WriteFileAtomically(directory / EntryName(job.key), file)) {
            writes.fetch_add(1, std::memory_order_relaxed);
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
        if (totals.hits == reported.hits && totals.misses == reported.misses && totals.writes == reported.writes && totals.loadFailures == reported.loadFailures && totals.writeFailures == reported.writeFailures) return;
        std::fprintf(stderr, "[shader-disk-cache] (10 s): %llu hits, %llu misses, %llu writes (%.1f MiB), %llu load failures, %llu write failures; %.1f MiB read in %.1f ms (totals: %llu hits, %llu misses, %llu writes)\n",
                     delta(totals.hits, reported.hits), delta(totals.misses, reported.misses), delta(totals.writes, reported.writes), static_cast<double>(totals.bytesWritten - reported.bytesWritten) / (1024.0 * 1024.0),
                     delta(totals.loadFailures, reported.loadFailures), delta(totals.writeFailures, reported.writeFailures), static_cast<double>(totals.bytesRead - reported.bytesRead) / (1024.0 * 1024.0), static_cast<double>(loadNs - reportedLoadNs) / 1e6,
                     static_cast<unsigned long long>(totals.hits), static_cast<unsigned long long>(totals.misses), static_cast<unsigned long long>(totals.writes));
        reported = totals;
        reportedLoadNs = loadNs;
    }

    std::filesystem::path root;
    std::filesystem::path directory;
    std::mutex mutex;
    std::condition_variable wake;
    std::condition_variable idle;
    std::deque<Job> jobs;
    bool writing = false;
    std::atomic<std::uint64_t> hits{0}, misses{0}, writes{0}, loadFailures{0}, writeFailures{0}, bytesRead{0}, bytesWritten{0}, loadNanoseconds{0};
    std::atomic<std::int64_t> lastReport{0};
    std::mutex reportMutex;
    Counters reported;
    std::uint64_t reportedLoadNs = 0;
};

DiskStore& store() {
    static DiskStore* instance = new DiskStore();
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

std::string EntryName(std::span<const std::byte> key) {
    return hex(HashBytes(key, 0x5eed0001ull)) + hex(HashBytes(key, 0x5eed0002ull)) + ".bin";
}

void EncodeResult(const RecompileResult& result, std::vector<std::byte>& out) {
    Writer writer(out);
    encodeArtifact(writer, result);
    encodeInvocation(writer, result);
}

bool DecodeResult(std::span<const std::byte> bytes, RecompileResult& result) {
    Reader reader(bytes);
    decodeArtifact(reader, result);
    decodeInvocation(reader, result);
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
    const FileHeader header{FileMagic, FormatVersion, SourceVersion(), key.size(), payload.size(), HashBytes(key), HashBytes(payload)};
    std::vector<std::byte> file(sizeof(header) + key.size() + payload.size());
    std::memcpy(file.data(), &header, sizeof(header));
    std::memcpy(file.data() + sizeof(header), key.data(), key.size());
    std::memcpy(file.data() + sizeof(header) + key.size(), payload.data(), payload.size());
    return file;
}

LoadStatus DecodeEntry(std::span<const std::byte> file, std::span<const std::byte> key, CompiledVariant& variant) {
    if (file.size() < sizeof(FileHeader)) return LoadStatus::Rejected;
    FileHeader header;
    std::memcpy(&header, file.data(), sizeof(header));
    if (header.magic != FileMagic || header.format != FormatVersion || header.sourceVersion != SourceVersion()) return LoadStatus::Rejected;
    const auto body = file.size() - sizeof(header);
    if (header.keyBytes > body || header.payloadBytes != body - header.keyBytes) return LoadStatus::Rejected;
    const auto storedKey = file.subspan(sizeof(header), static_cast<std::size_t>(header.keyBytes));
    const auto payload = file.subspan(sizeof(header) + static_cast<std::size_t>(header.keyBytes));
    if (HashBytes(storedKey) != header.keyHash) return LoadStatus::Rejected;
    if (storedKey.size() != key.size() || !std::equal(storedKey.begin(), storedKey.end(), key.begin())) return LoadStatus::KeyMismatch;
    if (HashBytes(payload) != header.payloadHash) return LoadStatus::Rejected;
    Reader reader(payload);
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

void Flush() {
    auto& instance = store();
    if (instance.Enabled()) instance.Flush();
}

Counters Totals() {
    return store().Totals();
}

}
