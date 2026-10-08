#include "ShaderDiskCache.hpp"
#include "ShaderCacheDirectory.hpp"
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

void requireSameResult(const RecompileResult& left, const RecompileResult& right, const char* what) {
    const std::string prefix = std::string(what) + ": ";
    require(left.spirv.Words() == right.spirv.Words(), prefix + "SPIR-V differs");
    require(sameBindings(left.bindings, right.bindings), prefix + "bindings differ");
    require(left.pushConstants == right.pushConstants, prefix + "push constants differ");
    require(left.bdaAbiVersion == right.bdaAbiVersion, prefix + "BDA ABI version differs");
    require(left.vertexAttributes.size() == right.vertexAttributes.size(), prefix + "vertex attribute count differs");
    for (std::size_t i = 0; i < left.vertexAttributes.size(); ++i) {
        const auto& a = left.vertexAttributes[i];
        const auto& b = right.vertexAttributes[i];
        require(a.location == b.location && a.components == b.components && a.resource.fields == b.resource.fields && a.fetchIndex == b.fetchIndex, prefix + "vertex attribute differs");
    }
    require(left.vertexOffsetSgpr == right.vertexOffsetSgpr && left.instanceOffsetSgpr == right.instanceOffsetSgpr, prefix + "offset SGPRs differ");
    require(left.vertexOffsetShared == right.vertexOffsetShared && left.instanceOffsetShared == right.instanceOffsetShared && left.vertexOffsetConflict == right.vertexOffsetConflict && left.instanceOffsetConflict == right.instanceOffsetConflict, prefix + "offset flags differ");
    require(left.parameterExports == right.parameterExports, prefix + "parameter exports differ");
    require(left.fragmentParameters.size() == right.fragmentParameters.size(), prefix + "fragment parameter count differs");
    for (std::size_t i = 0; i < left.fragmentParameters.size(); ++i) {
        const auto& a = left.fragmentParameters[i];
        const auto& b = right.fragmentParameters[i];
        require(a.location == b.location && a.sourceLocation == b.sourceLocation && a.flat == b.flat && a.perVertex == b.perVertex, prefix + "fragment parameter differs");
    }
}

void requireSameVariant(const CompiledVariant& left, const CompiledVariant& right, const char* what) {
    requireSameResult(left.result, right.result, what);
    const std::string prefix = std::string(what) + ": ";
    const auto& a = left.info;
    const auto& b = right.info;
    require(a.stage == b.stage && a.shaderHash == b.shaderHash && a.waveSize == b.waveSize && a.userDataBase == b.userDataBase && a.userDataCount == b.userDataCount && a.scratchDwords == b.scratchDwords && a.paramExportMask == b.paramExportMask, prefix + "compiled info differs");
    require(a.info == b.info, prefix + "shader info differs");
    require(a.bindings == b.bindings, prefix + "info binding layout differs");
    require(sameBindings(left.bindings.bindings, right.bindings.bindings), prefix + "allocation bindings differ");
    require(left.bindings.layout == right.bindings.layout, prefix + "allocation layout differs");
    require(left.bindings.pushConstantOffsetBytes == right.bindings.pushConstantOffsetBytes && left.bindings.pushConstantSizeBytes == right.bindings.pushConstantSizeBytes, prefix + "push constant range differs");
    require(left.bindings.pushConstants == right.bindings.pushConstants, prefix + "allocation push constants differ");
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
    result.vertexAttributes = {{1, 4, {{0x1000u, 0x20000u, 0x30u, 0x4u}}, 2}, {5, 2, {{9u, 8u, 7u, 6u}}, 0}};
    result.vertexOffsetSgpr = 12;
    result.instanceOffsetSgpr = -1;
    result.vertexOffsetShared = true;
    result.instanceOffsetShared = false;
    result.vertexOffsetConflict = false;
    result.instanceOffsetConflict = true;
    result.parameterExports = {0, 3, 7};
    result.fragmentParameters = {{0, 1, true, false}, {2, 3, false, true}};
    result.variantId = 99;
    return result;
}

CompiledVariant sampleVariant() {
    CompiledVariant variant;
    variant.result = sampleResult();
    variant.result.bindings.clear();
    variant.result.pushConstants.clear();
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
    buffer.packedStride = 16;
    buffer.descriptorSwizzle = 0x123u;
    buffer.imageAlias = 1;
    buffer.read = true;
    buffer.atomic = true;
    buffer.scalar = true;
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
    image.indirectRoot = 0;
    image.indirectMappingOffset = 12;
    image.indirectSearchIterations = 3;
    image.indirectSlots = 4;
    info.info.images = {image};
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
    info.bindings.pushDataStartDword = 2;
    info.bindings.memoryOffsetDword = 1;
    info.bindings.memoryOffsetCount = 5;
    info.bindings.userDataRegisters = {4, 5, 9};
    info.bindings.descriptors = {{DescriptorBindingKind::Buffers, {0, 1}}, {DescriptorBindingKind::FlattenedSrt, {}}};
    variant.bindings.layout = info.bindings;
    variant.bindings.pushConstantOffsetBytes = 16;
    variant.bindings.pushConstantSizeBytes = 112;
    variant.bindings.bindings = {sampleBinding(3)};
    variant.bindings.pushConstants = {std::byte{9}, std::byte{8}};
    return variant;
}

