#include "ShaderDiskCache.hpp"
#include "CacheKey.hpp"
#include "VertexInputSpecialization.hpp"
#include "SpirvBackend/SpirvSpecialization.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/DescriptorBindingBuilder.hpp"
#include "ShaderCacheDirectory.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include <algorithm>
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <functional>
#include <future>
#include <iostream>
#include <map>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

using namespace ShaderRecompiler;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void setEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

bool sameBinding(const DescriptorBinding& left, const DescriptorBinding& right) {
    return left.kind == right.kind && left.role == right.role && left.descriptorSet == right.descriptorSet && left.binding == right.binding && left.count == right.count && left.guestDescriptor == right.guestDescriptor && left.readOnly == right.readOnly && left.imageShape == right.imageShape && left.samplerDepthCompare == right.samplerDepthCompare && left.imageWritten == right.imageWritten && left.imageDepthCompare == right.imageDepthCompare && left.imageAtomic == right.imageAtomic && left.bufferAtomic == right.bufferAtomic && left.bufferWritten == right.bufferWritten && left.samplerUnnormalized == right.samplerUnnormalized && left.imageUnnormalized == right.imageUnnormalized && left.imageSamplers == right.imageSamplers;
}

bool sameBindings(const std::vector<DescriptorBinding>& left, const std::vector<DescriptorBinding>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t i = 0; i < left.size(); ++i) {
        if (!sameBinding(left[i], right[i])) return false;
    }
    return true;
}

void requireSameArtifact(const CompiledShaderArtifact& left, const CompiledShaderArtifact& right, const char* what) {
    const std::string prefix = std::string(what) + ": ";
    require(left.spirv.Words() == right.spirv.Words(), prefix + "SPIR-V differs");
    require(left.bdaAbiVersion == right.bdaAbiVersion, prefix + "BDA ABI version differs");
    require(left.runtimeAbiVersion == right.runtimeAbiVersion, prefix + "runtime ABI version differs");
    require(left.memoryOffsetDword == right.memoryOffsetDword, prefix + "memory offset differs");
    require(left.shaderDataDwords == right.shaderDataDwords && left.imageMetadataDword == right.imageMetadataDword && left.runtimeImageCount == right.runtimeImageCount && left.runtimeImageResources == right.runtimeImageResources, prefix + "compact data layout differs");
    require(left.hostSubgroupSize == right.hostSubgroupSize, prefix + "host subgroup size differs");
    require(left.vertexInputs == right.vertexInputs, prefix + "vertex inputs differ");
    require(left.vertexInputPatches == right.vertexInputPatches, prefix + "vertex type patches differ");
    require(left.vertexOffsetSgpr == right.vertexOffsetSgpr && left.instanceOffsetSgpr == right.instanceOffsetSgpr, prefix + "offset SGPRs differ");
    require(left.vertexOffsetShared == right.vertexOffsetShared && left.instanceOffsetShared == right.instanceOffsetShared && left.vertexOffsetConflict == right.vertexOffsetConflict && left.instanceOffsetConflict == right.instanceOffsetConflict, prefix + "offset flags differ");
    require(left.parameterExports == right.parameterExports, prefix + "parameter exports differ");
    require(left.fragmentParameters.size() == right.fragmentParameters.size(), prefix + "fragment parameter count differs");
    for (std::size_t i = 0; i < left.fragmentParameters.size(); ++i) {
        const auto& a = left.fragmentParameters[i];
        const auto& b = right.fragmentParameters[i];
        require(a.location == b.location && a.sourceLocation == b.sourceLocation && a.flat == b.flat && a.perVertex == b.perVertex && a.custom == b.custom, prefix + "fragment parameter differs");
    }
}

void requireSameResult(const RecompileResult& left, const RecompileResult& right, const char* what) {
    requireSameArtifact(left, right, what);
    const std::string prefix = std::string(what) + ": ";
    require(sameBindings(left.bindings, right.bindings), prefix + "bindings differ");
    require(left.pushConstants == right.pushConstants, prefix + "push constants differ");
    require(left.specialization == right.specialization, prefix + "specialization constants differ");
    require(left.poisonedSrtReads == right.poisonedSrtReads, prefix + "poisoned SRT read counts differ");
    require(left.vertexAttributes.size() == right.vertexAttributes.size(), prefix + "vertex attribute count differs");
    for (std::size_t i = 0; i < left.vertexAttributes.size(); ++i) {
        const auto& a = left.vertexAttributes[i];
        const auto& b = right.vertexAttributes[i];
        require(a.location == b.location && a.components == b.components && a.resource.fields == b.resource.fields && a.fetchIndex == b.fetchIndex && a.formatComponents == b.formatComponents, prefix + "vertex attribute differs");
    }
}

void requireSameVariant(const CompiledVariant& left, const CompiledVariant& right, const char* what) {
    requireSameArtifact(left.artifact, right.artifact, what);
    const std::string prefix = std::string(what) + ": ";
    const auto& a = left.info;
    const auto& b = right.info;
    require(a.stage == b.stage && a.shaderHash == b.shaderHash && a.waveSize == b.waveSize && a.userDataBase == b.userDataBase && a.userDataCount == b.userDataCount && a.scratchDwords == b.scratchDwords && a.paramExportMask == b.paramExportMask, prefix + "compiled info differs");
    require(a.info == b.info, prefix + "shader info differs");
    require(a.bindings == b.bindings, prefix + "info binding layout differs");
    require(left.bindings.layout == right.bindings.layout, prefix + "allocation layout differs");
    require(left.bindings.pushConstantOffsetBytes == right.bindings.pushConstantOffsetBytes && left.bindings.pushConstantSizeBytes == right.bindings.pushConstantSizeBytes, prefix + "push constant range differs");
}

DescriptorBinding sampleBinding(std::uint32_t seed) {
    DescriptorBinding binding{};
    binding.kind = DescriptorKind::StorageImage;
    binding.role = DescriptorRole::GuestImages;
    binding.descriptorSet = 0;
    binding.binding = 29 + seed;
    binding.count = 3;
    binding.guestDescriptor = {0x11111111u * seed, 0xdeadbeefu, 0x80000000u, 7u, 0u, 0xffffffffu, 42u, seed};
    binding.readOnly = seed % 2 == 0;
    binding.imageShape = DescriptorImageShape::Image2DArray;
    binding.samplerDepthCompare = {true, false, true};
    binding.imageDepthCompare = {false, true, false};
    binding.imageWritten = {false, true, true};
    binding.imageAtomic = {false, true, false};
    binding.bufferAtomic = {true};
    binding.bufferWritten = {false, false, true, true, false};
    binding.samplerUnnormalized = {false, true, true};
    binding.imageUnnormalized = {true, false, seed % 2 == 0};
    binding.imageSamplers = {0x5u, 0u, 0x80000000u};
    return binding;
}

RecompileResult sampleResult() {
    RecompileResult result;
    std::vector<std::uint32_t> words(1000);
    for (std::size_t i = 0; i < words.size(); ++i) words[i] = static_cast<std::uint32_t>(i * 2654435761u);
    words[0] = 0x07230203u;
    result.spirv = std::move(words);
    result.bindings = {sampleBinding(1), sampleBinding(2)};
    result.bindings[1].kind = DescriptorKind::StorageBuffer;
    result.bindings[1].role = DescriptorRole::GuestBuffers;
    result.bindings[1].imageShape.reset();
    result.pushConstants = {std::byte{1}, std::byte{0xff}, std::byte{0}, std::byte{0x80}, std::byte{7}};
    result.bdaAbiVersion = 3;
    result.memoryOffsetDword = 7;
    result.hostSubgroupSize = 32;
    result.vertexInputs = {{1, 4, 2}, {5, 2, 0}};
    result.vertexInputPatches = {{1, 20, {7, 8, 9}}, {5, 40, {10, 11, 12}}};
    result.specialization = {{512, 4}, {516, 0x3f800000u}, {517, 0}};
    result.vertexAttributes = {{1, 4, {{0x1000u, 0x20000u, 0x30u, 0x4u}}, 2}, {5, 2, {{9u, 8u, 7u, 6u}}, 0}};
    result.vertexInputs[0].outputMask = 5u;
    result.vertexAttributes[0].formatComponents = 3u;
    result.vertexOffsetSgpr = 12;
    result.instanceOffsetSgpr = -1;
    result.vertexOffsetShared = true;
    result.instanceOffsetShared = false;
    result.vertexOffsetConflict = false;
    result.instanceOffsetConflict = true;
    result.parameterExports = {0, 3, 7};
    result.fragmentParameters = {{0, 1, true, false, true}, {2, 3, false, true}};
    result.poisonedSrtReads = 3;
    result.variantId = 99;
    return result;
}

CompiledVariant sampleVariant() {
    CompiledVariant variant;
    variant.artifact = sampleResult();
    variant.layout = {0, 0, 0, 128};
    auto& info = variant.info;
    info.stage = IrShaderStage::Compute;
    info.shaderHash = 0x123456789abcdefull;
    info.waveSize = 32;
    info.userDataBase = 4;
    info.userDataCount = 16;
    info.scratchDwords = 8;
    info.paramExportMask = 0x5;
    info.info.scratchDwords = 8;
    info.info.sharedMemoryBytes = 4096;
    BufferResource buffer{};
    buffer.source = 3;
    buffer.firstUsePc = 0x40;
    buffer.maxByteExtent = 256;
    buffer.imageAlias = 1;
    buffer.read = true;
    buffer.atomic = true;
    buffer.scalar = true;
    buffer.descriptorFormatted = true;
    buffer.formattedReadMask = 5u;
    buffer.typedAlignment = 2u;
    info.info.buffers = {buffer, BufferResource{}};
    ImageResource image{};
    image.source = 5;
    image.firstUsePc = 0x80;
    image.resourceClass = ImageResourceClass::Storage;
    image.numericClass = IrTextureNumericClass::Uint;
    image.mipMode = ImageMipMode::DynamicStorage;
    image.mipCount = 4;
    image.shaderSwizzle = 0x321u;
    image.written = true;
    image.srgbDecode = true;
    image.cube = true;
    image.r128 = true;
    image.fmaskCompatible = false;
    image.depthBitsCompatible = false;
    image.byElements = 4;
    image.byComponents = 1;
    image.indirectRoot = 0;
    image.indirectMappingOffset = 12;
    image.indirectSearchIterations = 3;
    image.indirectResources = {1, 2, 3};
    info.info.images = {image};
    ResourceMaterializer::PrepareImageModes(info.info);
    info.info.samplers = {{7, 0x10, 3, true, false, SamplerUseExplicitLod | SamplerUseGather}};
    info.info.sampledPairs = {{0, 0, 0x10}};
    StageInput input{};
    input.kind = StageInputKind::GlobalInvocationId;
    input.location = 2;
    input.componentCount = 3;
    input.debugName = "gid";
    input.perVertex = true;
    info.info.inputs = {input};
    StageOutput output{};
    output.kind = StageOutputKind::Mrt;
    output.index = 1;
    output.location = 4;
    output.debugName = "mrt1";
    info.info.outputs = {output};
    info.info.vertexFetchComponents[3] = 4;
    info.info.vertexOffsetSgpr = 6;
    info.info.hasBitwiseXor = true;
    info.info.usesDma = true;
    info.info.usesFaultBuffer = true;
    info.bindings.pushDataStartDword = 2;
    info.bindings.memoryOffsetDword = 1;
    info.bindings.memoryOffsetCount = 5;
    info.bindings.userDataRegisters = {4, 5, 9};
    info.bindings.descriptors = {{DescriptorBindingKind::Buffers, {0, 1}}, {DescriptorBindingKind::FlattenedSrt, {}}};
    variant.bindings.layout = info.bindings;
    variant.bindings.pushConstantOffsetBytes = 16;
    variant.bindings.pushConstantSizeBytes = 112;
    return variant;
}

struct SampleRequest {
    std::vector<std::uint32_t> code{0xe0700000u, 0x80000000u, 0xbf810000u, 0x12345678u};
    std::vector<std::uint32_t> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    std::array<std::uint32_t, 2> capabilities{1u, 61u};
    std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    std::array<std::byte, 16> header{};
    RecompileRequest request{};
    std::uint32_t hostSubgroupSize = 32;

    SampleRequest() {
        request.shader = {ShaderStage::Compute, 0x20000u, code, 0x1f000u, header};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00403000u;
        request.target.spirvVersion = 0x00010600u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = 1;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
        request.target.maxWorkgroupInvocations = 1024;
        request.target.maxWorkgroupSharedMemoryBytes = 49152;
        request.layout = {0, 0, 0, 128};
    }

    std::vector<std::byte> Key() {
        request.shader.code = code;
        request.context.userData = userData;
        request.target.supportedCapabilities = capabilities;
        std::vector<std::byte> key;
        ShaderDiskCache::BuildKey(request, hostSubgroupSize, key);
        return key;
    }
};

void verifyResultRoundTrip() {
    const auto result = sampleResult();
    std::vector<std::byte> bytes;
    ShaderDiskCache::EncodeResult(result, bytes);
    RecompileResult decoded;
    require(ShaderDiskCache::DecodeResult(bytes, decoded), "an encoded result does not decode");
    requireSameResult(result, decoded, "result round trip");
    require(decoded.variantId == 0 && !decoded.cacheHit, "the per-process fields were stored");
    for (std::size_t size = 0; size < bytes.size(); size += size < 256 ? 1 : 97) {
        RecompileResult partial;
        require(!ShaderDiskCache::DecodeResult(std::span(bytes).first(size), partial), "a result truncated to " + std::to_string(size) + " bytes decodes");
    }
    auto longer = bytes;
    longer.push_back(std::byte{0});
    require(!ShaderDiskCache::DecodeResult(longer, decoded), "a result with a trailing byte decodes");
    bytes.clear();
    ShaderDiskCache::EncodeResult(RecompileResult{}, bytes);
    require(ShaderDiskCache::DecodeResult(bytes, decoded), "an empty result does not decode");
    requireSameResult(RecompileResult{}, decoded, "empty result round trip");
    auto incompatible = result;
    incompatible.runtimeAbiVersion = RuntimeAbi::Version + 1u;
    bytes.clear();
    ShaderDiskCache::EncodeResult(incompatible, bytes);
    require(!ShaderDiskCache::DecodeResult(bytes, decoded), "an incompatible runtime ABI result decodes");
}