struct SampleRequest {
    std::vector<std::uint32_t> code{0xe0700000u, 0x80000000u, 0xbf810000u, 0x12345678u};
    std::vector<std::uint32_t> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    std::array<std::uint32_t, 2> capabilities{1u, 61u};
    std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    std::array<std::byte, 16> header{};
    RecompileRequest request{};
    ResourceSpecialization specialization;
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
        specialization.buffers = {{16u, IrBufferFormat::Invalid, 0xfacu}};
        specialization.images = {ResourceSpecialization::Image{}};
        specialization.boundDescriptors = {0, 0};
    }

    std::vector<std::byte> Key() {
        request.shader.code = code;
        request.context.userData = userData;
        request.target.supportedCapabilities = capabilities;
        std::vector<std::byte> key;
        ShaderDiskCache::BuildKey(request, hostSubgroupSize, specialization, key);
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
}

void verifyEntryRoundTrip() {
    SampleRequest sample;
    const auto key = sample.Key();
    const auto variant = sampleVariant();
    const auto file = ShaderDiskCache::EncodeEntry(key, variant);
    CompiledVariant decoded;
    require(ShaderDiskCache::DecodeEntry(file, key, decoded) == ShaderDiskCache::LoadStatus::Loaded, "an encoded entry does not decode");
    requireSameVariant(variant, decoded, "entry round trip");

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
    changes("a buffer stride", [](SampleRequest& sample) { sample.specialization.buffers[0].packedStride = 32; });
    changes("a buffer format", [](SampleRequest& sample) { sample.specialization.buffers[0].descriptorFormat = static_cast<IrBufferFormat>(1); });
    changes("a buffer swizzle", [](SampleRequest& sample) { sample.specialization.buffers[0].descriptorSwizzle = 0xfadu; });
    changes("the buffer count", [](SampleRequest& sample) { sample.specialization.buffers.emplace_back(); });
    changes("an image class", [](SampleRequest& sample) { sample.specialization.images[0].numericClass = IrTextureNumericClass::Float; });
    changes("an image dimension", [](SampleRequest& sample) { sample.specialization.images[0].dimension = static_cast<RdnaImageDimension>(1); });
    changes("an image mip count", [](SampleRequest& sample) { sample.specialization.images[0].mipCount = 2; });
    changes("an image conversion", [](SampleRequest& sample) { sample.specialization.images[0].conversionFormat = static_cast<IrBufferFormat>(1); });
    changes("an image swizzle", [](SampleRequest& sample) { sample.specialization.images[0].shaderSwizzle = 0; });
    changes("an image indirect root", [](SampleRequest& sample) { sample.specialization.images[0].indirectRoot = 0; });
    changes("an image mapping offset", [](SampleRequest& sample) { sample.specialization.images[0].indirectMappingOffset = 4; });
    changes("an image search depth", [](SampleRequest& sample) { sample.specialization.images[0].indirectSearchIterations = 2; });
    changes("an image table slot count", [](SampleRequest& sample) { sample.specialization.images[0].indirectSlots = 1024; });
    changes("an image cube flag", [](SampleRequest& sample) { sample.specialization.images[0].cube = true; });
    changes("an image FMASK flag", [](SampleRequest& sample) { sample.specialization.images[0].fmask = true; });
    changes("an image sRGB decode", [](SampleRequest& sample) { sample.specialization.images[0].srgbDecode = true; });
    changes("the image count", [](SampleRequest& sample) { sample.specialization.images.emplace_back(); });
    changes("the bound descriptors", [](SampleRequest& sample) { sample.specialization.boundDescriptors.push_back(1); });

    SampleRequest moved;
    moved.userData[0] ^= 0x10000u;
    moved.request.shader.codeAddress += 0x1000000u;
    moved.request.shader.headerAddress += 0x1000000u;
    moved.header[3] = std::byte{1};
    require(moved.Key() == key, "the key depends on the user data values or the addresses");
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
    std::array<std::uint32_t, 1> capabilities{1u};
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
    std::array<std::uint32_t, 1> capabilities{1u};
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
        request.layout.pushConstantSizeBytes = 128;
    }
};

std::string emissionFailure(const RecompileRequest& request) {
    try {
        static_cast<void>(Recompile(request));
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
    require(diskMisses() == before.misses + 3, "another buffer specialization reused the failure");
    ClockRequest relocated;
    relocated.request.shader.codeAddress += 0x1000u;
    static_cast<void>(emissionFailure(relocated.request));
    require(diskMisses() == before.misses + 4, "a relocated copy reused the failure");
    require(emissionFailure(clock.request) == first && diskMisses() == before.misses + 4, "the other requests replaced the failure");

    ClockRequest concurrent;
    concurrent.request.layout.pushConstantSizeBytes = 64;
    std::array<std::future<std::string>, 4> failures;
    for (auto& future : failures) future = std::async(std::launch::async, [&concurrent] { return emissionFailure(concurrent.request); });
    std::vector<std::string> messages;
    for (auto& future : failures) messages.push_back(future.get());
    require(std::all_of(messages.begin(), messages.end(), [&](const std::string& message) { return message == messages.front(); }), "concurrent requests failed with different messages");
    require(diskMisses() == before.misses + 5, "concurrent requests emitted the program " + std::to_string(diskMisses() - before.misses - 4) + " times");

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
        verifyKeySensitivity();
        verifyStore();
        verifyAcrossProcesses(argv[0]);
        verifyEmissionFailureMemo();
        verifyFailureMemoSwitch(argv[0]);
        std::error_code error;
        std::filesystem::remove_all(directory, error);
        std::cout << "shader disk cache tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