void verifyEntryRoundTrip() {
    SampleRequest sample;
    const auto key = sample.Key();
    const auto variant = sampleVariant();
    const auto file = ShaderDiskCache::EncodeEntry(key, variant);
    CompiledVariant decoded;
    require(ShaderDiskCache::DecodeEntry(file, key, decoded) == ShaderDiskCache::LoadStatus::Loaded, "an encoded entry does not decode");
    requireSameVariant(variant, decoded, "entry round trip");
    require(decoded.artifact.variantId == 0, "the artifact stored a per-process id");

    for (std::size_t size = 0; size < file.size(); size += size < 512 ? 1 : 131) {
        CompiledVariant partial;
        require(ShaderDiskCache::DecodeEntry(std::span(file).first(size), key, partial) == ShaderDiskCache::LoadStatus::Rejected, "an entry truncated to " + std::to_string(size) + " bytes is not rejected");
    }
    for (std::size_t offset = 0; offset < file.size(); offset += offset < 512 ? 1 : 61) {
        auto damaged = file;
        damaged[offset] ^= std::byte{0x10};
        CompiledVariant partial;
        const auto status = ShaderDiskCache::DecodeEntry(damaged, key, partial);
        require(status == ShaderDiskCache::LoadStatus::Rejected, "an entry with byte " + std::to_string(offset) + " damaged is not rejected");
    }
    auto longer = file;
    longer.push_back(std::byte{0});
    require(ShaderDiskCache::DecodeEntry(longer, key, decoded) == ShaderDiskCache::LoadStatus::Rejected, "an entry with a trailing byte is not rejected");
    auto otherKey = key;
    otherKey.back() ^= std::byte{1};
    require(ShaderDiskCache::DecodeEntry(file, otherKey, decoded) == ShaderDiskCache::LoadStatus::KeyMismatch, "an entry for another key loads");
    auto previousVersion = file;
    previousVersion[4] = static_cast<std::byte>(ShaderDiskCache::FormatVersion - 1u);
    require(ShaderDiskCache::DecodeEntry(previousVersion, key, decoded) == ShaderDiskCache::LoadStatus::Rejected, "an entry with the previous format version loads");
    auto incompatible = variant;
    incompatible.artifact.runtimeAbiVersion = RuntimeAbi::Version + 1u;
    const auto incompatibleFile = ShaderDiskCache::EncodeEntry(key, incompatible);
    require(ShaderDiskCache::DecodeEntry(incompatibleFile, key, decoded) == ShaderDiskCache::LoadStatus::Rejected, "an entry with an incompatible runtime ABI loads");
}

void verifyArtifactStorageIsolation() {
    SampleRequest sample;
    const auto key = sample.Key();
    auto result = sampleResult();
    auto variant = sampleVariant();
    variant.artifact = result;
    const auto before = ShaderDiskCache::EncodeEntry(key, variant);
    result.bindings[0].guestDescriptor[0] ^= 0x10000u;
    result.pushConstants[0] ^= std::byte{0xff};
    result.vertexAttributes[0].resource.fields[0] ^= 0x10000u;
    variant.artifact = result;
    require(ShaderDiskCache::EncodeEntry(key, variant) == before, "invocation data changed the stored artifact");
    result.vertexInputs[0].components = 2;
    variant.artifact = result;
    require(ShaderDiskCache::EncodeEntry(key, variant) != before, "the stored artifact ignores vertex input metadata");
}

void verifyKeySensitivity() {
    SampleRequest base;
    const auto key = base.Key();
    require(base.Key() == key, "the key is not deterministic");
    require(ShaderDiskCache::EntryName(key).size() == 36, "unexpected entry name length");

    const auto changes = [&](const std::string& what, const std::function<void(SampleRequest&)>& change) {
        SampleRequest sample;
        change(sample);
        const auto changed = sample.Key();
        require(changed != key, "the key ignores " + what);
        require(ShaderDiskCache::EntryName(changed) != ShaderDiskCache::EntryName(key), "the entry name ignores " + what);
    };
    for (std::size_t word = 0; word < base.code.size(); ++word) {
        for (std::uint32_t bit = 0; bit < 32; ++bit) {
            changes("code word " + std::to_string(word) + " bit " + std::to_string(bit), [&](SampleRequest& sample) { sample.code[word] ^= 1u << bit; });
        }
    }
    changes("a code word appended", [](SampleRequest& sample) { sample.code.push_back(0); });
    changes("the stage", [](SampleRequest& sample) { sample.request.shader.stage = ShaderStage::Fragment; sample.request.context.compute.reset(); });
    changes("the wave size", [](SampleRequest& sample) { sample.request.context.waveSize = 32; });
    changes("the user data base", [](SampleRequest& sample) { sample.request.context.userDataBaseRegister = 2; });
    changes("the user data count", [](SampleRequest& sample) { sample.userData.push_back(0); });
    for (std::size_t axis = 0; axis < 3; ++axis) {
        changes("thread count " + std::to_string(axis), [&](SampleRequest& sample) { sample.request.context.compute->numThreads[axis] += 1; });
        changes("group id enable " + std::to_string(axis), [&](SampleRequest& sample) { sample.request.context.compute->groupIdEnable[axis] = true; });
    }
    changes("the LDS size", [](SampleRequest& sample) { sample.request.context.compute->ldsSizeDwords = 64; });
    changes("the thread group size enable", [](SampleRequest& sample) { sample.request.context.compute->tgSizeEnable = true; });
    changes("the thread id component count", [](SampleRequest& sample) { sample.request.context.compute->threadIdComponentCount = 3; });
    changes("the Vulkan version", [](SampleRequest& sample) { sample.request.target.vulkanVersion = 0x00402000u; });
    changes("the SPIR-V version", [](SampleRequest& sample) { sample.request.target.spirvVersion = 0x00010500u; });
    changes("the target subgroup size", [](SampleRequest& sample) { sample.request.target.subgroupSize = 64; });
    changes("the BDA ABI version", [](SampleRequest& sample) { sample.request.target.bdaAbiVersion = 2; });
    changes("a capability", [](SampleRequest& sample) { sample.capabilities[1] = 62u; });
    changes("an extension", [](SampleRequest& sample) { sample.extensions[0] = "SPV_KHR_storage_buffer_storage_clasS"; });
    changes("barycentrics", [](SampleRequest& sample) { sample.request.target.fragmentShaderBarycentricEnabled = true; });
    changes("non-constant texel offsets", [](SampleRequest& sample) { sample.request.target.nonConstantImageOffsets = true; });
    changes("the sRGB formats decoded in the shader", [](SampleRequest& sample) { sample.request.target.srgbDecodeFormats = 2u; });
    changes("the narrow subgroup clock", [](SampleRequest& sample) { sample.request.target.narrowSubgroupClock = true; });
    changes("the workgroup size limit", [](SampleRequest& sample) { sample.request.target.maxWorkgroupSize[2] = 128; });
    changes("the invocation limit", [](SampleRequest& sample) { sample.request.target.maxWorkgroupInvocations = 512; });
    changes("the shared memory limit", [](SampleRequest& sample) { sample.request.target.maxWorkgroupSharedMemoryBytes = 32768; });
    changes("the host subgroup size", [](SampleRequest& sample) { sample.hostSubgroupSize = 64; });
    changes("the descriptor set", [](SampleRequest& sample) { sample.request.layout.descriptorSet = 1; });
    changes("the first binding", [](SampleRequest& sample) { sample.request.layout.firstBinding = 1; });
    changes("the push constant offset", [](SampleRequest& sample) { sample.request.layout.pushConstantOffsetBytes = 16; });
    changes("the push constant size", [](SampleRequest& sample) { sample.request.layout.pushConstantSizeBytes = 64; });
    changes("a float mode", [](SampleRequest& sample) { sample.request.context.floatMode = ShaderFloatMode{}; });
    const ShaderFloatMode astroMode{0xc0u, true, false, false};
    const auto withMode = [&](const ShaderFloatMode& mode) {
        SampleRequest sample;
        sample.request.context.floatMode = mode;
        return std::pair{sample.Key(), RecompileCacheKey::ContextHash(sample.request)};
    };
    const auto astro = withMode(astroMode);
    require(withMode(astroMode) == astro, "the float mode key is not deterministic");
    for (const auto& [what, mode] : std::initializer_list<std::pair<const char*, ShaderFloatMode>>{
             {"FLOAT_MODE", {0x00u, true, false, false}}, {"DX10_CLAMP", {0xc0u, false, false, false}},
             {"IEEE_MODE", {0xc0u, true, true, false}}, {"FP16_OVFL", {0xc0u, true, false, true}}}) {
        const auto changed = withMode(mode);
        require(changed.first != astro.first && changed.second != astro.second, std::string("the key ignores ") + what);
    }

    SampleRequest moved;
    moved.userData[0] ^= 0x10000u;
    moved.request.shader.codeAddress += 0x1000000u;
    moved.request.shader.headerAddress += 0x1000000u;
    moved.header[3] = std::byte{1};
    require(moved.Key() == key, "the key depends on the user data values or the addresses");
    std::array<std::byte, 32> descriptors{};
    const std::array regions{MemoryRegion{0x100000u, descriptors}};
    moved.request.context.memory = regions;
    std::fill(moved.userData.begin(), moved.userData.end(), 0xffffffffu);
    require(moved.Key() == key, "runtime descriptors changed the artifact key");
    SampleRequest vertex;
    vertex.request.shader.stage = ShaderStage::Vertex;
    vertex.request.context.compute.reset();
    vertex.request.context.vertex.emplace();
    auto& input = *vertex.request.context.vertex;
    input.resourcesNum = 1u;
    input.resources[0].fields = {0x10000u, 16u << 16u, 64u, (22u << 12u) | 0xfacu};
    input.resourcesDst[0] = {0u, 4u, 0u, 0u};
    const auto vertexKey = vertex.Key();
    const auto contextKey = RecompileCacheKey::ContextHash(vertex.request);
    input.resources[0].fields = {0x20000u, 32u << 16u, 128u, (77u << 12u) | 0xfa9u};
    require(vertex.Key() == vertexKey && RecompileCacheKey::ContextHash(vertex.request) == contextKey, "runtime vertex format, stride, swizzle or address changed the static interface key");
    input.resources[0].fields[3] = (20u << 12u) | 0xfacu;
    require(vertex.Key() != vertexKey && RecompileCacheKey::ContextHash(vertex.request) != contextKey, "integer vertex input reused a float interface");
    input.resources[0].fields[3] = (77u << 12u) | 0xfa9u;
    input.resourcesDst[0].registersNum = 2u;
    require(vertex.Key() != vertexKey, "vertex component interface did not change the artifact key");
    input.fetchEmbedded = true;
    const auto embeddedKey = vertex.Key();
    input.resources[0].fields = {};
    input.resourcesDst[0].fetchIndex = 1u;
    require(vertex.Key() == embeddedKey, "runtime vertex fetch descriptors changed the artifact key");
    input.resources[0].fields[3] = (20u << 12u) | 0xfacu;
    require(vertex.Key() == embeddedKey, "embedded numeric class changed the prepared template key");
    input.resourcesNum = 0u;
    require(vertex.Key() != embeddedKey, "embedded semantic interface did not change the artifact key");
    SampleRequest fragment;
    fragment.request.shader.stage = ShaderStage::Fragment;
    fragment.request.context.compute.reset();
    fragment.request.context.pixel.emplace();
    fragment.request.context.pixel->targetOutputMode[0] = 9;
    const auto fragmentKey = fragment.Key();
    const auto fragmentContextKey = RecompileCacheKey::ContextHash(fragment.request);
    fragment.request.context.pixel->targetExportMapping.fill(0x1bu);
    require(fragment.Key() == fragmentKey && RecompileCacheKey::ContextHash(fragment.request) == fragmentContextKey, "runtime export mapping changed the fragment artifact key");
    fragment.request.context.pixel->targetOutputMode[0] = 7;
    require(fragment.Key() != fragmentKey, "integer fragment output reused a float interface");
}

void verifyFloatModeSerialization() {
    const RequestSerializer serializer;
    SampleRequest sample;
    require(!serializer.Deserialize(serializer.Serialize(sample.request)).request.context.floatMode.has_value(), "an unknown float mode came back from serialization");
    const ShaderFloatMode mode{0x04u, false, true, true};
    sample.request.context.floatMode = mode;
    const auto back = serializer.Deserialize(serializer.Serialize(sample.request));
    require(back.request.context.floatMode == mode, "the float mode did not survive serialization");
}

void verifyStore() {
    require(ShaderDiskCache::Enabled(), "the disk cache is not enabled");
    SampleRequest sample;
    const auto key = sample.Key();
    const auto variant = std::make_shared<const CompiledVariant>(sampleVariant());
    const auto before = ShaderDiskCache::Totals();
    CompiledVariant loaded;
    require(!ShaderDiskCache::Load(key, loaded), "an entry loads before it was stored");
    ShaderDiskCache::Store(key, variant);
    ShaderDiskCache::Flush();
    require(ShaderDiskCache::Totals().writes == before.writes + 1, "the store did not write the entry");
    const auto path = ShaderDiskCache::EntryDirectory() / ShaderDiskCache::EntryName(key);
    require(std::filesystem::exists(path), "no entry file at " + path.string());
    require(ShaderDiskCache::Load(key, loaded), "a stored entry does not load");
    requireSameVariant(*variant, loaded, "store round trip");
    require(ShaderDiskCache::Totals().hits == before.hits + 1, "the load was not counted as a hit");

    std::vector<std::byte> file;
    require(ReadWholeFile(path, file), "cannot read the entry back");
    require(WriteFileAtomically(path, std::span(file).first(file.size() / 2)), "cannot truncate the entry");
    require(!ShaderDiskCache::Load(key, loaded), "a truncated entry loads");
    auto corrupt = file;
    corrupt[corrupt.size() - 5] ^= std::byte{0x40};
    require(WriteFileAtomically(path, corrupt), "cannot damage the entry");
    require(!ShaderDiskCache::Load(key, loaded), "a corrupt entry loads");
    require(ShaderDiskCache::Totals().loadFailures == before.loadFailures + 2, "the rejected entries were not counted");
    ShaderDiskCache::Store(key, variant);
    ShaderDiskCache::Flush();
    require(ShaderDiskCache::Load(key, loaded), "a rewritten entry does not load");
}

struct ComputeRequest {
    std::vector<std::uint32_t> code{0xe0700000u, 0x80000000u, 0xbf810000u};
    std::array<std::uint32_t, 4> userData{0x10000000u, 0x00000000u, 0x40u, 0x00027facu};
    std::array<std::uint32_t, 4> capabilities{1u, 11u, 5347u, 4448u};
    std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};

    explicit ComputeRequest(bool useCache) {
        request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.bdaAbiVersion = 1u;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = useCache;
    }
};

int runLoadingProcess() {
    ComputeRequest cached(true);
    const auto loaded = Recompile(cached.request);
    const auto totals = ShaderDiskCache::Totals();
    require(totals.hits == 1 && totals.misses == 0 && totals.loadFailures == 0, "the second process did not load the stored variant (hits " + std::to_string(totals.hits) + ", misses " + std::to_string(totals.misses) + ")");
    ComputeRequest fresh(false);
    const auto compiled = Recompile(fresh.request);
    requireSameResult(compiled, loaded, "loaded against compiled");
    require(!loaded.spirv.empty(), "the loaded SPIR-V is empty");
    std::cout << "second process loaded " << loaded.spirv.size() << " SPIR-V words from the disk cache\n";
    return 0;
}

void verifyAcrossProcesses(const char* self) {
    ComputeRequest request(true);
    const auto before = ShaderDiskCache::Totals();
    const auto compiled = Recompile(request.request);
    ShaderDiskCache::Flush();
    const auto after = ShaderDiskCache::Totals();
    require(after.misses == before.misses + 1 && after.writes == before.writes + 1, "the first compile was not looked up and stored");
    require(!compiled.spirv.empty(), "the compile produced no SPIR-V");
    const std::string command = "\"" + std::string(self) + "\" --load";
    require(std::system(command.c_str()) == 0, "the loading process failed");
}

struct ClockRequest {
    std::vector<std::uint32_t> code{0xf4900100u, 0x00000000u, 0xbf8cc07fu, 0x7e020204u, 0xe0700000u, 0x80000100u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    std::array<std::uint32_t, 4> userData{0x10000000u, 0x00000000u, 0x40u, 0x00027facu};
    std::array<std::uint32_t, 4> capabilities{1u, 11u, 5347u, 4448u};
    std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    RecompileRequest request{};

    explicit ClockRequest(std::uint32_t stride = 0) {
        userData[1] = stride << 16u;
        ShaderPixelStageInfo pixel{};
        pixel.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
        pixel.hasPerspectiveCenterVgpr = true;
        pixel.targetOutputMode[0] = 9u;
        pixel.targetExportMapping.fill(0xe4u);
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.pixel = pixel;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.bdaAbiVersion = 1u;
        request.layout.pushConstantSizeBytes = 128;
    }
};

std::string emissionFailure(const RecompileRequest& request) {
    try {
        static_cast<void>(PrepareShader(request));
    } catch (const std::runtime_error& error) {
        const std::string message = error.what();
        require(message.find("reads the shader clock, which needs the device's VK_KHR_shader_clock") != std::string::npos, "unexpected failure: " + message.substr(0, message.find("RecompileRequest:")));
        return message;
    }
    throw std::runtime_error("a shader clock read compiled without VK_KHR_shader_clock");
}

std::uint64_t diskMisses() {
    return ShaderDiskCache::Totals().misses;
}

void verifyEmissionFailureMemo() {
    const auto before = ShaderDiskCache::Totals();
    ClockRequest clock;
    const auto first = emissionFailure(clock.request);
    require(diskMisses() == before.misses + 1, "the first compile was not looked up");
    require(emissionFailure(clock.request) == first, "the second compile failed with another message");
    require(diskMisses() == before.misses + 1, "the second compile looked up and emitted the program again");

    ClockRequest layout;
    layout.request.layout = {0, 0, 16, 112};
    static_cast<void>(emissionFailure(layout.request));
    require(diskMisses() == before.misses + 2, "another binding layout reused the failure");
    ClockRequest stride(16);
    static_cast<void>(emissionFailure(stride.request));
    require(diskMisses() == before.misses + 2, "a runtime buffer stride change repeated static preparation");
    ClockRequest relocated;
    relocated.request.shader.codeAddress += 0x1000u;
    static_cast<void>(emissionFailure(relocated.request));
    require(diskMisses() == before.misses + 3, "a relocated copy reused the failure");
    require(emissionFailure(clock.request) == first && diskMisses() == before.misses + 3, "the other requests replaced the failure");

    ClockRequest concurrent;
    concurrent.request.layout.pushConstantSizeBytes = 64;
    std::array<std::future<std::string>, 4> failures;
    for (auto& future : failures) future = std::async(std::launch::async, [&concurrent] { return emissionFailure(concurrent.request); });
    std::vector<std::string> messages;
    for (auto& future : failures) messages.push_back(future.get());
    require(std::all_of(messages.begin(), messages.end(), [&](const std::string& message) { return message == messages.front(); }), "concurrent requests failed with different messages");
    require(diskMisses() == before.misses + 4, "concurrent requests emitted the program " + std::to_string(diskMisses() - before.misses - 3) + " times");

    ShaderDiskCache::Flush();
    require(ShaderDiskCache::Totals().writes == before.writes, "an emission failure was stored in the disk cache");
}

int runWithoutFailureMemo() {
    ClockRequest clock;
    const auto first = emissionFailure(clock.request);
    require(emissionFailure(clock.request) == first, "the second compile failed with another message");
    require(diskMisses() == 2, "APS5_NO_FAILURE_MEMO=1 did not emit the program again (" + std::to_string(diskMisses()) + " lookups)");
    std::cout << "APS5_NO_FAILURE_MEMO=1 emitted the failing program twice\n";
    return 0;
}

void verifyFailureMemoSwitch(const char* self) {
    setEnvironment("APS5_NO_FAILURE_MEMO", "1");
    const std::string command = "\"" + std::string(self) + "\" --no-failure-memo";
    require(std::system(command.c_str()) == 0, "the process with APS5_NO_FAILURE_MEMO=1 failed");
}

void verifyInvocationIsolation() {
    ComputeRequest request(true);
    const auto first = Recompile(request.request);
    request.userData[0] += 0x10000u;
    const auto second = Recompile(request.request);
    require(first.variantId != 0 && first.variantId == second.variantId, "a buffer address change did not reuse the artifact");
    require(second.cacheHit, "the second invocation missed the compiled cache");
    requireSameArtifact(first, second, "invocations sharing an artifact");
    require(first.spirv.data() == second.spirv.data(), "invocations duplicated SPIR-V storage");
    require(!sameBindings(first.bindings, second.bindings) || first.pushConstants != second.pushConstants, "the second invocation reused stale resource data");
    request.userData[0] -= 0x10000u;
    const auto restored = Recompile(request.request);
    requireSameResult(first, restored, "restored invocation");
    const auto original = request.userData;
    const std::array<std::array<std::uint32_t, 4>, 10> descriptors{{
        {original[0] + 0x20000u, original[1], original[2], original[3]},
        {original[0] + 1u, original[1], original[2], original[3]},
        {original[0] + 2u, original[1], original[2], original[3]},
        {original[0] + 3u, original[1], original[2], original[3]},
        {original[0], original[1] | 1u, original[2], original[3]},
        {original[0], original[1], original[2] / 2u, original[3]},
        {original[0], original[1] | (16u << 16u), original[2], original[3]},
        {original[0], original[1], original[2], 0x31004688u},
        {original[0], original[1] | 0x80100000u, original[2], original[3] | 0x00600000u},
        {0u, 0u, 0u, 0u}
    }};
    for (const auto& descriptor : descriptors) {
        request.userData = descriptor;
        const auto changed = Recompile(request.request);
        require(changed.cacheHit && first.variantId == changed.variantId, "buffer metadata changed the compiled variant");
        if ((descriptor[0] & 3u) == (original[0] & 3u) && (descriptor[1] & 0xffff0000u) == (original[1] & 0xffff0000u) && descriptor[3] == original[3] && (descriptor[0] != 0u || (descriptor[1] & 0xffffu) != 0u)) {
            require(first.spirv.data() == changed.spirv.data() && first.PipelineVariantId() == changed.PipelineVariantId(), "buffer address or size rebuilt the specialized module");
        }
        ComputeRequest fresh(false);
        fresh.userData = descriptor;
        requireSameResult(Recompile(fresh.request), changed, "reused binding plan against fresh compilation");
        const auto repeated = Recompile(request.request);
        require(changed.spirv.data() == repeated.spirv.data() && changed.PipelineVariantId() == repeated.PipelineVariantId(), "buffer specialization did not reuse its module");
    }
    request.userData = original;
    request.userData[3] |= 0x40000000u;
    bool rejected = false;
    try { static_cast<void>(Recompile(request.request)); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "an unsupported buffer descriptor type was accepted");
}

void verifyBufferAlignmentSpecialization() {
    ComputeRequest request(true);
    request.request.context.waveSize = 32u;
    request.request.context.compute->numThreads = {32u, 1u, 1u};
    request.userData[3] = 0x31027facu;
    static_cast<void>(PrepareShader(request.request));
    const auto misses = diskMisses();
    const auto aligned = Recompile(request.request);
    const auto countAtomics = [](const RecompileResult& result) {
        std::size_t atomics = 0;
        for (std::size_t cursor = 5; cursor < result.spirv.size();) {
            const auto count = result.spirv[cursor] >> 16u;
            require(count != 0u && count <= result.spirv.size() - cursor, "invalid specialized buffer instruction");
            if ((result.spirv[cursor] & 0xffffu) == spv::OpAtomicCompareExchange) ++atomics;
            cursor += count;
        }
        return atomics;
    };
    require(countAtomics(aligned) == 0u, "aligned DWORD store retained unaligned atomic updates");
    for (std::uint32_t offset = 1; offset < 4u; ++offset) {
        request.userData[0] = 0x10000000u + offset;
        const auto unaligned = Recompile(request.request);
        require(unaligned.variantId == aligned.variantId && diskMisses() == misses, "base alignment repeated static shader preparation");
        require(countAtomics(unaligned) == 2u, "unaligned DWORD store did not split into two atomic updates");
        request.userData[0] += 0x10000u;
        const auto relocated = Recompile(request.request);
        require(unaligned.spirv.data() == relocated.spirv.data(), "relocation with unchanged alignment rebuilt the buffer specialization");
    }
    request.userData[0] = 0x10000000u;
    const auto restored = Recompile(request.request);
    require(restored.spirv.data() == aligned.spirv.data(), "unaligned buffers replaced the aligned specialization");
}

void verifyBindingPlanSelection() {
    ShaderInfo info{};
    info.buffers.resize(2);
    info.buffers[0].atomic = true;
    info.buffers[1].read = true;
    CompiledBindingLayout compiled{};
    compiled.layout.descriptors = {{DescriptorBindingKind::Buffers, {0, 1}}, {DescriptorBindingKind::FlattenedSrt, {}}, {DescriptorBindingKind::ShaderData, {}}, {DescriptorBindingKind::Gds, {}}};
    compiled.layout.userDataRegisters = {0};
    compiled.layout.memoryOffsetDword = 1;
    ResourceSnapshot snapshot{};
    snapshot.buffers.resize(2);
    for (auto& buffer : snapshot.buffers) {
        buffer.dwordCount = 4u;
        buffer.dwords = {0x10000000u, 0u, 64u, 0x31016facu};
    }
    snapshot.userData = {17u};
    snapshot.flattenedSrt = {23u};
    const DescriptorBindingBuilder builder;
    const auto plan = builder.Prepare(compiled.layout, info, IrShaderStage::Compute, snapshot);
    BindingAllocationResult full;
    builder.Populate(full, compiled, plan, 0u, snapshot, {});
    require(full.bindings.size() == 4u, "binding plan lost a descriptor before module selection");
    require(full.bindings[0].bufferAtomic == std::vector<bool>{true, false} && full.bindings[0].bufferWritten == std::vector<bool>{true, false}, "binding plan lost buffer access metadata");
    const std::array liveBindings{full.bindings[0].binding, full.bindings[0].binding, full.bindings[3].binding};
    const auto selected = builder.Select(plan, liveBindings);
    snapshot.userData.clear();
    snapshot.flattenedSrt.clear();
    snapshot.buffers[0].dwords[0] += 0x10000u;
    snapshot.buffers[1].dwords[2] = 128u;
    BindingAllocationResult populated;
    builder.Populate(populated, compiled, selected, 0u, snapshot, {});
    require(populated.bindings.size() == 2u && populated.pushConstants.empty(), "module selection retained an unused descriptor");
    auto expected = full.bindings[0];
    expected.guestDescriptor[0] += 0x10000u;
    expected.guestDescriptor[6] = 128u;
    require(sameBinding(expected, populated.bindings[0]) && sameBinding(full.bindings[3], populated.bindings[1]), "selected plan changed binding order or reused stale buffer data");
    bool rejected = false;
    try { static_cast<void>(builder.Select(plan, std::array{UINT32_MAX})); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "module selection accepted an unknown binding");
    snapshot.buffers[0].dwordCount = 3u;
    rejected = false;
    try { builder.Populate(populated, compiled, selected, 0u, snapshot, {}); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "selected plan accepted an invalid live buffer descriptor");
    snapshot.buffers[0].dwordCount = 4u;
    const auto rejectsAlignment = [&](const char* diagnostic) {
        try { static_cast<void>(builder.Prepare(compiled.layout, info, IrShaderStage::Compute, snapshot)); }
        catch (const std::runtime_error& error) {
            require(std::string_view(error.what()).find(diagnostic) != std::string_view::npos, "alignment check returned an unrelated failure");
            return;
        }
        throw std::runtime_error("binding plan accepted an unsupported buffer alignment");
    };
    snapshot.buffers[0].dwords[0] |= 2u;
    rejectsAlignment("buffer atomic on a V# whose base is not DWORD aligned");
    snapshot.buffers[0].dwords[0] &= ~3u;
    info.buffers[1].typedAlignment = 2u;
    snapshot.buffers[1].dwords[0] |= 2u;
    static_cast<void>(builder.Prepare(compiled.layout, info, IrShaderStage::Compute, snapshot));
    snapshot.buffers[1].dwords[0] |= 1u;
    rejectsAlignment("base is not aligned to its element");
    info.buffers[1].typedAlignment = 1u;
    info.buffers[1].descriptorFormatted = true;
    snapshot.buffers[1].dwords[3] = 0x3100bfacu;
    rejectsAlignment("base is not aligned to its element");
    snapshot.buffers[1].dwords[0] &= ~1u;
    static_cast<void>(builder.Prepare(compiled.layout, info, IrShaderStage::Compute, snapshot));
    snapshot.buffers[1].dwords[3] = 0x31014facu;
    rejectsAlignment("base is not aligned to its element");
}

void verifyVertexTypeSpecialization() {
    CompiledShaderArtifact artifact;
    artifact.vertexInputs = {{0, 4, 0, 1}, {1, 4, 0, 1}};
    artifact.spirv = std::vector<std::uint32_t>{
        spv::MagicNumber, 0x00010000u, 0, 28, 0,
        (2u << 16u) | spv::OpCapability, spv::CapabilityShader,
        (3u << 16u) | spv::OpMemoryModel, spv::AddressingModelLogical, spv::MemoryModelGLSL450,
        (7u << 16u) | spv::OpEntryPoint, spv::ExecutionModelVertex, 20, 0x6e69616du, 0, 10, 11,
        (4u << 16u) | spv::OpDecorate, 10, spv::DecorationLocation, 0,
        (4u << 16u) | spv::OpDecorate, 11, spv::DecorationLocation, 1,
        (2u << 16u) | spv::OpTypeVoid, 1,
        (3u << 16u) | spv::OpTypeFloat, 2, 32,
        (4u << 16u) | spv::OpTypeInt, 3, 32, 0,
        (4u << 16u) | spv::OpTypeVector, 4, 2, 4,
        (4u << 16u) | spv::OpTypePointer, 5, spv::StorageClassInput, 4,
        (4u << 16u) | spv::OpTypePointer, 6, spv::StorageClassInput, 2,
        (3u << 16u) | spv::OpTypeFunction, 7, 1,
        (4u << 16u) | spv::OpConstant, 3, 8, 0,
        (4u << 16u) | spv::OpVariable, 5, 10, spv::StorageClassInput,
        (4u << 16u) | spv::OpVariable, 5, 11, spv::StorageClassInput,
        (5u << 16u) | spv::OpFunction, 1, 20, spv::FunctionControlMaskNone, 7,
        (2u << 16u) | spv::OpLabel, 21,
        (5u << 16u) | spv::OpAccessChain, 6, 22, 10, 8,
        (4u << 16u) | spv::OpLoad, 2, 23, 22,
        (4u << 16u) | spv::OpBitcast, 3, 24, 23,
        (5u << 16u) | spv::OpAccessChain, 6, 25, 11, 8,
        (4u << 16u) | spv::OpLoad, 2, 26, 25,
        (4u << 16u) | spv::OpBitcast, 3, 27, 26,
        (1u << 16u) | spv::OpReturn,
        (1u << 16u) | spv::OpFunctionEnd
    };
    PrepareVertexInputSpecialization(artifact);
    const auto original = artifact.spirv.Words();
    for (std::uint32_t first = 0; first < 3u; ++first) {
        for (std::uint32_t second = 0; second < 3u; ++second) {
            const std::array classes{first, second};
            const auto specialized = SpecializeVertexInputTypes(artifact, classes);
            const auto& words = specialized.Words();
            std::map<std::uint32_t, std::uint32_t> scalarKinds;
            std::uint32_t checked = 0;
            for (std::size_t cursor = 5; cursor < words.size(); cursor += words[cursor] >> 16u) {
                const auto op = static_cast<spv::Op>(words[cursor] & 0xffffu);
                if (op == spv::OpTypeFloat) scalarKinds.emplace(words[cursor + 1u], 0u);
                if (op == spv::OpTypeInt) scalarKinds.emplace(words[cursor + 1u], words[cursor + 3u] != 0u ? 1u : 2u);
                if (op == spv::OpLoad) {
                    const auto attribute = words[cursor + 2u] == 23u ? 0u : 1u;
                    require(scalarKinds.at(words[cursor + 1u]) == classes[attribute], "vertex input loaded the wrong numeric type");
                    ++checked;
                }
                if (op == spv::OpBitcast || op == spv::OpCopyObject) {
                    const auto attribute = words[cursor + 2u] == 24u ? 0u : 1u;
                    require(op == (classes[attribute] == 2u ? spv::OpCopyObject : spv::OpBitcast), "vertex input did not preserve raw component bits");
                    ++checked;
                }
            }
            require(checked == 4u, "vertex specialization lost attribute loads");
            require(artifact.spirv.Words() == original, "vertex specialization mutated the shared template");
        }
    }
    auto invalid = artifact;
    invalid.vertexInputPatches.front().word = static_cast<std::uint32_t>(original.size());
    bool rejected = false;
    try { static_cast<void>(SpecializeVertexInputTypes(invalid, std::array{0u, 0u})); }
    catch (const std::runtime_error&) { rejected = true; }
    require(rejected, "out-of-range vertex type patch was accepted");
}

void verifyBuiltinSpecialization() {
    std::vector<std::uint32_t> words{spv::MagicNumber, 0x00010300u, 0u, 40u, 0u};
    const auto emit = [&](spv::Op op, std::initializer_list<std::uint32_t> operands) {
        words.push_back((static_cast<std::uint32_t>(operands.size() + 1u) << 16u) | op);
        words.insert(words.end(), operands);
    };
    emit(spv::OpCapability, {spv::CapabilityShader});
    emit(spv::OpMemoryModel, {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
    emit(spv::OpEntryPoint, {spv::ExecutionModelGLCompute, 10u, 0x6e69616du, 0u});
    emit(spv::OpExecutionMode, {10u, spv::ExecutionModeLocalSize, 1u, 1u, 1u});
    emit(spv::OpTypeVoid, {1u});
    emit(spv::OpTypeBool, {2u});
    emit(spv::OpTypeInt, {3u, 32u, 0u});
    emit(spv::OpTypeVector, {4u, 3u, 4u});
    emit(spv::OpTypeFunction, {5u, 1u});
    emit(spv::OpConstant, {3u, 6u, 0u});
    emit(spv::OpConstant, {3u, 7u, 1u});
    emit(spv::OpConstant, {3u, 8u, 2u});
    emit(spv::OpConstant, {3u, 9u, 3u});
    emit(spv::OpConstantComposite, {4u, 30u, 6u, 7u, 8u, 9u});
    emit(spv::OpTypePointer, {32u, spv::StorageClassPrivate, 4u});
    emit(spv::OpVariable, {32u, 31u, spv::StorageClassPrivate});
    emit(spv::OpTypePointer, {33u, spv::StorageClassPrivate, 3u});
    emit(spv::OpVariable, {33u, 34u, spv::StorageClassPrivate});
    emit(spv::OpFunction, {1u, 10u, spv::FunctionControlMaskNone, 5u});
    emit(spv::OpLabel, {11u});
    emit(spv::OpBitFieldUExtract, {3u, 12u, 9u, 7u, 7u});
    emit(spv::OpSelectionMerge, {16u, spv::SelectionControlMaskNone});
    emit(spv::OpSwitch, {12u, 15u, 1u, 14u});
    emit(spv::OpLabel, {14u});
    emit(spv::OpBranch, {16u});
    emit(spv::OpLabel, {15u});
    emit(spv::OpIAdd, {3u, 17u, 8u, 9u});
    emit(spv::OpBranch, {16u});
    emit(spv::OpLabel, {16u});
    emit(spv::OpPhi, {3u, 18u, 7u, 14u, 17u, 15u});
    emit(spv::OpStore, {34u, 18u});
    emit(spv::OpIEqual, {2u, 19u, 18u, 7u});
    emit(spv::OpSelectionMerge, {22u, spv::SelectionControlMaskNone});
    emit(spv::OpBranchConditional, {19u, 20u, 21u});
    emit(spv::OpLabel, {20u});
    emit(spv::OpVectorExtractDynamic, {3u, 23u, 30u, 6u});
    emit(spv::OpVectorExtractDynamic, {3u, 24u, 30u, 7u});
    emit(spv::OpVectorExtractDynamic, {3u, 25u, 30u, 8u});
    emit(spv::OpVectorExtractDynamic, {3u, 26u, 30u, 9u});
    emit(spv::OpCompositeConstruct, {4u, 27u, 23u, 24u, 25u, 26u});
    emit(spv::OpStore, {31u, 27u});
    emit(spv::OpBranch, {22u});
    emit(spv::OpLabel, {21u});
    emit(spv::OpUnreachable, {});
    emit(spv::OpLabel, {22u});
    emit(spv::OpReturn, {});
    emit(spv::OpFunctionEnd, {});
    const auto specialized = SpecializeSpirv(words);
    bool identity = false;
    std::uint32_t phiValue = 0u;
    for (std::size_t cursor = 5; cursor < specialized.size();) {
        const auto count = specialized[cursor] >> 16u;
        require(count != 0u && count <= specialized.size() - cursor, "builtin specialization produced a truncated instruction");
        const auto op = static_cast<spv::Op>(specialized[cursor] & 0xffffu);
        require(op != spv::OpSwitch && op != spv::OpBranchConditional && op != spv::OpPhi && op != spv::OpVectorExtractDynamic, "builtin specialization retained constant control flow or dynamic exports");
        if (op == spv::OpStore && specialized[cursor + 1u] == 31u) identity = specialized[cursor + 2u] == 30u;
        if (op == spv::OpStore && specialized[cursor + 1u] == 34u) phiValue = specialized[cursor + 2u];
        cursor += count;
    }
    bool correctPhi = false;
    for (std::size_t cursor = 5; cursor < specialized.size(); cursor += specialized[cursor] >> 16u) {
        if ((specialized[cursor] & 0xffffu) == spv::OpConstant && specialized[cursor + 2u] == phiValue) correctPhi = specialized[cursor + 3u] == 1u;
    }
    require(identity && correctPhi, "builtin specialization selected the wrong export or phi value");
    require(SpecializeSpirv(specialized) == specialized, "builtin specialization is not stable");
}

void verifySpecializationLiveness() {
    std::vector<std::uint32_t> words{spv::MagicNumber, 0x00010300u, 0u, 64u, 0u};
    const auto emit = [&](spv::Op op, std::initializer_list<std::uint32_t> operands) {
        words.push_back((static_cast<std::uint32_t>(operands.size() + 1u) << 16u) | op);
        words.insert(words.end(), operands);
    };
    emit(spv::OpCapability, {spv::CapabilityShader});
    emit(spv::OpMemoryModel, {spv::AddressingModelLogical, spv::MemoryModelGLSL450});
    emit(spv::OpEntryPoint, {spv::ExecutionModelGLCompute, 10u, 0x6e69616du, 0u});
    emit(spv::OpExecutionMode, {10u, spv::ExecutionModeLocalSize, 1u, 1u, 1u});
    emit(spv::OpDecorate, {27u, spv::DecorationRelaxedPrecision});
    emit(spv::OpTypeVoid, {1u});
    emit(spv::OpTypeInt, {3u, 32u, 0u});
    emit(spv::OpTypePointer, {4u, spv::StorageClassPrivate, 3u});
    emit(spv::OpTypeFunction, {5u, 1u});
    emit(spv::OpConstant, {3u, 6u, 0u});
    emit(spv::OpConstant, {3u, 7u, 1u});
    emit(spv::OpTypeStruct, {24u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u, 3u});
    emit(spv::OpConstantComposite, {24u, 25u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u, 7u});
    emit(spv::OpVariable, {4u, 8u, spv::StorageClassPrivate, 6u});
    emit(spv::OpFunction, {1u, 10u, spv::FunctionControlMaskNone, 5u});
    emit(spv::OpLabel, {11u});
    emit(spv::OpLoad, {3u, 12u, 8u});
    emit(spv::OpIMul, {3u, 13u, 12u, 7u});
    emit(spv::OpIAdd, {3u, 14u, 13u, 7u});
    emit(spv::OpCopyObject, {3u, 15u, 12u});
    emit(spv::OpCopyObject, {3u, 16u, 15u});
    emit(spv::OpIAdd, {3u, 17u, 16u, 7u});
    emit(spv::OpStore, {8u, 17u});
    emit(spv::OpLoad, {3u, 18u, 8u, spv::MemoryAccessVolatileMask});
    emit(spv::OpFunctionCall, {1u, 20u, 30u});
    emit(spv::OpCompositeExtract, {3u, 26u, 25u, 15u});
    emit(spv::OpStore, {8u, 26u});
    emit(spv::OpCopyObject, {3u, 27u, 12u});
    emit(spv::OpStore, {8u, 27u});
    emit(spv::OpLoad, {3u, 28u, 8u});
    emit(spv::OpReturn, {});
    emit(spv::OpFunctionEnd, {});
    emit(spv::OpFunction, {1u, 30u, spv::FunctionControlMaskNone, 5u});
    emit(spv::OpLabel, {31u});
    emit(spv::OpReturn, {});
    emit(spv::OpFunctionEnd, {});
    const auto specialized = SpecializeSpirv(words);
    std::map<std::uint32_t, std::vector<std::uint32_t>> results;
    std::size_t stores = 0;
    for (std::size_t cursor = 5; cursor < specialized.size();) {
        const auto count = specialized[cursor] >> 16u;
        const auto op = static_cast<spv::Op>(specialized[cursor] & 0xffffu);
        require(count != 0u && count <= specialized.size() - cursor, "dead computation removal truncated an instruction");
        if (op == spv::OpLoad || op == spv::OpCopyObject || op == spv::OpIAdd || op == spv::OpIMul || op == spv::OpCompositeExtract || op == spv::OpFunctionCall) results.emplace(specialized[cursor + 2u], std::vector<std::uint32_t>(specialized.begin() + cursor, specialized.begin() + cursor + count));
        if (op == spv::OpStore) ++stores;
        cursor += count;
    }
    for (const auto id : {13u, 14u, 15u, 16u, 28u}) require(!results.contains(id), "specialization retained a dead computation or copy chain");
    require(results.at(17u).at(3) == 12u, "copy propagation lost the live source");
    require(results.at(26u).at(4) == 15u, "copy propagation rewrote a literal as an ID");
    require(results.contains(18u) && results.contains(20u) && results.contains(27u) && stores == 3u, "specialization removed an effect or decorated copy");
    require(SpecializeSpirv(specialized) == specialized, "dead computation removal is not stable");
}

void verifyDefaultDirectory(const char* self) {
    setEnvironment("ANYPS5_SHADER_CACHE_DIR", "");
    setEnvironment("ANYPS5_NO_SHADER_CACHE", "1");
    require(ShaderRecompiler::ShaderCacheDirectory().empty(), "ANYPS5_NO_SHADER_CACHE=1 did not disable the cache");
    setEnvironment("ANYPS5_NO_SHADER_CACHE", "0");
    const auto directory = ShaderRecompiler::ShaderCacheDirectory();
    require(directory.filename() == "shader_cache", "the default cache directory is not named shader_cache");
    std::error_code error;
    require(std::filesystem::equivalent(directory.parent_path(), std::filesystem::absolute(self).parent_path(), error) && !error, "the default cache directory is not beside the executable");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string_view(argv[1]) == "--load") return runLoadingProcess();
        if (argc == 2 && std::string_view(argv[1]) == "--no-failure-memo") return runWithoutFailureMemo();
        const auto directory = std::filesystem::temp_directory_path() / ("aps5-shader-disk-cache-test-" + std::to_string(std::random_device{}()));
        std::filesystem::remove_all(directory);
        verifyDefaultDirectory(argv[0]);
        setEnvironment("ANYPS5_NO_SHADER_CACHE", "0");
        setEnvironment("ANYPS5_SHADER_CACHE_DIR", directory.string());
        verifyResultRoundTrip();
        verifyEntryRoundTrip();
        verifyArtifactStorageIsolation();
        verifyKeySensitivity();
        verifyFloatModeSerialization();
        verifyStore();
        verifyAcrossProcesses(argv[0]);
        verifyEmissionFailureMemo();
        verifyFailureMemoSwitch(argv[0]);
        verifyInvocationIsolation();
        verifyBufferAlignmentSpecialization();
        verifyBindingPlanSelection();
        verifyVertexTypeSpecialization();
        verifyBuiltinSpecialization();
        verifySpecializationLiveness();
        ShaderRecompiler::ShaderDiskCache::Flush();
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        require(!error, "cannot remove the shader cache test directory: " + error.message());
        std::cout << "shader disk cache tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
