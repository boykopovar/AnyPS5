#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Optimization/DescriptorBindingBuilder.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/ShaderStageInputInfo.hpp"
#include "Optimization/SrtWalker/SrtEvaluator.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#include "SpirvBackend/SpirvAnalysis.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include "CacheKey.hpp"
#include "BdaAbi.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <map>
#include <future>
#include <memory>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace {

ShaderRecompiler::SpirvTarget BufferTarget() {
    static constexpr std::array<std::uint32_t, 5> capabilities{spv::CapabilityShader, spv::CapabilityImageGatherExtended, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    static constexpr std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    ShaderRecompiler::SpirvTarget target{};
    target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    target.supportedCapabilities = capabilities;
    target.supportedExtensions = extensions;
    return target;
}

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void expectFailure(TAction action, const char* expected, const char* message) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        if (std::string(error.what()).find(expected) == std::string::npos) throw std::runtime_error(std::string(message) + ": expected failure containing '" + expected + "', got: " + error.what());
        return;
    }
    throw std::runtime_error(message);
}

void verifyResult(const ShaderRecompiler::RecompileResult& first, const ShaderRecompiler::RecompileResult& second) {
    require(first.spirv == second.spirv, "replayed SPIR-V differs");
    require(first.pushConstants == second.pushConstants, "replayed push constants differ");
    require(first.bdaAbiVersion == second.bdaAbiVersion && first.bindings.size() == second.bindings.size(), "replayed layout differs");
    for (std::size_t index = 0; index < first.bindings.size(); ++index) {
        const auto& left = first.bindings[index];
        const auto& right = second.bindings[index];
        require(left.kind == right.kind && left.role == right.role && left.descriptorSet == right.descriptorSet && left.binding == right.binding && left.count == right.count && left.guestDescriptor == right.guestDescriptor && left.readOnly == right.readOnly, "replayed binding differs");
    }
}

void verifyRegisterSources() {
    using namespace ShaderRecompiler;
    IrResourcePlan plan;
    IrValue samplerRegister(IrOpcode::Void, IrType::ScalarReg, 0);
    IrValue bufferRegister(IrOpcode::Void, IrType::ScalarReg, 1);
    IrValue sameSamplerRegister(IrOpcode::Void, IrType::ScalarReg, 2);
    samplerRegister.SetRegister({RegisterBank::Scalar, 8});
    bufferRegister.SetRegister({RegisterBank::Scalar, 12});
    sameSamplerRegister.SetRegister({RegisterBank::Scalar, 8});
    IrValue samplerRead(IrOpcode::GetUserData, IrType::U32, 3);
    IrValue bufferRead(IrOpcode::GetUserData, IrType::U32, 4);
    IrValue sameSamplerRead(IrOpcode::GetUserData, IrType::U32, 5);
    samplerRead.AddArgument(&samplerRegister);
    bufferRead.AddArgument(&bufferRegister);
    sameSamplerRead.AddArgument(&sameSamplerRegister);
    require(!EquivalentValue(plan, &samplerRead, &bufferRead), "sampler SGPRs were merged with buffer SGPRs");
    require(EquivalentValue(plan, &samplerRead, &sameSamplerRead), "identical user data reads were not recognized");
    sameSamplerRegister.SetRegister({RegisterBank::UserData, 8});
    require(!EquivalentValue(plan, &samplerRead, &sameSamplerRead), "different register banks were merged");
    IrValue firstVector(IrOpcode::Void, IrType::VectorReg, 6);
    IrValue secondVector(IrOpcode::Void, IrType::VectorReg, 7);
    firstVector.SetRegister({RegisterBank::Vector, 0});
    secondVector.SetRegister({RegisterBank::Vector, 1});
    require(!EquivalentValue(plan, &firstVector, &secondVector), "different vector registers were merged");
    require(!EquivalentValue(plan, &samplerRegister, &firstVector), "different register types were merged");
}

void verifyEvaluatedValues() {
    using namespace ShaderRecompiler;
    std::vector<std::unique_ptr<IrValue>> values;
    for (std::uint32_t id = 0; id < 1000u; id++) {
        values.push_back(std::make_unique<IrValue>(IrOpcode::Void, IrType::U32, id));
    }
    Detail::EvaluatedValues table;
    std::uint64_t found = 0;
    require(!table.Find(values.front().get(), found), "evaluated values: an empty table found a value");
    for (std::uint32_t id = 0; id < values.size(); id++) {
        table.Insert(values[id].get(), std::uint64_t{id} * 3u);
    }
    for (std::uint32_t id = 0; id < values.size(); id++) {
        require(table.Find(values[id].get(), found) && found == std::uint64_t{id} * 3u, "evaluated values: a value was lost when the table grew");
    }
    table.Insert(values[7].get(), 0u);
    require(table.Find(values[7].get(), found) && found == 21u, "evaluated values: a second insert replaced the first value");
    IrValue absent(IrOpcode::Void, IrType::U32, 1000u);
    require(!table.Find(&absent, found), "evaluated values: a value that was never inserted was found");
}

// The pure flat slots of a hand-built plan (Detail::ComputePureFlatSlots): a slot is pure unless
// a descriptor dword, a condition, a uniform value or another slot's address cone reaches it.
void verifyPureFlatSlots() {
    using namespace ShaderRecompiler;
    std::vector<std::unique_ptr<IrValue>> values;
    std::uint32_t ids = 0;
    const auto make = [&](IrOpcode opcode, IrType type) -> IrValue& {
        values.push_back(std::make_unique<IrValue>(opcode, type, ids++));
        return *values.back();
    };
    const auto constant = [&](std::uint32_t value) -> IrValue& {
        auto& immediate = make(IrOpcode::Void, IrType::U32);
        immediate.SetImmediateU32(value);
        return immediate;
    };
    auto& resource = make(IrOpcode::GetSrtResource, IrType::SrtResource);
    const auto userData = [&](std::uint32_t index) -> IrValue& {
        auto& reg = make(IrOpcode::Void, IrType::ScalarReg);
        reg.SetRegister({RegisterBank::Scalar, index});
        auto& read = make(IrOpcode::GetUserData, IrType::U32);
        read.AddArgument(&reg);
        return read;
    };
    const auto handle = [&](IrValue& low, IrValue& high) -> IrValue& {
        auto& composed = make(IrOpcode::CompositeConstructU64, IrType::U64);
        composed.AddArgument(&low);
        composed.AddArgument(&high);
        return composed;
    };
    const auto rawRead = [&](IrValue& address, std::uint32_t offset) -> IrValue& {
        auto& read = make(IrOpcode::LoadAddressU32, IrType::U32);
        read.AddArgument(&address);
        read.AddArgument(&constant(offset));
        return read;
    };
    const auto readConst = [&](std::uint32_t slot) -> IrValue& {
        auto& read = make(IrOpcode::ReadConst, IrType::U32);
        read.AddArgument(&resource);
        read.AddArgument(&constant(slot));
        return read;
    };
    // Slots: A (0) reads through a pointer B (1) read from user data; C (2) and D (3) read from
    // user-data handles; a descriptor source consumes C.
    auto& b = rawRead(handle(userData(0), userData(1)), 0);
    auto& c = rawRead(handle(userData(2), userData(3)), 4);
    auto& a = rawRead(handle(readConst(1), userData(4)), 8);
    auto& d = rawRead(handle(userData(5), userData(6)), 12);
    IrResourcePlan plan;
    plan.srtPlanComplete = true;
    plan.resourceTrackingComplete = true;
    plan.srtReads = {{&a, 0}, {&b, 1}, {&c, 2}, {&d, 3}};
    DescriptorSource source;
    source.dwordCount = 1;
    source.dwords[0] = &readConst(2);
    plan.descriptorSources.push_back(source);
    using Pure = std::vector<std::uint8_t>;
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 0, 1}, "pure flat slots: the address cone or the descriptor source was not excluded");
    plan.controlFlow.push_back({&readConst(3), {}, {}});
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 0, 0}, "pure flat slots: a control-flow condition was not excluded");
    plan.controlFlow.clear();
    auto& phi = make(IrOpcode::Phi, IrType::U32);
    phi.AddArgument(&readConst(0));
    phi.AddArgument(&constant(0));
    plan.descriptorSources[0].dwords[0] = &phi;
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 1, 1}, "pure flat slots: a phi argument was not excluded");
    plan.descriptorSources[0].dwords[0] = &b;
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 1, 1}, "pure flat slots: a raw read named directly was not excluded");
    plan.descriptorSources[0].dwords[0] = &readConst(2);
    plan.uniformFill.fill.kind = UniformFillKind::Buffer;
    plan.uniformFill.fill.words = 1;
    plan.uniformFill.values[0] = &readConst(3);
    require(Detail::ComputePureFlatSlots(plan) == Pure{1, 0, 0, 0}, "pure flat slots: a uniform-fill value was not excluded");
    plan.uniformFill = {};
    plan.descriptorSources[0].indirectImage = DescriptorSource::IndirectImage{};
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 0, 0}, "pure flat slots: an indirect image did not disqualify the plan");
    plan.descriptorSources[0].indirectImage.reset();
    plan.requiresSpecializationMemory = true;
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 0, 0}, "pure flat slots: specialization memory did not disqualify the plan");
    plan.requiresSpecializationMemory = false;
    plan.srtPlanComplete = false;
    require(Detail::ComputePureFlatSlots(plan) == Pure{0, 0, 0, 0}, "pure flat slots: an incomplete plan was classified");
}

// A compute program sampling a T# loaded from a table buffer at a runtime key (a bindless image
// table): mode M enumerates the keys from the material records, mode T binds the whole table.
void verifyBindlessTable() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    const std::uint32_t slots = ResourceMaterializer::BindlessSlots();

    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture textures[2];
    struct alignas(4096) GuestTables {
        std::array<std::array<std::uint32_t, 8>, 4> heap{};
        std::array<std::array<std::uint32_t, 4>, 3> materials{};
        std::array<std::uint32_t, 4> output{};
        std::array<std::uint32_t, 16> srt{};
    };
    static GuestTables guest;
    auto& heap = guest.heap;
    auto& materials = guest.materials;
    auto& output = guest.output;
    auto& srt = guest.srt;
    const auto makeTexture = [&](std::uint32_t entry, const Texture& texture) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
        heap[entry] = {static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    makeTexture(0, textures[0]);
    makeTexture(1, textures[1]);
    heap[3] = heap[0];
    materials = {{{0u, 1u, 0u, 0u}, {0u, 0u, 0u, 0u}, {0u, 3u, 0u, 0u}}};
    const auto bufferDescriptor = [](const void* base, std::uint32_t stride, std::uint32_t records) {
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
        return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, 0xfacu};
    };
    const auto fillSrt = [&](std::uint32_t heapRecords) {
        const auto heapV = bufferDescriptor(heap.data(), 32u, heapRecords);
        const auto materialV = bufferDescriptor(materials.data(), 16u, 3u);
        const auto outputV = bufferDescriptor(output.data(), 0u, 16u);
        std::copy(heapV.begin(), heapV.end(), srt.begin());
        srt[4] = 0u; srt[5] = 0u; srt[6] = 0u; srt[7] = 0u;
        std::copy(materialV.begin(), materialV.end(), srt.begin() + 8);
        std::copy(outputV.begin(), outputV.end(), srt.begin() + 12);
    };
    fillSrt(4u);

    // s_load_dwordx4 x4 (heap V#, S#, material V#, output V#); v_readfirstlane_b32 s16, v0;
    // s_mul_i32 s16, s16, 16; s_buffer_load_dword s16, s[12:15], s16 offset:4; s_lshl_b32 s16, s16, 5;
    // s_buffer_load_dwordx8 s[20:27], s[4:7], s16; image_sample_lz v[0:3], v[0:1], s[20:27], s[8:11];
    // buffer_store_dword v0, off, s[28:31], 0; s_endpgm.
    const std::vector<std::uint32_t> materialCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x93109010u, 0xf4200406u, 0x20000004u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    // The same without the material read: the key is the wave's first lane id.
    const std::vector<std::uint32_t> wholeCode{0xf4080100u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xf4080300u, 0xfa000020u, 0xf4080700u, 0xfa000030u, 0x7e200500u, 0x8f108510u, 0xf42c0502u, 0x20000000u, 0xf09c0f08u, 0x00450000u, 0xe0700000u, 0x80070000u, 0xbf810000u};
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    // A wave64 workgroup on a 32-wide host is held by one subgroup (two lanes per invocation).
    const auto makeRequest = [&](const std::vector<std::uint32_t>& code) {
        RecompileRequest request{};
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
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        return request;
    };
    const auto covered = [](const std::vector<MemoryRegion>& regions, const void* pointer, std::size_t bytes) {
        auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
        const auto end = address + bytes;
        while (address < end) {
            const auto region = std::find_if(regions.begin(), regions.end(), [&](const MemoryRegion& candidate) { return address >= candidate.guestAddress && address < candidate.guestAddress + candidate.bytes.size(); });
            if (region == regions.end()) return false;
            address = region->guestAddress + region->bytes.size();
        }
        return true;
    };
    const auto mappingOf = [&](const ResourceSnapshot& snapshot) {
        require(snapshot.flattenedSrt.size() >= 1u + 2u * slots, "bindless: the mapping block is missing from the flattened SRT");
        return std::vector<std::uint32_t>(snapshot.flattenedSrt.end() - static_cast<std::ptrdiff_t>(1u + 2u * slots), snapshot.flattenedSrt.end());
    };
    const auto tableRoot = [&](const ResourceCapture& capture, std::uint32_t direct) {
        require(capture.snapshot.images.size() == direct + slots - 1u, "bindless: the snapshot does not hold the table slots");
        std::uint32_t root = ImageResource::NoIndirectImage;
        for (std::uint32_t i = 0; i < direct; i++) {
            if (capture.plan->descriptorSources.at(capture.plan->info.images.at(i).source).indirectImage.has_value()) root = i;
        }
        require(root != ImageResource::NoIndirectImage, "bindless: no table root");
        return root;
    };

    auto request = makeRequest(materialCode);
    const auto plan = GetResourcePlan(request);
    std::size_t tables = 0;
    for (const auto& source : plan->descriptorSources) {
        if (!source.indirectImage.has_value()) continue;
        ++tables;
        const auto& table = *source.indirectImage;
        require(table.hasMaterial && table.selectorStride == 16u && table.selectorOffset == 4u && table.entryOffset == 0u, "bindless: the material pattern was not recorded");
    }
    require(tables == 1, "bindless: the table source was not planned");
    for (const auto& image : plan->info.images) require(image.indirectSearchIterations == 0u, "bindless: the plan carries a search depth");
    const auto direct = static_cast<std::uint32_t>(plan->info.images.size());
    auto staticRequest = request;
    const std::array<std::uint32_t, 2> absentResources{};
    staticRequest.context.userData = absentResources;
    staticRequest.context.memory = {};
    auto staticProgram = PrepareResourceProgram(staticRequest);
    ResourceMaterializer{}.ApplyStaticInterface(staticProgram);
    const auto& staticImages = staticProgram.Resources().info.images;
    require(staticImages.size() == direct + slots - 1u, "bindless: the static interface needs runtime descriptors");
    const auto staticRoot = std::ranges::find_if(staticImages, [](const ImageResource& image) { return image.indirectSearchIterations != 0u; });
    require(staticRoot != staticImages.end() && staticRoot->indirectResources.size() == slots && staticRoot->indirectMappingOffset == plan->srtReads.size(), "bindless: the static table interface is incomplete");

    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto root = tableRoot(*capture, direct);
    require(capture->snapshot.images[root].dwords == heap[0] && capture->snapshot.images[direct].dwords == heap[1] && capture->snapshot.images[direct + 1u].dwords == heap[3], "bindless: the slots do not hold the keyed entries");
    for (std::uint32_t i = direct + 2u; i < capture->snapshot.images.size(); i++) require(capture->snapshot.images[i].dwords == heap[2], "bindless: a pad slot is not null");
    const auto mapping = mappingOf(capture->snapshot);
    require(std::vector<std::uint32_t>(mapping.begin(), mapping.begin() + 7) == std::vector<std::uint32_t>{3u, 0u, 0u, 1u, 1u, 3u, 2u}, "bindless: the (key, slot) mapping is wrong");
    require(capture->plan->srtReads.size() + mapping.size() == capture->snapshot.flattenedSrt.size(), "bindless: the mapping offset does not name the block");
    auto regions = memory.Regions();
    for (const auto& material : materials) require(covered(regions, &material[1], sizeof(std::uint32_t)), "bindless: a material key was not captured");
    for (const auto entry : {0u, 1u, 3u}) require(covered(regions, heap[entry].data(), 32u), "bindless: a table entry was not captured");
    request.context.memory = regions;
    const auto compiled = Recompile(request, *capture);
    bool sampled = false;
    bool flattened = false;
    for (const auto& binding : compiled->bindings) {
        if (binding.role == DescriptorRole::FlattenedSrt) flattened = true;
        if (binding.kind != DescriptorKind::SampledImage) continue;
        sampled = true;
        require(binding.count == direct + slots - 1u && binding.guestDescriptor.size() == 8u * binding.count, "bindless: the sampled image binding does not hold the table slots");
        require(std::none_of(binding.imageWritten.begin(), binding.imageWritten.end(), [](bool written) { return written; }), "bindless: a table slot is marked written");
    }
    require(sampled && flattened, "bindless: the bindings lack the image array or the flattened SRT");
    struct SpirvScan {
        bool dynamicIndexing = false;
        bool shaderNonUniform = false;
        bool nonUniform = false;
        bool switched = false;
    };
    const auto scan = [&](const std::vector<std::uint32_t>& words) {
        SpirvScan result;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "bindless: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            if (op == 17u && words[cursor + 1] == 29u) result.dynamicIndexing = true;
            if (op == 17u && words[cursor + 1] == 5301u) result.shaderNonUniform = true;
            if (op == 71u && words[cursor + 2] == 5300u) result.nonUniform = true;
            if (op == 251u) result.switched = true;
            cursor += count;
        }
        return result;
    };
    const auto uniform = scan(compiled->spirv);
    require(uniform.dynamicIndexing && uniform.switched, "bindless: the SPIR-V does not index the image array dynamically");
    require(!uniform.shaderNonUniform && !uniform.nonUniform, "bindless: a single-subgroup workgroup was decorated NonUniform");
#if ANYPS5_ENABLE_SPIRV_TOOLS
    static_cast<void>(ValidateAndOptimizeSpirv(compiled->spirv, request.target.vulkanVersion, request.target.spirvVersion));
#endif

    // A wave64 workgroup kept at one lane per invocation (a 64-wide host) spans two subgroups, so
    // the slot needs NonUniform: rejected without the descriptor indexing capabilities, decorated
    // with them.
    auto split = request;
    split.target.subgroupSize = 64;
    AgcDriver::ShaderMemory splitMemory({});
    const auto splitCapture = splitMemory.Capture(split);
    expectFailure([&] { static_cast<void>(Recompile(split, *splitCapture)); }, "not uniform over the workgroup", "bindless: a split wave indexed the image array as uniform");
    const std::array<std::uint32_t, 6> indexingCapabilities{29u, 5301u, 5307u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 3> indexingExtensions{"SPV_EXT_descriptor_indexing", "SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    split.target.supportedCapabilities = indexingCapabilities;
    split.target.supportedExtensions = indexingExtensions;
    AgcDriver::ShaderMemory indexingMemory({});
    const auto indexingCapture = indexingMemory.Capture(split);
    const auto splitScan = scan(Recompile(split, *indexingCapture)->spirv);
    require(splitScan.dynamicIndexing && splitScan.shaderNonUniform && splitScan.nonUniform, "bindless: a split wave's slot is not decorated NonUniform");

    // A key past the table (the no-texture marker 0xffffffff among them) and a key naming a null
    // entry are left out of the mapping, so they sample zeros; the variant is the same.
    for (const auto unmapped : {9u, 0xffffffffu}) {
        materials[2][1] = unmapped;
        AgcDriver::ShaderMemory rangeMemory({});
        const auto rangeCapture = rangeMemory.Capture(request);
        const auto rangeMapping = mappingOf(rangeCapture->snapshot);
        require(std::vector<std::uint32_t>(rangeMapping.begin(), rangeMapping.begin() + 5) == std::vector<std::uint32_t>{2u, 0u, 0u, 1u, 1u}, "bindless: an out-of-range key was kept");
        request.context.memory = rangeMemory.Regions();
        require(Recompile(request, *rangeCapture)->variantId == compiled->variantId, "bindless: the keys changed the variant");
    }
    materials[2][1] = 2u;
    AgcDriver::ShaderMemory nullMemory({});
    const auto nullCapture = nullMemory.Capture(request);
    const auto nullMapping = mappingOf(nullCapture->snapshot);
    require(std::vector<std::uint32_t>(nullMapping.begin(), nullMapping.begin() + 5) == std::vector<std::uint32_t>{2u, 0u, 0u, 1u, 1u}, "bindless: a null entry's key was mapped");
    require(nullCapture->snapshot.images[direct + 1u].dwords == heap[2], "bindless: a null entry's slot is not the pad");
    materials[2][1] = 3u;

    auto whole = makeRequest(wholeCode);
    const auto wholePlan = GetResourcePlan(whole);
    for (const auto& source : wholePlan->descriptorSources) {
        if (source.indirectImage.has_value()) require(!source.indirectImage->hasMaterial, "bindless: a material pattern was recorded without one");
    }
    AgcDriver::ShaderMemory wholeMemory({});
    const auto wholeCapture = wholeMemory.Capture(whole);
    const auto wholeDirect = static_cast<std::uint32_t>(wholePlan->info.images.size());
    const auto wholeRoot = tableRoot(*wholeCapture, wholeDirect);
    const auto wholeMapping = mappingOf(wholeCapture->snapshot);
    require(std::vector<std::uint32_t>(wholeMapping.begin(), wholeMapping.begin() + 7) == std::vector<std::uint32_t>{3u, 0u, 0u, 1u, 1u, 3u, 3u}, "bindless: mode T is not the identity mapping");
    require(wholeCapture->snapshot.images[wholeRoot].dwords == heap[0] && wholeCapture->snapshot.images[wholeDirect].dwords == heap[1] && wholeCapture->snapshot.images[wholeDirect + 1u].dwords == heap[2] && wholeCapture->snapshot.images[wholeDirect + 2u].dwords == heap[3], "bindless: mode T slots are wrong");
    whole.context.memory = wholeMemory.Regions();
    require(!Recompile(whole, *wholeCapture)->spirv.empty(), "bindless: mode T did not compile");

    // A table wider than the slots without a material pattern is rejected.
    fillSrt(100u);
    AgcDriver::ShaderMemory wideMemory({});
    expectFailure([&] { static_cast<void>(wideMemory.Capture(whole)); }, "bindless image table has 100 entries", "bindless: a wide table was bound");
    fillSrt(4u);
    const auto savedHeap = heap;
    heap = {};
    AgcDriver::ShaderMemory emptyMemory({});
    const auto emptyCapture = emptyMemory.Capture(request);
    require(mappingOf(emptyCapture->snapshot).front() == 0u, "bindless: an empty table has mapped keys");
    request.context.memory = emptyMemory.Regions();
    require(Recompile(request, *emptyCapture)->variantId == compiled->variantId, "bindless: an empty table changed the artifact");
    heap = savedHeap;
    heap[0][3] &= 0x0fffffffu;
    AgcDriver::ShaderMemory invalidMemory({});
    expectFailure([&] { static_cast<void>(invalidMemory.Capture(request)); }, "invalid descriptor", "bindless: an invalid T# was accepted");
    heap = savedHeap;
    srt[1] &= 0xffffu;
    srt[2] = 127u;
    AgcDriver::ShaderMemory partialMemory({});
    expectFailure([&] { static_cast<void>(partialMemory.Capture(request)); }, "partial descriptor", "bindless: a partial heap descriptor was accepted");
    fillSrt(4u);
}


void verifyDescriptorPhis() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture textures[2];
    const auto imageDescriptor = [&](const Texture& texture) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
        return std::array<std::uint32_t, 8>{static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    const std::array<std::uint32_t, 4> pointWrap{0u, 0u, 0u, 0u};
    const std::array<std::uint32_t, 4> linearMirror{0x49u, 0u, 0x00500000u, 0u};
    std::array<std::uint32_t, 4> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    std::array<std::uint32_t, 32> srt{};
    const auto first = imageDescriptor(textures[0]);
    const auto second = imageDescriptor(textures[1]);
    std::copy(first.begin(), first.end(), srt.begin());
    std::copy(pointWrap.begin(), pointWrap.end(), srt.begin() + 8);
    std::copy(linearMirror.begin(), linearMirror.end(), srt.begin() + 12);
    const std::array<std::uint32_t, 4> outputV{static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 16u, 0xfacu};
    std::copy(outputV.begin(), outputV.end(), srt.begin() + 16);
    srt[20] = 1u;
    std::copy(second.begin(), second.end(), srt.begin() + 24);
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 4> capabilities{29u, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    const auto makeRequest = [&](const std::vector<std::uint32_t>& code, std::uint32_t waveSize = 32u) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x21000u, code, 0, {}};
        request.context.waveSize = waveSize;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{waveSize, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        return request;
    };
    const auto countOps = [](const std::vector<std::uint32_t>& words, std::uint32_t opcode) {
        std::size_t count = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto length = words[cursor] >> 16u;
            require(length != 0 && length <= words.size() - cursor, "descriptor Phi: truncated SPIR-V instruction");
            count += (words[cursor] & 0xffffu) == opcode ? 1u : 0u;
            cursor += length;
        }
        return count;
    };
    constexpr std::uint32_t OpImageSampleExplicitLod = 88;

    const std::vector<std::uint32_t> samplerCode{0xf40c0200u, 0xfa000000u, 0xf4000400u, 0xfa000050u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf4080500u, 0xfa000020u, 0xbf820002u, 0xf4080500u, 0xfa000030u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const std::vector<std::uint32_t> imageCode{0xf4000400u, 0xfa000050u, 0xf4080500u, 0xfa000020u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf40c0200u, 0xfa000000u, 0xbf820002u, 0xf40c0200u, 0xfa000060u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const std::vector<std::uint32_t> dynamicCode{0xf40c0200u, 0xfa000000u, 0xf4000400u, 0xfa000050u, 0xbf8cc07fu, 0xbf068010u, 0xbf850003u, 0xf4080500u, 0xfa000020u, 0xbf820002u, 0xf4080500u, 0x20000000u, 0xf4080600u, 0xfa000040u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};

    const auto compile = [&](const std::vector<std::uint32_t>& code, std::size_t images, std::size_t samplers) {
        auto request = makeRequest(code);
        const auto plan = GetResourcePlan(request);
        require(plan->info.images.size() == images && plan->info.samplers.size() == samplers && plan->info.sampledPairs.size() == 2u, "descriptor Phi: the edges were not given one resource each");
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        request.context.memory = memory.Regions();
        const auto compiled = Recompile(request, *capture);
        require(countOps(compiled->spirv, OpImageSampleExplicitLod) == 2u, "descriptor Phi: the specialized SPIR-V does not sample once per edge");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        static_cast<void>(ValidateAndOptimizeSpirv(compiled->spirv, request.target.vulkanVersion, request.target.spirvVersion));
#endif
        return capture;
    };
    const auto samplerCapture = compile(samplerCode, 1u, 2u);
    const auto& samplers = samplerCapture->snapshot.samplers;
    require(samplers.size() == 2u, "descriptor Phi: the snapshot does not hold both S#s");
    const auto holds = [&](const std::array<std::uint32_t, 4>& words) {
        return std::ranges::any_of(samplers, [&](const DescriptorValue& value) {
            return value.dwordCount == 4u && std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(holds(pointWrap) && holds(linearMirror), "descriptor Phi: the snapshot S#s are not the two edges' S#s");

    const auto imageCapture = compile(imageCode, 2u, 1u);
    const auto& images = imageCapture->snapshot.images;
    require(images.size() == 2u, "descriptor Phi: the snapshot does not hold both T#s");
    const auto holdsImage = [&](const std::array<std::uint32_t, 8>& words) {
        return std::ranges::any_of(images, [&](const DescriptorValue& value) {
            return std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(holdsImage(first) && holdsImage(second), "descriptor Phi: the snapshot T#s are not the two edges' T#s");

    auto twoLane = makeRequest(samplerCode, 64u);
    AgcDriver::ShaderMemory twoLaneMemory({});
    const auto twoLaneCapture = twoLaneMemory.Capture(twoLane);
    twoLane.context.memory = twoLaneMemory.Regions();
    require(countOps(Recompile(twoLane, *twoLaneCapture)->spirv, OpImageSampleExplicitLod) == 4u, "descriptor Phi: the specialized two-lane SPIR-V does not sample once per edge and half");

    auto dynamic = makeRequest(dynamicCode);
    expectFailure([&] { static_cast<void>(GetResourcePlan(dynamic)); }, "GetSamplerResource dword 0 is not a valid runtime value", "descriptor Phi: an edge without an SRT slot was accepted");

    const std::vector<std::uint32_t> loopCode{0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u, 0xbe910380u, 0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf068011u, 0xbf850003u, 0xf40c0200u, 0xfa000000u, 0xbf820002u, 0xf40c0200u, 0xfa000060u, 0xbf8cc07fu, 0x80118111u, 0xbf0a8211u, 0xbf85fff0u, 0xbf810000u};
    const auto loopCapture = compile(loopCode, 2u, 1u);
    const auto& loopImages = loopCapture->snapshot.images;
    const auto holdsLoopImage = [&](const std::array<std::uint32_t, 8>& words) {
        return std::ranges::any_of(loopImages, [&](const DescriptorValue& value) {
            return std::equal(words.begin(), words.end(), value.dwords.begin());
        });
    };
    require(loopImages.size() == 2u && holdsLoopImage(first) && holdsLoopImage(second), "descriptor Phi: the loop's chained T# Phis were not split into the two SRT T#s");

    const auto entryWrites = [&](std::initializer_list<std::uint32_t> words) {
        auto code = loopCode;
        code.insert(code.begin() + 8, words);
        return code;
    };
    const std::array<std::pair<std::vector<std::uint32_t>, const char*>, 3> invalidEntries{{
        {entryWrites({0xbe8e1f00u}), "descriptor Phi: a T# holding the program counter was accepted"},
        {entryWrites({0xbe8e037eu}), "descriptor Phi: a T# holding EXEC was accepted"},
        {entryWrites({0x7d840080u, 0xbe8e036au}), "descriptor Phi: a T# holding a compare mask was accepted"},
    }};
    for (const auto& [code, message] : invalidEntries) {
        auto request = makeRequest(code);
        expectFailure([&] { static_cast<void>(GetResourcePlan(request)); }, "GetImageResource dword 0 is not a valid runtime value", message);
    }
    auto unwritten = loopCode;
    unwritten[4] = 0xf4080200u;
    auto unwrittenRequest = makeRequest(unwritten);
    require(GetResourcePlan(unwrittenRequest)->info.images.size() == 3u, "descriptor Phi: a T# whose unwritten registers read as 0 was not planned as its own image");

    const std::array<std::uint32_t, 7> prologue{0xf4080500u, 0xfa000020u, 0xf4080600u, 0xfa000040u, 0xf40c0200u, 0xfa000000u, 0xbf8cc07fu};
    const std::array<std::uint32_t, 7> epilogue{0xbf8cc07fu, 0xf09c0f08u, 0x00a20000u, 0xbf8c3f70u, 0xe0700000u, 0x80060000u, 0xbf810000u};
    const auto planImages = [&](const std::vector<std::uint32_t>& body) {
        std::vector<std::uint32_t> code(prologue.begin(), prologue.end());
        code.insert(code.end(), body.begin(), body.end());
        code.insert(code.end(), epilogue.begin(), epilogue.end());
        auto request = makeRequest(code);
        return GetResourcePlan(request)->info.images.size();
    };
    const auto distinctLoads = [](std::uint32_t loads) {
        std::vector<std::uint32_t> body;
        for (std::uint32_t load = 0; load < loads; load++) {
            body.insert(body.end(), {0xbf068014u, 0xbf850002u, 0xf40c0200u, 0xfa000060u + load * 0x20u});
        }
        return body;
    };
    require(planImages(distinctLoads(63u)) == 64u, "descriptor Phi: 64 distinct T#s were not given one image each");
    expectFailure([&] { static_cast<void>(planImages(distinctLoads(64u))); }, "GetImageResource dword 0 is not a valid runtime value", "descriptor Phi: more than 64 distinct T#s were accepted");

    const auto splitReloads = [](std::uint32_t joins) {
        std::vector<std::uint32_t> body;
        for (std::uint32_t join = 0; join < joins; join++) {
            body.insert(body.end(), {0xbf068014u, 0xbf850003u, 0xf40003c0u, join % 2u == 0u ? 0xfa000088u : 0xfa00008cu, 0xbf820006u, 0xf4080200u, 0xfa000060u, 0xf4040300u, 0xfa000070u, 0xf4000380u, 0xfa000078u});
        }
        return body;
    };
    require(planImages(splitReloads(8u)) == 4u, "descriptor Phi: separate reloads of T# dword 7 and dwords 0-6 were not split into their 4 T#s");
    expectFailure([&] { static_cast<void>(planImages(splitReloads(260u))); }, "GetImageResource dword 0 is not a valid runtime value", "descriptor Phi: a web over more than 512 descriptor tuples was accepted");
}

void verifyProgramCounterRelativeData() {
    using namespace ShaderRecompiler;
    static const std::array<std::uint32_t, 15> code{
        0xbe801f00u,
        0x800000ffu, 52u,
        0x82010180u,
        0xb0020010u,
        0xbe8303ffu, 0x10005004u,
        0xf4200100u, 0xfa000000u,
        0xbf8cc07fu,
        0x7e000204u,
        0xf80008cfu, 0u,
        0xbf810000u,
        0x3f800000u
    };
    const auto codeAddress = reinterpret_cast<std::uintptr_t>(code.data());
    RecompileRequest request{};
    request.shader = {ShaderStage::Vertex, codeAddress, code, 0, {}};
    request.target = BufferTarget();
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 8;
    request.context.vertex = ShaderVertexStageInfo{};
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = false;
    request.layout.pushConstantSizeBytes = 128;
    const auto dataBase = [](const RecompileResult& result) {
        for (const auto& binding : result.bindings) {
            if (binding.role != DescriptorRole::GuestBuffers || binding.guestDescriptor.size() < 4u) continue;
            return static_cast<std::uint64_t>(binding.guestDescriptor[0]) | (static_cast<std::uint64_t>(binding.guestDescriptor[1] & 0xffffu) << 32u);
        }
        throw std::runtime_error("program counter data: no guest buffer was bound");
    };
    AgcDriver::ShaderMemory memory({});
    static_cast<void>(memory.Capture(request));
    request.context.memory = memory.Regions();
    const auto first = Recompile(request);
    require(dataBase(first) == codeAddress + 56u, "program counter data: the V# does not name the data at the shader's address");
    auto relocated = request;
    relocated.shader.codeAddress += 0x1000u;
    const auto moved = Recompile(relocated);
    require(moved.cacheHit, "program counter data: relocating the shader recompiled it");
    require(dataBase(moved) == codeAddress + 0x1000u + 56u, "program counter data: the relocated shader bound the old address");
}

void verifyLanesOutsideHostSubgroup() {
    using namespace ShaderRecompiler;
    const auto pixel = [](std::span<const std::uint32_t> code, std::uint32_t subgroupSize) {
        ShaderPixelStageInfo info{};
        info.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
        info.hasPerspectiveCenterVgpr = true;
        info.targetOutputMode[0] = 9u;
        info.targetExportMapping.fill(0xe4u);
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.pixel = info;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return !Recompile(request).spirv.empty();
    };
    const auto compute = [](std::span<const std::uint32_t> code, std::uint32_t subgroupSize) {
        static constexpr std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return !Recompile(request).spirv.empty();
    };
    static constexpr std::array<std::uint32_t, 6> lane63{0xd7600006u, 0x00017f00u, 0x7e020206u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 6> lane31{0xd7600006u, 0x00013f00u, 0x7e020206u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 11> waterfall{0xbe80047eu, 0xbe821400u, 0xd7600003u, 0x00000500u, 0xbe801c02u, 0xbf138000u, 0xbf85fffau, 0x7e020203u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 6> computeLane63{0xd7600006u, 0x00017f00u, 0x7e020206u, 0xe0700000u, 0x80000100u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 5> permlanex16{0xd7780001u, 0x02010100u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 5> permlane16{0xd7770001u, 0x02010100u, 0xf800180fu, 0x01010101u, 0xbf810000u};
    expectFailure([&] { static_cast<void>(pixel(lane63, 32u)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup that runs this wave64 program at one lane per invocation", "host lanes: a wave64 pixel program read lane 63 of a 32-lane subgroup");
    expectFailure([&] { static_cast<void>(pixel(lane63, 8u)); }, "v_readlane_b32 of lane 63 is outside the 8-lane host subgroup", "host lanes: a wave64 pixel program read lane 63 of an 8-lane subgroup");
    require(pixel(lane31, 32u), "host lanes: a wave64 pixel program did not build reading lane 31 of a 32-lane subgroup");
    expectFailure([&] { static_cast<void>(pixel(lane31, 8u)); }, "v_readlane_b32 of lane 31 is outside the 8-lane host subgroup", "host lanes: a wave64 pixel program read lane 31 of an 8-lane subgroup");
    require(pixel(lane63, 64u), "host lanes: a wave64 pixel program did not build reading lane 63 of a 64-lane subgroup");
    require(compute(computeLane63, 32u), "host lanes: a wave64 compute program did not build reading lane 63 at two lanes per invocation");
    require(compute(computeLane63, 8u), "host lanes: a wave64 compute program did not build reading lane 63 of an 8-lane subgroup");
    require(pixel(waterfall, 32u) && pixel(waterfall, 8u), "host lanes: an s_ff1_i32_b64 waterfall reading a lane in a register did not build");
    expectFailure([&] { static_cast<void>(pixel(permlanex16, 8u)); }, "v_permlanex16_b32 from lanes 16-31 is outside the 8-lane host subgroup", "host lanes: v_permlanex16_b32 read lanes 16-31 of an 8-lane subgroup");
    require(pixel(permlanex16, 32u) && pixel(permlane16, 8u), "host lanes: a v_permlane16_b32 row that the host subgroup may hold did not build");
}

void verifyHalfWaveReduction() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::uint32_t, 4> capabilities{spv::CapabilityGroupNonUniform, spv::CapabilityGroupNonUniformBallot, spv::CapabilityGroupNonUniformShuffle, spv::CapabilityGroupNonUniformArithmetic};
    const auto pixel = [](std::span<const std::uint32_t> code, std::uint32_t subgroupSize, bool arithmetic) {
        ShaderPixelStageInfo info{};
        info.inputAddr = PixelInputBit(PixelInput::PositionX);
        info.posX = true;
        info.targetOutputMode[0] = 9u;
        info.targetExportMapping.fill(0xe4u);
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.pixel = info;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        if (arithmetic) request.target.supportedCapabilities = capabilities;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    const auto reductions = [](const std::vector<std::uint32_t>& spirv, spv::Op opcode) {
        std::uint32_t count = 0;
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            if ((spirv[cursor] & 0xffffu) == static_cast<std::uint32_t>(opcode) && (spirv[cursor] >> 16u) == 6u && spirv[cursor + 4u] == spv::GroupOperationReduce) ++count;
        }
        return count;
    };
    const auto halves = [](const std::vector<std::uint32_t>& spirv, spv::Op opcode, std::uint32_t identity) {
        std::map<std::uint32_t, std::size_t> definitions;
        std::vector<std::size_t> reduces;
        for (std::size_t cursor = 5; cursor < spirv.size() && (spirv[cursor] >> 16u) != 0u; cursor += spirv[cursor] >> 16u) {
            const auto op = spirv[cursor] & 0xffffu;
            if (op == spv::OpConstant || op == spv::OpSelect || op == spv::OpUGreaterThanEqual) definitions[spirv[cursor + 2u]] = cursor;
            if (op == static_cast<std::uint32_t>(opcode) && (spirv[cursor] >> 16u) == 6u && spirv[cursor + 4u] == spv::GroupOperationReduce) reduces.push_back(cursor);
        }
        const auto defined = [&](std::uint32_t id, spv::Op op) -> std::size_t {
            const auto found = definitions.find(id);
            return found != definitions.end() && (spirv[found->second] & 0xffffu) == static_cast<std::uint32_t>(op) ? found->second : 0u;
        };
        const auto constant = [&](std::uint32_t id, std::uint32_t value) {
            const auto at = defined(id, spv::OpConstant);
            return at != 0u && spirv[at + 3u] == value;
        };
        std::uint32_t lower = 0;
        std::uint32_t upper = 0;
        std::vector<std::uint32_t> keys;
        for (const auto reduce : reduces) {
            const auto select = defined(spirv[reduce + 5u], spv::OpSelect);
            const auto compare = select != 0u ? defined(spirv[select + 3u], spv::OpUGreaterThanEqual) : 0u;
            if (compare == 0u || !constant(spirv[compare + 4u], 32u)) continue;
            if (constant(spirv[select + 4u], identity)) {
                ++lower;
                keys.push_back(spirv[select + 5u]);
            } else if (constant(spirv[select + 5u], identity)) {
                ++upper;
                keys.push_back(spirv[select + 4u]);
            }
        }
        return reduces.size() == 2u && lower == 1u && upper == 1u && keys[0] == keys[1];
    };
    const auto scan = [](std::uint32_t identity, std::uint32_t vector, std::uint32_t scalar) {
        std::vector<std::uint32_t> code{0xbe98047eu, 0x7e160f00u, 0x8786187eu, 0xbeea287eu, 0xd501000du};
        if (identity == 0u || identity == 0xffffffffu) code.push_back(identity == 0u ? 0x001a1680u : 0x001a16c1u);
        else code.insert(code.end(), {0x001a16ffu, identity});
        for (const auto control : {0xff01110du, 0xff01120du, 0xff01140du, 0xff01180du}) code.insert(code.end(), {vector | 0x001a1afau, control});
        code.insert(code.end(), {0xd778100cu, 0x0305830du, vector | 0x001a190du, 0xbefe046au, 0xd7600006u, 0x00013f0du, 0xd7600007u, 0x00017f0du, scalar | 0x00080706u, 0x7e020208u, 0xf800180fu, 0x01010101u, 0xbf810000u});
        return code;
    };
    struct Reduction {
        std::uint32_t identity;
        std::uint32_t vector;
        std::uint32_t scalar;
        spv::Op reduce;
        const char* message;
    };
    static constexpr std::array<Reduction, 7> scans{{
        {0xffffffffu, 0x26000000u, 0x83800000u, spv::OpGroupNonUniformUMin, "half-wave reduction: the wave64 UMin scan did not read lane 31 as a subgroup UMin on 32 lanes"},
        {0u, 0x28000000u, 0x84800000u, spv::OpGroupNonUniformUMax, "half-wave reduction: the wave64 UMax scan did not read lane 31 as a subgroup UMax on 32 lanes"},
        {0x7fffffffu, 0x22000000u, 0x83000000u, spv::OpGroupNonUniformSMin, "half-wave reduction: the wave64 SMin scan did not read lane 31 as a subgroup SMin on 32 lanes"},
        {0x80000000u, 0x24000000u, 0x84000000u, spv::OpGroupNonUniformSMax, "half-wave reduction: the wave64 SMax scan did not read lane 31 as a subgroup SMax on 32 lanes"},
        {0u, 0x4a000000u, 0x80000000u, spv::OpGroupNonUniformIAdd, "half-wave reduction: the wave64 IAdd scan did not read lane 31 as a subgroup IAdd on 32 lanes"},
        {0xffffffffu, 0x36000000u, 0x87000000u, spv::OpGroupNonUniformBitwiseAnd, "half-wave reduction: the wave64 AND scan did not read lane 31 as a subgroup AND on 32 lanes"},
        {0u, 0x38000000u, 0x88000000u, spv::OpGroupNonUniformBitwiseOr, "half-wave reduction: the wave64 OR scan did not read lane 31 as a subgroup OR on 32 lanes"},
    }};
    for (const auto& reduction : scans) require(reductions(pixel(scan(reduction.identity, reduction.vector, reduction.scalar), 32u, true), reduction.reduce) == 1u, reduction.message);
    for (const auto& reduction : scans) {
        const auto wide = "half-wave reduction: a wave64 scan reduced by SPIR-V opcode " + std::to_string(reduction.reduce) + " did not read lanes 31 and 63 as reductions of host invocations 0-31 and 32-63 on 64 lanes";
        require(halves(pixel(scan(reduction.identity, reduction.vector, reduction.scalar), 64u, true), reduction.reduce, reduction.identity), wide.c_str());
    }
    const auto vertex = [](std::vector<std::uint32_t> code, std::uint32_t subgroupSize) {
        *std::find(code.begin(), code.end(), 0xf800180fu) = 0xf80008cfu;
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.supportedCapabilities = capabilities;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    require(reductions(vertex(scan(0xffffffffu, 0x26000000u, 0x83800000u), 32u), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: the wave64 vertex UMin scan, whose entry EXEC is every invocation, did not read lane 31 as a subgroup UMin on 32 lanes");
    require(halves(vertex(scan(0x80000000u, 0x24000000u, 0x84000000u), 64u), spv::OpGroupNonUniformSMax, 0x80000000u), "half-wave reduction: the wave64 vertex SMax scan did not read lanes 31 and 63 as reductions of host invocations 0-31 and 32-63 on 64 lanes");
    const auto floatScan = [&scan](std::uint32_t identity, std::uint32_t vector, std::uint32_t join) {
        auto code = scan(identity, vector, 0u);
        code[23] = join;
        code[24] = 0x00000e06u;
        return code;
    };
    const auto fmax = floatScan(0xff800000u, 0x20000000u, 0xd5100001u);
    const auto fmin = floatScan(0x7f800000u, 0x1e000000u, 0xd50f0001u);
    for (const auto& code : {fmax, fmin}) {
        const auto narrow = pixel(code, 32u, false);
        require(!narrow.empty() && reductions(narrow, spv::OpGroupNonUniformFMax) == 0u && reductions(narrow, spv::OpGroupNonUniformFMin) == 0u, "half-wave reduction: a wave64 v_max_f32/v_min_f32 scan did not read lane 63 as its -inf/+inf keys on 32 lanes");
        require(!pixel(code, 64u, false).empty(), "half-wave reduction: a wave64 v_max_f32/v_min_f32 scan did not build on 64 lanes");
    }
    auto fmaxUnderExec = fmax;
    fmaxUnderExec[3] = 0xbeea047eu;
    expectFailure([&] { static_cast<void>(pixel(fmaxUnderExec, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: a v_max_f32 scan under the entry EXEC read lane 63");
    auto fmaxJoinedByMin = fmax;
    fmaxJoinedByMin[17] = 0x1e000000u | 0x001a190du;
    expectFailure([&] { static_cast<void>(pixel(fmaxJoinedByMin, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: a v_max_f32 row scan joined by v_min_f32 read lane 63");
    auto fmaxOfZero = fmax;
    fmaxOfZero[6] = 0u;
    expectFailure([&] { static_cast<void>(pixel(fmaxOfZero, 32u, true)); }, "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup", "half-wave reduction: a v_max_f32 scan of 0 outside the live lanes read lane 63");
    const auto umin = scan(0xffffffffu, 0x26000000u, 0x83800000u);
    const auto patched = [&umin](std::initializer_list<std::pair<std::size_t, std::uint32_t>> words) {
        auto code = umin;
        for (const auto& [index, word] : words) code[index] = word;
        return code;
    };
    require(reductions(pixel(patched({{3, 0xbeea25c1u}}), 32u, true), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: the scan under s_or_saveexec_b64 -1 did not read lane 31 as a subgroup UMin");
    const char* lane63 = "v_readlane_b32 of lane 63 is outside the 32-lane host subgroup";
    auto divergent = umin;
    divergent.insert(divergent.begin() + 3, 0x88fe1a7eu);
    divergent.insert(divergent.begin() + 2, {0xbe860480u, 0x7d821688u, 0xbe9a246au, 0xbf880001u});
    require(reductions(pixel(divergent, 32u, true), spv::OpGroupNonUniformUMin) == 1u, "half-wave reduction: keys masked inside a divergent branch did not read lane 31 as a subgroup UMin");
    const char* arithmetic = "v_readlane_b32 of lane 31 of a wave64 half-wave reduction scan needs subgroup arithmetic";
    expectFailure([&] { static_cast<void>(pixel(umin, 32u, false)); }, arithmetic, "half-wave reduction: a device without subgroup arithmetic read lane 31 on 32 lanes");
    expectFailure([&] { static_cast<void>(pixel(umin, 64u, false)); }, arithmetic, "half-wave reduction: a device without subgroup arithmetic read lane 31 on 64 lanes");
    expectFailure([&] { static_cast<void>(pixel(umin, 128u, true)); }, "half-wave reduction scan on a 128-lane host subgroup, which is wider than the wave", "half-wave reduction: the scan built on a 128-lane subgroup");
    require(reductions(pixel(patched({{3, 0xbeea047eu}}), 64u, true), spv::OpGroupNonUniformUMin) == 0u, "half-wave reduction: a scan under the entry EXEC became a subgroup UMin on 64 lanes");
    expectFailure([&] { static_cast<void>(pixel(patched({{3, 0xbeea047eu}}), 32u, true)); }, lane63, "half-wave reduction: a scan under the entry EXEC read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{5, 0x001a1680u}}), 32u, true)); }, lane63, "half-wave reduction: a UMin scan of 0 outside the live lanes read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{0, 0xbe980a7eu}, {2, 0xbe860418u}}), 32u, true)); }, lane63, "half-wave reduction: a scan of keys under an s_wqm_b64 mask read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{7, 0xff09110du}}), 32u, true)); }, lane63, "half-wave reduction: a bound_ctrl row_shr:1 step read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{15, 0x0305010du}}), 32u, true)); }, lane63, "half-wave reduction: a v_permlanex16_b32 of lane 0 read lane 63");
    expectFailure([&] { static_cast<void>(pixel(patched({{16, 0x281a190du}}), 32u, true)); }, lane63, "half-wave reduction: a UMin row scan joined by v_max_u32 read lane 63");
    auto threeSteps = umin;
    threeSteps.erase(threeSteps.begin() + 12, threeSteps.begin() + 14);
    expectFailure([&] { static_cast<void>(pixel(threeSteps, 32u, true)); }, lane63, "half-wave reduction: a scan without row_shr:8 read lane 63");
    auto compared = umin;
    compared.erase(compared.begin() + 2);
    compared.insert(compared.begin() + 3, {0xd4c50006u, 0x00021680u});
    expectFailure([&] { static_cast<void>(pixel(compared, 32u, true)); }, lane63, "half-wave reduction: a scan selected by a compare under the all-lanes EXEC read lane 63");
    expectFailure([&] { static_cast<void>(pixel(umin, 8u, true)); }, "v_permlanex16_b32 from lanes 16-31 is outside the 8-lane host subgroup", "half-wave reduction: the scan built on an 8-lane subgroup");
}

void verifyMeshConfiguration() {
    using namespace ShaderRecompiler;
    ShaderMeshInputInfo list;
    list.inputPrimitive = 4u;
    require(list.InputPrimitiveSize() == 3u && list.InputPrimitiveStep() == 3u && list.InputVertexCount(21u) == 63u && list.InputPrimitiveCount(63u) == 21u && list.InputPrimitiveCount(2u) == 0u && list.InputVertexCount(0u) == 0u, "triangle list subgroup sizes changed");
    ShaderMeshInputInfo strip;
    strip.inputPrimitive = 6u;
    require(strip.InputPrimitiveSize() == 3u && strip.InputPrimitiveStep() == 1u && strip.InputVertexCount(21u) == 23u && strip.InputPrimitiveCount(23u) == 21u, "triangle strip subgroup sizes changed");
    ShaderMeshInputInfo fan;
    fan.inputPrimitive = 5u;
    require(fan.InputPrimitiveSize() == 3u && fan.InputPrimitiveStep() == 1u && fan.InputVertexCount(30u) == 32u && fan.InputPrimitiveCount(32u) == 30u && fan.InputPrimitiveCount(2u) == 0u, "triangle fan subgroup sizes changed");
    ShaderMeshInputInfo lines;
    lines.inputPrimitive = 2u;
    ShaderMeshInputInfo points;
    points.inputPrimitive = 1u;
    require(lines.InputPrimitiveSize() == 2u && lines.InputPrimitiveStep() == 2u && points.InputPrimitiveSize() == 1u && points.InputVertexCount(5u) == 5u, "line or point subgroup sizes changed");

    static constexpr std::array<std::uint32_t, 1> code{0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Mesh, 0x10000u, code, 0, {}};
    request.context.waveSize = 64;
    const MeshConfiguration mesh{4u, 21u, 63u, 64u, 21u, 64u, 256u, 0u, 12u};
    request.graphics = GraphicsCompileContext{0u, {}, mesh, std::nullopt, {}};
    const auto replay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(request));
    require(replay.request.graphics.has_value() && replay.request.graphics->mesh.has_value() && replay.request.graphics->mesh->esgsItemSize == 12u && replay.request.graphics->mesh->primitivesPerGroup == 21u, "mesh configuration was lost in serialization");
    std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    const auto first = key;
    auto other = request;
    auto otherMesh = mesh;
    otherMesh.esgsItemSize = 16u;
    other.graphics = GraphicsCompileContext{0u, {}, otherMesh, std::nullopt, {}};
    RecompileCacheKey::Build(other, key);
    require(key != first && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(other), "the cache keys ignore the mesh configuration");
}

ShaderRecompiler::ShaderPixelStageInfo twoParameterPixel() {
    ShaderRecompiler::ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = 2u;
    pixel.interpolatorSettings[1] = 1u;
    pixel.wave32 = true;
    pixel.inputAddr = ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::PerspectiveCenter) | ShaderRecompiler::PixelInputBit(ShaderRecompiler::PixelInput::LinearCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.noPerspective = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping[0] = 0xe4u;
    return pixel;
}

std::string requestPrefix(std::string_view text, std::size_t bytes) {
    constexpr std::string_view alphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string result(text.substr(0, ((bytes + 2u) / 3u) * 4u));
    if (bytes % 3u == 1u) {
        result[result.size() - 3u] = alphabet[alphabet.find(result[result.size() - 3u]) & 0x30u];
        result[result.size() - 2u] = '=';
        result.back() = '=';
    } else if (bytes % 3u == 2u) {
        result[result.size() - 2u] = alphabet[alphabet.find(result[result.size() - 2u]) & 0x3cu];
        result.back() = '=';
    }
    return result;
}

void verifyPixelRequestSerialization() {
    using namespace ShaderRecompiler;
    const auto fields = [](const ShaderPixelStageInfo& value) {
        return std::tie(value.interpolatorCount, value.interpolatorSettings, value.wave32, value.inputAddr,
                        value.hasPerspectiveCenterVgpr, value.perspectiveCentroid, value.posX, value.posY,
                        value.posZ, value.posW, value.frontFace, value.ancillary, value.sampleShading,
                        value.noPerspective, value.linearCentroid, value.pixelKillEnable, value.depthExportEnable,
                        value.sampleMaskExportEnable, value.earlyZ, value.executeOnNoop, value.conservativeZExport, value.orderedPixelShader,
                        value.targetOutputMode, value.targetExportMapping);
    };
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    const std::array<std::uint32_t, 3> userData{0x12345678u, 0xabcdef01u, 0x87654321u};
    const std::array<MemoryRegion, 1> memory{{{0x60000u, std::as_bytes(std::span(userData))}}};
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userData = userData;
    request.context.memory = memory;
    request.context.vertex = ShaderVertexStageInfo{};
    request.context.vertex->fetchAttribReg = 17u;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.nonConstantImageOffsets = true;
    request.layout = {0u, 11u, 16u, 128u};
    request.useCache = false;
    ShaderPixelStageInfo pixel{
        .interpolatorCount = 32u,
        .wave32 = true,
        .inputAddr = 0x7fffu,
        .hasPerspectiveCenterVgpr = true,
        .perspectiveCentroid = true,
        .posX = true,
        .posY = true,
        .posZ = true,
        .posW = true,
        .frontFace = true,
        .ancillary = true,
        .sampleShading = true,
        .noPerspective = true,
        .linearCentroid = true,
        .pixelKillEnable = true,
        .depthExportEnable = true,
        .sampleMaskExportEnable = true,
        .earlyZ = true,
        .executeOnNoop = true,
        .conservativeZExport = ConservativeZExport::GreaterThanZ,
        .orderedPixelShader = true,
        .targetOutputMode = {1u, 2u, 3u, 4u, 5u, 6u, 7u, 8u}
    };
    for (std::uint32_t i = 0; i < pixel.interpolatorSettings.size(); ++i) pixel.interpolatorSettings[i] = 0x10101010u + i;
    const std::array<std::array<std::uint8_t, 8>, 3> mappings{{
        {0x00u, 0xe4u, 0xc6u, 0x1bu, 0xffu, 0x80u, 0x55u, 0xaau},
        {0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u, 0xe4u},
        {0xc6u, 0x1bu, 0x00u, 0xffu, 0x55u, 0xaau, 0x80u, 0x39u}
    }};
    const RequestSerializer serializer;
    for (const auto& mapping : mappings) {
        pixel.targetExportMapping = mapping;
        request.context.pixel = pixel;
        const auto replay = serializer.Deserialize(serializer.Serialize(request));
        require(replay.request.context.pixel.has_value() && fields(*replay.request.context.pixel) == fields(pixel), "pixel fields or export mappings were lost in serialization");
        require(replay.request.context.vertex.has_value() && replay.request.context.vertex->fetchAttribReg == 17u && replay.request.context.memory.size() == 1u && replay.request.context.memory[0].guestAddress == 0x60000u && replay.request.context.memory[0].bytes.size() == sizeof(userData), "pixel mappings displaced the following guest context");
        require(replay.request.target.subgroupSize == 64u && replay.request.target.nonConstantImageOffsets && replay.request.layout.firstBinding == 11u && replay.request.layout.pushConstantOffsetBytes == 16u && !replay.request.useCache, "pixel mappings displaced the following request fields");
        std::vector<std::uint64_t> key;
        RecompileCacheKey::Build(request, key);
        std::vector<std::uint64_t> replayKey;
        RecompileCacheKey::Build(replay.request, replayKey);
        require(key == replayKey && RecompileCacheKey::ContextHash(request) == RecompileCacheKey::ContextHash(replay.request), "pixel replay changed shader identity");
        for (std::size_t i = 0; i < mapping.size(); ++i) {
            auto changed = request;
            changed.context.pixel->targetExportMapping[i] ^= 1u;
            RecompileCacheKey::Build(changed, replayKey);
            require(key == replayKey && RecompileCacheKey::ContextHash(request) == RecompileCacheKey::ContextHash(changed), "runtime pixel export mapping changed the static shader identity");
        }
    }
    request.context.pixel.reset();
    request.shader.stage = ShaderStage::Vertex;
    const auto withoutPixel = serializer.Deserialize(serializer.Serialize(request));
    require(!withoutPixel.request.context.pixel.has_value() && withoutPixel.request.context.vertex.has_value() && withoutPixel.request.context.vertex->fetchAttribReg == 17u && withoutPixel.request.context.memory.size() == 1u && withoutPixel.request.context.memory[0].guestAddress == 0x60000u && withoutPixel.request.target.nonConstantImageOffsets && withoutPixel.request.layout.firstBinding == 11u && !withoutPixel.request.useCache, "a request without pixel state was misaligned");

    RecompileRequest minimal{};
    minimal.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    minimal.context.waveSize = 64;
    minimal.context.pixel = ShaderPixelStageInfo{};
    const auto encoded = serializer.Serialize(minimal);
    require(requestPrefix(encoded, 8u) == "NVNQQQ0AAAA=", "new requests did not use serialization version 13");
    constexpr std::size_t mappingOffset = 8u + 37u + 18u + 163u;
    for (std::size_t bytes = 0; bytes < 8u; ++bytes) {
        expectFailure([&] { static_cast<void>(serializer.Deserialize(requestPrefix(encoded, mappingOffset + bytes))); }, "truncated data", "a truncated version-12 pixel mapping was accepted");
    }
    for (const auto unsupported : {"NVNQQQAAAAA=", "NVNQQQ4AAAA="}) {
        expectFailure([&] { static_cast<void>(serializer.Deserialize(unsupported)); }, "serialization version", "an unsupported request version was accepted");
    }
}

void verifyLegacyPixelRequests() {
    using namespace ShaderRecompiler;
    static constexpr std::array<std::string_view, 10> legacyPixelRequests{
        "NVNQQQEAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAA",
        "NVNQQQIAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAAAA==",
        "NVNQQQMAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAAAA==",
        "NVNQQQQAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAAAAkAAAAAAAAAAAAA"
        "AAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAAAIAA"
        "AAAAAA==",
        "NVNQQQUAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAACQAAAAAAAAAA"
        "AAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAsAAAAAAAAA"
        "gAAAAAAA",
        "NVNQQQYAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAACQAAAAAAAAAA"
        "AAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAsAAAAAAAAA"
        "gAAAAAAAAQ==",
        "NVNQQQcAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAABBAAAADAQBAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAALAAAAAAAA"
        "AIAAAAAAAAE=",
        "NVNQQQgAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAsAAAAAAAAAgAAAAAAAAQ==",
        "NVNQQQkAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAsAAAAAAAAAgAAAAAAAAQAAAAA=",
        "NVNQQQoAAAAFAAADAAAAAAABAAAAAAAAAAAAgb8AAAAAAAAAAAAAAAAAAAAAQAAAAAAAAAAAAAAAAAAAAAABAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAIAAAABAAAAAAAAAAAAAAAAAAAAAgkAAAAAAAAA"
        "AAAAAAAAAAAAAAAAAAAAAAAAEEAAAAMBAEAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA"
        "AAsAAAAAAAAAgAAAAAAAAQAAAAA=",
    };
    const RequestSerializer serializer;
    for (std::size_t index = 0; index < legacyPixelRequests.size(); ++index) {
        const auto version = index + 1u;
        const auto replay = serializer.Deserialize(legacyPixelRequests[index]);
        const auto& request = replay.request;
        require(request.context.pixel.has_value(), "legacy pixel state was lost");
        const auto& pixel = *request.context.pixel;
        require(pixel.targetExportMapping == std::array<std::uint8_t, 8>{}, "legacy pixel mapping no longer defaults to zero");
        require(pixel.inputAddr == 2u && pixel.hasPerspectiveCenterVgpr && pixel.targetOutputMode[0] == 9u, "legacy pixel layout was misread");
        require(pixel.conservativeZExport == (version >= 7u ? ConservativeZExport::GreaterThanZ : ConservativeZExport::AnyZ), "legacy conservative Z layout was misread");
        require(!pixel.orderedPixelShader, "a legacy request became a primitive-ordered pixel shader");
        require(request.shader.stage == ShaderStage::Fragment && request.shader.code.size() == 1u && request.shader.code[0] == 0xbf810000u && !request.context.vertex.has_value() && request.context.memory.empty(), "legacy guest context was misaligned");
        require(request.target.vulkanVersion == 0x00401000u && request.target.spirvVersion == 0x00010300u && request.target.subgroupSize == 64u && request.layout.firstBinding == 11u && request.layout.pushConstantSizeBytes == 128u, "legacy target or binding layout was misaligned");
        require(request.useCache == (version == 1u) && request.target.nonConstantImageOffsets == (version >= 6u) && request.target.srgbDecodeFormats == 0u && !request.target.narrowSubgroupClock, "legacy request trailer was misread");
        const auto upgraded = serializer.Deserialize(serializer.Serialize(request));
        require(upgraded.request.context.pixel->targetExportMapping == pixel.targetExportMapping && RecompileCacheKey::ContextHash(upgraded.request) == RecompileCacheKey::ContextHash(request), "upgrading a legacy capture changed its pixel mapping");
    }
}

void verifyPixelExportReplay() {
    using namespace ShaderRecompiler;
    std::array<std::uint32_t, 11> code{
        0x7e0002ffu, 0x3e800000u, 0x7e0202ffu, 0x3f000000u,
        0x7e0402ffu, 0x3f400000u, 0x7e0602ffu, 0x3f800000u,
        0xf800180fu, 0x03020100u, 0xbf810000u
    };
    const RequestSerializer serializer;
    for (const auto target : {0u, 7u}) {
        code[8] = 0xf800180fu | (target << 4u);
        ShaderPixelStageInfo pixel{};
        pixel.targetOutputMode[target] = 9u;
        pixel.targetExportMapping.fill(0xe4u);
        pixel.targetExportMapping[target] = 0xc6u;
        RecompileRequest request{};
        request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
        request.context.waveSize = 64u;
        request.context.pixel = pixel;
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64u;
        request.layout.pushConstantSizeBytes = 128u;
        request.useCache = false;
        const auto replay = serializer.Deserialize(serializer.Serialize(request));
        const auto original = Recompile(request);
        auto identity = request;
        identity.context.pixel->targetExportMapping[target] = 0xe4u;
        require(original.spirv != Recompile(identity).spirv, "the non-identity pixel export mapping did not affect the compiled shader");
        require(replay.request.context.pixel.has_value() && replay.request.context.pixel->targetExportMapping == pixel.targetExportMapping, "replay lost the non-identity pixel export mapping");
        const auto replayed = Recompile(replay.request);
        verifyResult(original, replayed);
        request.useCache = true;
        const auto cached = Recompile(request);
        const auto cachedReplay = serializer.Deserialize(serializer.Serialize(request));
        const auto hit = Recompile(cachedReplay.request);
        require(hit.cacheHit && hit.variantId == cached.variantId, "pixel replay did not reuse the original shader variant");
        verifyResult(cached, hit);
    }
}

std::vector<std::uint32_t> noPerspectiveLocations(std::span<const std::uint32_t> code) {
    using namespace ShaderRecompiler;
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = twoParameterPixel();
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    const auto result = Recompile(request);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, std::uint32_t> locations;
    std::vector<std::uint32_t> decorated;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        if (static_cast<spv::Op>(words[at] & 0xffffu) != spv::OpDecorate) continue;
        if (words[at + 2] == spv::DecorationLocation) locations[words[at + 1]] = words[at + 3];
        if (words[at + 2] == spv::DecorationNoPerspective) decorated.push_back(words[at + 1]);
    }
    std::vector<std::uint32_t> result2;
    for (const auto id : decorated) result2.push_back(locations.count(id) != 0 ? locations.at(id) : 0xffffffffu);
    return result2;
}

void verifyPixelInputs() {
    using namespace ShaderRecompiler;
    require(PixelInputVgpr(0x326u, PixelInput::PerspectiveCentroid) == 2u && PixelInputVgpr(0x326u, PixelInput::LinearCenter) == 4u && PixelInputVgpr(0x326u, PixelInput::PositionX) == 6u, "the SPI_PS_INPUT_ADDR layout moved the inputs");
    require(PixelInputVgpr(0x7afu, PixelInput::PerspectiveCentroid) == 4u && PixelInputVgpr(0x7afu, PixelInput::PositionX) == 12u && PixelInputVgpr(0x7afu, PixelInput::PositionZ) == 14u, "ADDR-only inputs did not reserve their VGPRs");

    static constexpr std::array<std::uint32_t, 7> byPair{0xc8100000u, 0xc8110001u, 0xc8140402u, 0xc8150403u, 0xf800180fu, 0x05040504u, 0xbf810000u};
    const auto linear = noPerspectiveLocations(byPair);
    require(linear.size() == 1u && linear[0] == 1u, "only the parameter interpolated through the linear pair must be NoPerspective");
    static constexpr std::array<std::uint32_t, 7> bothPairs{0xc8100000u, 0xc8110001u, 0xc8140002u, 0xc8150003u, 0xf800180fu, 0x05040504u, 0xbf810000u};
    expectFailure([&] { static_cast<void>(noPerspectiveLocations(bothPairs)); }, "interpolated through both a perspective and a linear I/J pair", "a parameter read through both pairs was given one interpolation");

    static constexpr std::array<std::uint32_t, 1> code{0xbf810000u};
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    auto pixel = twoParameterPixel();
    pixel.inputAddr |= PixelInputBit(PixelInput::PerspectiveCentroid) | PixelInputBit(PixelInput::LinearCentroid);
    pixel.perspectiveCentroid = true;
    pixel.linearCentroid = true;
    request.context.pixel = pixel;
    const auto replay = RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(request));
    const auto& back = *replay.request.context.pixel;
    require(back.inputAddr == pixel.inputAddr && back.perspectiveCentroid && back.linearCentroid && back.noPerspective, "the pixel input layout did not survive serialization");
    std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    const auto first = key;
    for (const auto change : {0, 1, 2}) {
        auto other = request;
        auto changed = pixel;
        if (change == 0) changed.inputAddr |= PixelInputBit(PixelInput::PerspectiveSample);
        if (change == 1) changed.perspectiveCentroid = false;
        if (change == 2) changed.linearCentroid = false;
        other.context.pixel = changed;
        RecompileCacheKey::Build(other, key);
        require(key != first && RecompileCacheKey::ContextHash(request) != RecompileCacheKey::ContextHash(other), "the cache keys ignore the pixel input layout");
    }
}

ShaderRecompiler::RecompileResult recompileSlots(std::initializer_list<std::uint32_t> controls, std::span<const std::uint32_t> code) {
    using namespace ShaderRecompiler;
    ShaderPixelStageInfo pixel{};
    pixel.interpolatorCount = static_cast<std::uint32_t>(controls.size());
    std::uint32_t index = 0;
    for (const auto control : controls) pixel.interpolatorSettings[index++] = control;
    pixel.inputAddr = PixelInputBit(PixelInput::PerspectiveCenter);
    pixel.hasPerspectiveCenterVgpr = true;
    pixel.targetOutputMode[0] = 9u;
    pixel.targetExportMapping[0] = 0xe4u;
    RecompileRequest request{};
    request.shader = {ShaderStage::Fragment, 0x30000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.pixel = pixel;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 64;
    request.target.fragmentShaderBarycentricEnabled = true;
    static constexpr std::array<std::uint32_t, 1> capabilities{spv::CapabilityFloat64};
    request.target.supportedCapabilities = capabilities;
    request.layout.pushConstantSizeBytes = 128;
    request.useCache = false;
    return Recompile(request);
}

std::vector<std::pair<std::uint32_t, bool>> slotInputs(std::initializer_list<std::uint32_t> controls, std::span<const std::uint32_t> code) {
    const auto result = recompileSlots(controls, code);
    const auto& words = result.spirv.Words();
    std::map<std::uint32_t, std::uint32_t> locations;
    std::map<std::uint32_t, bool> perVertex;
    std::vector<std::uint32_t> inputs;
    for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) {
        const auto op = static_cast<spv::Op>(words[at] & 0xffffu);
        if (op == spv::OpVariable && words[at + 3] == spv::StorageClassInput) inputs.push_back(words[at + 2]);
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationLocation) locations[words[at + 1]] = words[at + 3];
        if (op == spv::OpDecorate && words[at + 2] == spv::DecorationPerVertexKHR) perVertex[words[at + 1]] = true;
    }
    std::vector<std::pair<std::uint32_t, bool>> located;
    for (const auto id : inputs) {
        if (locations.contains(id)) located.emplace_back(locations.at(id), perVertex.contains(id));
    }
    std::sort(located.begin(), located.end());
    return located;
}

void verifyPixelParameterSlots() {
    static constexpr std::array<std::uint32_t, 7> shared{0xc8100000u, 0xc8110001u, 0xc8140500u, 0xc8150501u, 0xf800180fu, 0x05040504u, 0xbf810000u};
    auto inputs = slotInputs({0x3u, 0x3u}, shared);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "inputs reading one slot were not declared once at the slot");
    inputs = slotInputs({0x404u, 0x0u}, shared);
    require(inputs.size() == 2u && inputs[0].first == 0u && inputs[1].first == 4u, "inputs of different slots moved");
    require(slotInputs({0x20u, 0x2320u}, shared).empty(), "a defaulted input was declared as a parameter");
    static constexpr std::array<std::uint32_t, 8> mixed{0xc8100000u, 0xc8110001u, 0xc8160402u, 0xc81a0802u, 0xc81e0f02u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    inputs = slotInputs({0x0u, 0x400u, 0x22u, 0x320u}, mixed);
    require(inputs.size() == 1u && inputs[0].first == 0u && inputs[0].second, "a slot read flat and interpolated did not become one per-vertex input");
    static constexpr std::array<std::uint32_t, 7> vertices{0xc8120002u, 0xc8160000u, 0xc81a0001u, 0xc81e0302u, 0xf800180fu, 0x07060504u, 0xbf810000u};
    const auto subtracts = [](std::initializer_list<std::uint32_t> controls) {
        const auto result = recompileSlots(controls, vertices);
        const auto& words = result.spirv.Words();
        std::size_t count = 0;
        for (std::size_t at = 5; at < words.size() && (words[at] >> 16u) != 0; at += words[at] >> 16u) count += (words[at] & 0xffffu) == spv::OpFSub;
        return count;
    };
    inputs = slotInputs({0x423u}, vertices);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "a pass-through input (OFFSET bit 5 with FLAT_SHADE) was not read per vertex at its slot");
    require(subtracts({0x423u}) == 0u, "v_interp_mov p10/p20 of a pass-through input subtracted vertex 0");
    inputs = slotInputs({0x403u}, vertices);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second && subtracts({0x403u}) == 2u, "v_interp_mov p10/p20 of a flat input did not read differences to vertex 0");
    require(slotInputs({0x23u}, vertices).empty(), "a defaulted input (OFFSET bit 5 without FLAT_SHADE) was declared as a parameter");
    expectFailure([] { static_cast<void>(recompileSlots({0x423u, 0x3u}, shared)); }, "passes its vertices through unchanged", "an interpolated pass-through input was accepted");
}

void verifyF16PixelParameterSlots() {
    static constexpr std::array<std::uint32_t, 7> high{0xd7420002u, 0x00020100u, 0xd75a0003u, 0x040a0300u, 0xf800180fu, 0x03030303u, 0xbf810000u};
    static constexpr std::array<std::uint32_t, 7> low{0xd7420002u, 0x00020000u, 0xd75a0003u, 0x040a0200u, 0xf800180fu, 0x03030303u, 0xbf810000u};
    auto inputs = slotInputs({0x03080003u}, high);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "a 16-bit interpolated input was not read per vertex at its slot");
    inputs = slotInputs({0x03080023u}, high);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "an input with a defaulted low half was not read per vertex for its high half");
    require(slotInputs({0x03180023u}, high).empty(), "an input whose high half is defaulted was declared for a high-half read");
    require(slotInputs({0x03080023u}, low).empty(), "an input whose low half is defaulted was declared for a low-half read");
    inputs = slotInputs({0x03180003u}, low);
    require(inputs.size() == 1u && inputs[0].first == 3u && inputs[0].second, "an input with a defaulted high half was not read per vertex for its low half");
}

}

void verifyComputedTexelOffsets() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Type2D = 9;
    struct alignas(256) Texture { std::array<std::uint8_t, 256> bytes{}; };
    static Texture texture;
    static std::array<std::uint32_t, 64> output{};
    const auto textureBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 16> userData{
        static_cast<std::uint32_t>(textureBase >> 8u), static_cast<std::uint32_t>((textureBase >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u,
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    const auto program = [](std::uint32_t offsetSource, std::uint32_t literal) {
        std::vector<std::uint32_t> code{0x7e020200u | offsetSource};
        if (offsetSource == 0xffu) code.push_back(literal);
        code.insert(code.end(), {0x7e040280u, 0x7e060280u, 0xf0dc0f08u, 0x00400401u, 0xe0700000u, 0x80030400u, 0xbf810000u});
        return code;
    };
    const auto computed = program(0x100u, 0u);
    const auto constant = program(0xffu, 0x3fu | (1u << 8u));
    const auto recompile = [&](const std::vector<std::uint32_t>& code, bool offsets) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x30000u, code, 0, {}};
        request.target = BufferTarget();
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.target.nonConstantImageOffsets = offsets;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(request));
        request.context.memory = memory.Regions();
        return Recompile(request).spirv;
    };
    const auto sampleOperands = [](const std::vector<std::uint32_t>& words) {
        std::uint32_t mask = 0;
        bool gatherExtended = false;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "texel offsets: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            if (op == spv::OpCapability && words[cursor + 1] == spv::CapabilityImageGatherExtended) gatherExtended = true;
            if (op == spv::OpImageSampleExplicitLod && count > 5u) mask |= words[cursor + 5];
            cursor += count;
        }
        return std::pair{mask, gatherExtended};
    };
    expectFailure([&] { static_cast<void>(recompile(computed, false)); }, "texel offset that is not a constant", "texel offsets: a computed offset compiled without maintenance8");
    const auto [computedMask, computedGather] = sampleOperands(recompile(computed, true));
    require((computedMask & spv::ImageOperandsOffsetMask) != 0u && (computedMask & spv::ImageOperandsConstOffsetMask) == 0u && computedGather, "texel offsets: a computed offset is not an Offset operand");
    for (const bool offsets : {false, true}) {
        const auto [constantMask, constantGather] = sampleOperands(recompile(constant, offsets));
        require((constantMask & spv::ImageOperandsConstOffsetMask) != 0u && (constantMask & spv::ImageOperandsOffsetMask) == 0u && !constantGather, "texel offsets: a constant offset is not a ConstOffset operand");
    }
}

void verifyShaderClockScopes() {
    using namespace ShaderRecompiler;
    static std::array<std::uint32_t, 64> output{};
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 4> userData{static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    const std::array<std::uint32_t, 2> capabilities{1u, static_cast<std::uint32_t>(spv::CapabilityShaderClockKHR)};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_shader_clock"};
    const auto scope = [&](std::uint32_t clockOpcode, bool narrow) {
        const std::vector<std::uint32_t> code{clockOpcode, 0x00000000u, 0xbf8cc07fu, 0x7e020204u, 0xe0700000u, 0x80000100u, 0xbf810000u};
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x30000u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.narrowSubgroupClock = narrow;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(request));
        request.context.memory = memory.Regions();
        const auto words = Recompile(request).spirv;
        std::map<std::uint32_t, std::uint32_t> constants;
        std::vector<std::uint32_t> scopeIds;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "shader clock: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            if (op == spv::OpConstant && count == 4u) constants[words[cursor + 2]] = words[cursor + 3];
            if (op == spv::OpReadClockKHR) scopeIds.push_back(words[cursor + 3]);
            cursor += count;
        }
        require(scopeIds.size() == 1u && constants.contains(scopeIds[0]), "shader clock: expected one OpReadClockKHR with a constant scope");
        return constants[scopeIds[0]];
    };
    constexpr std::uint32_t Memtime = 0xf4900100u;
    constexpr std::uint32_t Memrealtime = 0xf4940100u;
    require(scope(Memtime, false) == spv::ScopeSubgroup, "shader clock: s_memtime does not read the subgroup clock");
    require(scope(Memtime, true) == spv::ScopeDevice, "shader clock: s_memtime reads the narrow subgroup clock");
    require(scope(Memrealtime, false) == spv::ScopeDevice && scope(Memrealtime, true) == spv::ScopeDevice, "shader clock: s_memrealtime does not read the device clock");
}

void verifyInt64AtomicCapabilities() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format32_32UInt = 62;
    constexpr std::uint32_t Type2D = 9;
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    alignas(256) static std::array<std::uint32_t, 64> buffer{};
    alignas(4096) static std::array<std::uint32_t, 512> texels{};
    const auto bufferAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(buffer.data()));
    const auto texelAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texels.data()));
    const std::array<std::uint32_t, 16> userData{
        0u, 0u, 0u, 0u,
        static_cast<std::uint32_t>(bufferAddress), static_cast<std::uint32_t>((bufferAddress >> 32u) & 0xffffu), 256u, 0x01016facu,
        static_cast<std::uint32_t>(texelAddress >> 8u), static_cast<std::uint32_t>((texelAddress >> 40u) & 0xffu) | (Format32_32UInt << 20u) | (3u << 30u), 7u | (7u << 14u), 0xfacu | (Type2D << 28u),
        0u, 0u, 0u, 0u};
    const std::vector<std::uint32_t> bufferAtomic{0xe1705000u, 0x80012803u, 0xbf810000u};
    const std::vector<std::uint32_t> imageAtomic{0xf03c0308u, 0x00020a08u, 0xbf810000u};
    const auto compile = [&](const std::vector<std::uint32_t>& code, spv::Capability added) {
        const std::array<std::uint32_t, 4> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, static_cast<std::uint32_t>(added)};
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x58000u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    const auto declares = [](const std::vector<std::uint32_t>& words, spv::Capability capability) {
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "64-bit atomics: truncated SPIR-V instruction");
            if ((words[cursor] & 0xffffu) == spv::OpCapability && words[cursor + 1] == static_cast<std::uint32_t>(capability)) return true;
            cursor += count;
        }
        return false;
    };
    expectFailure([&] { static_cast<void>(compile(bufferAtomic, spv::CapabilityShader)); }, "64-bit buffer atomics need shaderBufferInt64Atomics", "64-bit atomics: a buffer_atomic_inc_x2 compiled without shaderBufferInt64Atomics");
    require(declares(compile(bufferAtomic, spv::CapabilityInt64Atomics), spv::CapabilityInt64Atomics), "64-bit atomics: a buffer_atomic_inc_x2 does not declare Int64Atomics");
    expectFailure([&] { static_cast<void>(compile(imageAtomic, spv::CapabilityShader)); }, "64-bit image atomics need VK_EXT_shader_image_atomic_int64", "64-bit atomics: an image_atomic_swap on a 32_32 image compiled without shaderImageInt64Atomics");
    require(declares(compile(imageAtomic, spv::CapabilityInt64ImageEXT), spv::CapabilityInt64ImageEXT), "64-bit atomics: an image_atomic_swap on a 32_32 image does not declare Int64ImageEXT");
}

void verifyUnnormalizedSamplers() {
    using namespace ShaderRecompiler;
    constexpr std::uint32_t Format8888UNorm = 56;
    constexpr std::uint32_t Format11_11_10UInt = 34;
    constexpr std::uint32_t Format32Float = 22;
    constexpr std::uint32_t Type2D = 9;
    constexpr std::uint32_t Type3D = 10;
    constexpr std::uint32_t TypeCube = 11;
    constexpr std::uint32_t Type2DArray = 13;
    struct alignas(256) Texture { std::array<std::uint8_t, 4096> bytes{}; };
    static Texture texture;
    static std::array<std::uint32_t, 64> output{};
    const auto textureBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 4> unnormalized{0x00008092u, 0x00fff000u, 0x05500000u, 0u};
    const std::array<std::uint32_t, 4> normalized{0x00000092u, 0x00fff000u, 0x05500000u, 0u};
    const auto imageData = [&](const std::array<std::uint32_t, 4>& sampler, std::uint32_t type, std::uint32_t format, std::uint32_t depth) {
        return std::array<std::uint32_t, 16>{
            static_cast<std::uint32_t>(textureBase >> 8u), static_cast<std::uint32_t>((textureBase >> 40u) & 0xffu) | (format << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (type << 28u), depth, 0u, 0u, 0u,
            sampler[0], sampler[1], sampler[2], sampler[3],
            static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    };
    const auto userData = [&](const std::array<std::uint32_t, 4>& sampler) {
        return imageData(sampler, Type2D, Format8888UNorm, 0u);
    };
    const auto program = [](std::uint32_t mimg) {
        return std::vector<std::uint32_t>{0x7e020280u, 0x7e040280u, 0x7e060280u, 0x7e080280u, 0x7e0a0280u, 0x7e0c0280u, mimg, 0x00400801u, 0xe0700000u, 0x80030800u, 0xbf810000u};
    };
    const std::array<std::uint32_t, 3> capabilities{1u, static_cast<std::uint32_t>(spv::CapabilityImageGatherExtended), static_cast<std::uint32_t>(spv::CapabilityMinLod)};
    std::uint64_t nextAddress = 0x40000u;
    const auto recompile = [&](const std::vector<std::uint32_t>& code, const std::array<std::uint32_t, 16>& data, std::uint64_t address = 0u) {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, address != 0u ? address : (nextAddress += 0x1000u), code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = data;
        request.context.compute = ShaderComputeStageInfo{{16u, 2u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.supportedCapabilities = capabilities;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        AgcDriver::ShaderMemory memory({});
        static_cast<void>(memory.Capture(request));
        request.context.memory = memory.Regions();
        return Recompile(request);
    };
    const auto flags = [](const RecompileResult& result, DescriptorRole role) {
        for (const auto& binding : result.bindings) {
            if (binding.role == role) return role == DescriptorRole::GuestSamplers ? binding.samplerUnnormalized : binding.imageUnnormalized;
        }
        throw std::runtime_error("unnormalized samplers: the program has no sampler or image binding");
    };
    const auto proven = [&](const RecompileResult& result, bool expected) {
        return flags(result, DescriptorRole::GuestSamplers) == std::vector<bool>{expected} && flags(result, DescriptorRole::GuestImages) == std::vector<bool>{expected};
    };
    const auto auditSpirv = [](const std::vector<std::uint32_t>& words) {
        std::size_t samples = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto count = words[cursor] >> 16u;
            require(count != 0 && count <= words.size() - cursor, "unnormalized samplers: truncated SPIR-V instruction");
            const auto op = words[cursor] & 0xffffu;
            require(op != spv::OpImageSampleImplicitLod && op != spv::OpImageSampleDrefImplicitLod && op != spv::OpImageSampleDrefExplicitLod && op != spv::OpImageGather && op != spv::OpImageDrefGather && op != spv::OpImageQueryLod, "unnormalized samplers: the SPIR-V samples in a form an unnormalized sampler does not allow");
            if (op == spv::OpImageSampleExplicitLod) {
                require(count == 7u && words[cursor + 5] == spv::ImageOperandsLodMask, "unnormalized samplers: an explicit-LOD sample has operands other than Lod");
                ++samples;
            }
            cursor += count;
        }
        require(samples != 0u, "unnormalized samplers: the SPIR-V does not sample");
    };

    const auto lz = program(0xf09c0f08u);
    const auto accepted = recompile(lz, userData(unnormalized), 0x40000u);
    require(proven(accepted, true), "unnormalized samplers: image_sample_lz through an unnormalized S# was not flagged");
    auditSpirv(accepted.spirv.Words());
    const auto plain = recompile(lz, userData(normalized), 0x40000u);
    require(proven(plain, false), "unnormalized samplers: a normalized S# was flagged");
    require(plain.variantId == accepted.variantId && plain.spirv.Words() == accepted.spirv.Words(), "unnormalized samplers: the unnormalized S# compiled another variant");

    for (const std::uint32_t mimg : {0xf0900f08u, 0xf0800f08u, 0xf0940f08u, 0xf0840f08u}) {
        const auto result = recompile(program(mimg), userData(unnormalized));
        require(proven(result, true), "unnormalized samplers: a compute image_sample_l, image_sample, image_sample_b or image_sample_cl was not flagged");
        auditSpirv(result.spirv.Words());
    }

    const auto reject = [&](std::uint32_t mimg, const std::array<std::uint32_t, 16>& data, const char* reason, const char* what) {
        expectFailure([&] { static_cast<void>(recompile(program(mimg), data)); }, reason, what);
    };
    reject(0xf0c00f08u, userData(unnormalized), "unnormalized guest sampler is used with a texel offset, which is not implemented", "unnormalized samplers: image_sample_o was accepted");
    reject(0xf0bc0f08u, imageData(unnormalized, Type2D, Format32Float, 0u), "unnormalized guest sampler is used with depth comparison, which is not implemented", "unnormalized samplers: image_sample_c_lz was accepted");
    reject(0xf11c0108u, userData(unnormalized), "unnormalized guest sampler is used by a gather, which is not implemented", "unnormalized samplers: image_gather4_lz was accepted");
    reject(0xf1800308u, userData(unnormalized), "unnormalized guest sampler is used by image_get_lod, which is not implemented", "unnormalized samplers: image_get_lod was accepted");
    reject(0xf0880f08u, userData(unnormalized), "unnormalized guest sampler is used by a sample with derivatives, which is not implemented", "unnormalized samplers: image_sample_d was accepted");
    reject(0xf0800f09u, userData(unnormalized), "unnormalized guest sampler is used by an image_sample_*_a variant, which is not implemented", "unnormalized samplers: image_sample_a was accepted");
    reject(0xf09c0f18u, imageData(unnormalized, TypeCube, Format8888UNorm, 5u), "unnormalized guest sampler samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a cube T# was accepted");
    reject(0xf09c0f10u, imageData(unnormalized, Type3D, Format8888UNorm, 3u), "unnormalized guest sampler samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a 3D T# was accepted");
    reject(0xf09c0f28u, imageData(unnormalized, Type2DArray, Format8888UNorm, 5u), "unnormalized guest sampler samples a 1D-array, 2D-array, 3D, cube or multisampled image", "unnormalized samplers: a 2D-array sample was accepted");
    reject(0xf09c0f08u, imageData(unnormalized, Type2D, Format11_11_10UInt, 0u), "unnormalized guest sampler samples an image that needs a format conversion or packed access", "unnormalized samplers: a T# with a format conversion was accepted");
    for (const std::uint32_t mimg : {0xf0c00f08u, 0xf11c0108u, 0xf09c0f18u}) {
        static_cast<void>(recompile(program(mimg), imageData(normalized, mimg == 0xf09c0f18u ? TypeCube : Type2D, Format8888UNorm, mimg == 0xf09c0f18u ? 5u : 0u)));
    }

    const std::array<std::uint32_t, 5> pixelCode{0xf0800f08u, 0x00400801u, 0xf800180fu, 0x0b0a0908u, 0xbf810000u};
    const auto pixelData = userData(unnormalized);
    RecompileRequest pixel{};
    pixel.shader = {ShaderStage::Fragment, 0x4f000u, pixelCode, 0, {}};
    pixel.context.waveSize = 64;
    pixel.context.userDataBaseRegister = 0;
    pixel.context.userData = pixelData;
    pixel.context.pixel = twoParameterPixel();
    pixel.target.vulkanVersion = 0x00401000u;
    pixel.target.spirvVersion = 0x00010300u;
    pixel.target.subgroupSize = 64;
    pixel.layout.pushConstantSizeBytes = 128;
    pixel.useCache = false;
    expectFailure([&] { static_cast<void>(Recompile(pixel)); }, "unnormalized guest sampler is used by an implicit-LOD sample, which is not implemented", "unnormalized samplers: a pixel image_sample was accepted");
}

void verifyUnusedUnnormalizedSampler() {
    using namespace ShaderRecompiler;
    ImageResource image{};
    image.resourceClass = ImageResourceClass::Sampled;
    image.numericClass = IrTextureNumericClass::Float;
    image.dimension = RdnaImageDimension::Dim2D;
    image.read = true;
    ShaderInfo info;
    info.images = {image};
    info.samplers = {SamplerResource{}};
    info.sampledPairs = {{0u, 0u, 0u}};
    ResourceSnapshot snapshot;
    snapshot.images = {DescriptorValue{{0x00001000u, 0x03800000u, 0x0000c000u, 0x90000facu, 0u, 0u, 0u, 0u}, 8u}};
    snapshot.samplers = {DescriptorValue{{0x00008092u, 0x00fff000u, 0x05500000u, 0u}, 4u}};
    const auto populate = [&](ShaderInfo shader) {
        shader.runtimeImageModes = {ResourceMaterializer::RuntimeImageModes(shader.images[0])};
        BindingAllocationResult allocation;
        allocation.layout.descriptors = {{DescriptorBindingForImage(shader.images[0]), {0u}}, {DescriptorBindingKind::Samplers, {0u}}};
        DescriptorBindingBuilder{}.Populate(allocation, shader, IrShaderStage::Compute, 0u, snapshot, {});
        return allocation.bindings;
    };
    const auto bindings = populate(info);
    require(bindings.size() == 2u && bindings[0].imageUnnormalized == std::vector<bool>{true} && bindings[1].samplerUnnormalized == std::vector<bool>{true}, "unnormalized samplers: an S# without live uses was not flagged");
    auto selected = info;
    selected.images[0].indirectRoot = 0u;
    expectFailure([&] { static_cast<void>(populate(selected)); }, "unnormalized guest sampler samples an image selected at run time, which is not implemented", "unnormalized samplers: an image table root was accepted");
    snapshot.images[0].dwords[3] = 0xa0000041u;
    const auto constant = populate(info);
    require(constant.size() == 1u && constant[0].samplerUnnormalized == std::vector<bool>{true}, "unnormalized samplers: a 3D view whose channels select constants was bound or refused");
    expectFailure([&] { static_cast<void>(populate(selected)); }, "unnormalized guest sampler samples an image selected at run time, which is not implemented", "unnormalized samplers: an image table root whose channels select constants was accepted");
    snapshot.images[0].dwords[3] = 0x90000facu;
    auto compared = info;
    compared.samplers[0].depthCompare = true;
    expectFailure([&] { static_cast<void>(populate(compared)); }, "unnormalized guest sampler is used with depth comparison, which is not implemented", "unnormalized samplers: a depth-compare S# without live uses was accepted");
    snapshot.samplers[0].dwords[0] = 0x00000092u;
    const auto normalized = populate(info);
    require(normalized[0].imageUnnormalized == std::vector<bool>{false} && normalized[1].samplerUnnormalized == std::vector<bool>{false}, "unnormalized samplers: a normalized S# was flagged");
}

void verifyWaveUniformValues() {
    using namespace ShaderRecompiler;
    IrProgram program;
    program.Resources().stage = IrShaderStage::Compute;
    MemoryInfo scalar;
    scalar.kind = ResourceKind::ScalarAddress;
    MemoryInfo global;
    global.kind = ResourceKind::Global;
    program.Resources().memoryInfo = {scalar, global};
    auto& block = program.CreateBlock();
    program.SetEntryBlock(block);
    program.BlockOrder().push_back(&block);
    const auto emit = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint64_t flags = 0) -> IrValue& {
        auto& value = program.CreateValue(opcode, type, flags);
        for (auto* argument : arguments) value.AddArgument(argument);
        block.AppendInstruction(&value);
        return value;
    };
    const auto memory = [](std::uint32_t index) {
        MemoryFlags flags{index, 0u};
        std::uint64_t bits = 0;
        std::memcpy(&bits, &flags, sizeof(flags));
        return bits;
    };
    auto& zero = program.CreateValue(IrOpcode::Void, IrType::U32);
    zero.SetImmediateU32(0u);
    auto& active = program.CreateValue(IrOpcode::Void, IrType::U1);
    active.SetImmediateBool(true);
    auto& userData = emit(IrOpcode::GetUserData, IrType::U32, {&zero});
    auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
    auto& uniformSum = emit(IrOpcode::IAdd32, IrType::U32, {&userData, &userData});
    auto& laneSum = emit(IrOpcode::IAdd32, IrType::U32, {&userData, &lane});
    auto& address = emit(IrOpcode::GetAddressResource, IrType::AddressResource, {&userData, &userData});
    auto& scalarLoad = emit(IrOpcode::LoadAddressU32, IrType::U32, {&address, &uniformSum, &zero, &active}, memory(0u));
    auto& laneOffsetLoad = emit(IrOpcode::LoadAddressU32, IrType::U32, {&address, &laneSum, &zero, &active}, memory(0u));
    auto& globalLoad = emit(IrOpcode::LoadAddressU32, IrType::U32, {&address, &uniformSum, &zero, &active}, memory(1u));
    auto& fromScalarLoad = emit(IrOpcode::IAdd32, IrType::U32, {&scalarLoad, &uniformSum});
    auto& fromGlobalLoad = emit(IrOpcode::IAdd32, IrType::U32, {&globalLoad, &uniformSum});
    auto& compare = emit(IrOpcode::ULessThan32, IrType::U1, {&laneSum, &userData});
    auto& ballot = emit(IrOpcode::Ballot, IrType::U32x4, {&compare});
    const auto uniform = WaveUniformValues(program);
    for (const auto* value : {&userData, &uniformSum, &address, &scalarLoad, &fromScalarLoad, &ballot}) {
        require(uniform.contains(value), "wave-uniform values: a value every lane of the wave computes alike was not found uniform");
    }
    for (const auto* value : {&lane, &laneSum, &laneOffsetLoad, &globalLoad, &fromGlobalLoad, &compare}) {
        require(!uniform.contains(value), "wave-uniform values: a value that may differ between lanes was found uniform");
    }
}

void verifyTwoLaneUniformValues() {
    using namespace ShaderRecompiler;
    struct alignas(4096) GuestTables {
        std::array<std::uint32_t, 64> output{};
        std::array<std::uint32_t, 8> srt{};
    };
    static GuestTables guest;
    auto& output = guest.output;
    const auto outputBase = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    auto& srt = guest.srt;
    srt = {6u, 7u, 0u, 0u, static_cast<std::uint32_t>(outputBase), static_cast<std::uint32_t>((outputBase >> 32u) & 0xffffu), 64u, 0xfacu};
    const auto srtAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt.data()));
    const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(srtAddress), static_cast<std::uint32_t>(srtAddress >> 32u)};
    const std::array<std::uint32_t, 10> code{0xf4040080u, 0xfa000000u, 0xf4080200u, 0xfa000010u, 0xbf8cc07fu, 0x93040302u, 0x4a020004u, 0xe0700000u, 0x80020100u, 0xbf810000u};
    const auto multiplies = [&](std::uint32_t subgroupSize, bool multiply) {
        auto shaderCode = code;
        if (!multiply) shaderCode[5] = 0x80040302u;
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x40000u, shaderCode, 0, {}};
        request.target = BufferTarget();
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = subgroupSize;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        request.context.memory = memory.Regions();
        const auto words = Recompile(request, *capture)->spirv;
        std::size_t count = 0;
        for (std::size_t cursor = 5; cursor < words.size();) {
            const auto length = words[cursor] >> 16u;
            require(length != 0 && length <= words.size() - cursor, "two-lane uniform values: truncated SPIR-V instruction");
            if ((words[cursor] & 0xffffu) == spv::OpIMul) ++count;
            cursor += length;
        }
        return count;
    };
    for (const auto subgroupSize : {32u, 64u}) {
        require(multiplies(subgroupSize, true) == multiplies(subgroupSize, false) + 1, "two-lane uniform values: the scalar multiply must be emitted once per invocation");
    }
}

void verifyBdaReadFallbackFunctions() {
    using namespace ShaderRecompiler;
    const std::array<std::uint32_t, 3> capabilities{spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess};
    const std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    alignas(256) static std::array<std::uint32_t, 32> output{};
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    const std::array<std::uint32_t, 8> userData{0x10000000u, 0u, 0u, 0u, static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), 128u, 0x01016facu};
    const auto compile = [&](std::uint32_t loads, bool barrier, bool coherent) {
        std::vector<std::uint32_t> code{0x7e020200u, 0x7e040201u};
        for (std::uint32_t load = 0; load < loads; ++load) {
            code.push_back(0xdc308000u | (coherent ? 0x10000u : 0u) | (4u * load + 4u));
            code.push_back(((3u + load) << 24u) | 0x007d0001u);
        }
        code.push_back(0xbf8c3f70u);
        for (std::uint32_t load = 1; load < loads; ++load) code.push_back(0x4a060103u | ((3u + load) << 9u));
        if (barrier) code.push_back(0xbf8a0000u);
        code.insert(code.end(), {0xe0700000u, 0x80010300u, 0xbf810000u});
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x50000u, code, 0, {}};
        request.context.waveSize = 32;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{32u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.bdaAbiVersion = BdaAbi::Version;
        request.target.supportedCapabilities = capabilities;
        request.target.supportedExtensions = extensions;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        request.useCache = false;
        return Recompile(request).spirv;
    };
    for (const bool barrier : {false, true}) {
        for (const bool coherent : {false, true}) {
            const auto one = compile(1u, barrier, coherent);
            const auto five = compile(5u, barrier, coherent);
            std::map<std::uint32_t, std::string> names;
            std::map<std::string, std::uint32_t> definitions;
            const auto reader = std::string("read_bda_dword_bytes") + (barrier ? "" : "_stop") + (coherent ? "_coherent" : "");
            const auto span = std::string("read_bda_span") + (barrier ? "" : "_stop") + (coherent ? "_coherent" : "");
            std::string function;
            std::size_t compareExchanges = 0;
            std::size_t mainLookups = 0;
            std::size_t mainProbes = 0;
            std::size_t spanCalls = 0;
            std::array<std::size_t, 2> readerLoads{};
            for (std::size_t cursor = 5; cursor < five.size();) {
                const auto length = five[cursor] >> 16u;
                require(length != 0 && length <= five.size() - cursor, "BDA read functions: truncated SPIR-V instruction");
                const auto op = five[cursor] & 0xffffu;
                if (op == spv::OpName) names[five[cursor + 1]] = reinterpret_cast<const char*>(&five[cursor + 2]);
                if (op == spv::OpFunction) {
                    function = names[five[cursor + 2]];
                    if (function == "record_bda_fault" || function.starts_with("read_bda_dword_bytes") || function.starts_with("read_bda_span")) {
                        require((five[cursor + 3] & spv::FunctionControlDontInlineMask) != 0u, "BDA read functions: a fault, byte read or span read function may be inlined");
                        ++definitions[function];
                    }
                }
                if (op == spv::OpAtomicCompareExchange) ++compareExchanges;
                if (op == spv::OpFunctionCall && function == "main" && names[five[cursor + 3]] == "get_bda_pointer") ++mainLookups;
                if (op == spv::OpFunctionCall && function == "main") {
                    const auto& callee = names[five[cursor + 3]];
                    if (callee == "probe_bda_pointer" || callee.starts_with("read_bda_dword_bytes")) ++mainProbes;
                    if (callee == span) ++spanCalls;
                }
                if (op == spv::OpLoad && function == reader) ++readerLoads[length > 4u && (five[cursor + 4] & spv::MemoryAccessVolatileMask) != 0u];
                cursor += length;
            }
            require(definitions["record_bda_fault"] == 1u && definitions[reader] == 1u && definitions[span] == 1u, "BDA read functions: the fault, byte read and span read functions are not defined");
            for (const auto& [name, count] : definitions) require(count == 1u, "BDA read functions: a function is defined twice");
            require(compareExchanges == 1u, "BDA read functions: a fault is recorded outside record_bda_fault");
            require(mainLookups == 0u, "BDA read functions: a read site looks up its bytes inline");
            require(readerLoads[coherent] == 4u && readerLoads[!coherent] == 0u, "BDA read functions: the byte loads do not keep the access's coherence");
            require(mainProbes == 0u, "BDA read functions: a read site probes or reads bytes outside its span read function");
            require(spanCalls == 5u, "BDA read functions: a read site does not call its span read function once");
            require(five.size() - one.size() < 4u * 150u, "BDA read functions: a read site takes 150 SPIR-V words or more");
        }
    }
}

void verifyFunctionLdsBound() {
    using namespace ShaderRecompiler;
    const auto build = [](const auto& body) {
        IrProgram program;
        program.Resources().stage = IrShaderStage::Pixel;
        for (const std::uint32_t offset : {0u, 256u, 512u, 0xfffffff0u}) {
            MemoryInfo lds;
            lds.kind = ResourceKind::Lds;
            lds.offset = offset;
            program.Resources().memoryInfo.push_back(lds);
        }
        auto& block = program.CreateBlock();
        program.SetEntryBlock(block);
        program.BlockOrder().push_back(&block);
        const auto emit = [&](IrOpcode opcode, IrType type, std::initializer_list<IrValue*> arguments, std::uint32_t memory = ~0u) -> IrValue& {
            std::uint64_t bits = 0;
            if (memory != ~0u) {
                MemoryFlags flags{memory, 0u};
                std::memcpy(&bits, &flags, sizeof(flags));
            }
            auto& value = program.CreateValue(opcode, type, bits);
            for (auto* argument : arguments) value.AddArgument(argument);
            block.AppendInstruction(&value);
            return value;
        };
        const auto constant = [&](std::uint32_t immediate) -> IrValue& {
            auto& value = program.CreateValue(IrOpcode::Void, IrType::U32);
            value.SetImmediateU32(immediate);
            return value;
        };
        auto& active = program.CreateValue(IrOpcode::Void, IrType::U1);
        active.SetImmediateBool(true);
        body(program, block, emit, constant, active);
        return std::pair{FunctionLdsDwords(program), AnalyzeProgramRequirements(program)};
    };

    const auto [laneSlots, laneRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& address = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(2u)});
        for (const std::uint32_t memory : {0u, 1u, 2u}) emit(IrOpcode::WriteSharedU32, IrType::Void, {&address, &lane, &active}, memory);
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&address, &active}, 2u);
    });
    require(laneSlots == 192u, "function LDS: lane-strided slots up to byte 767 must take 192 dwords");
    require(laneRequirements.functionLds && laneRequirements.functionLdsDwords == 192u, "function LDS: the requirements do not carry the bounded size");
    {
        std::vector<std::uint32_t> rebased;
        for (const auto& [inst, address] : laneRequirements.functionLdsAddresses) rebased.push_back(address);
        std::sort(rebased.begin(), rebased.end());
        require(rebased == std::vector<std::uint32_t>{0u, 0u, 0u, 0u}, "function LDS: lane-strided slots must be addressed without the lane term");
    }

    const auto [mixed, mixedRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& address = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(2u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&address, &lane, &active}, 0u);
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&constant(8u), &active}, 0u);
    });
    require(mixed == 64u && mixedRequirements.functionLdsAddresses.empty(), "function LDS: a constant address beside lane-strided ones must keep the lane term");

    const auto [strides, stridesRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& four = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(2u)});
        auto& eight = emit(IrOpcode::IMul32, IrType::U32, {&lane, &constant(8u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&four, &lane, &active}, 0u);
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&eight, &active}, 0u);
    });
    require(strides == 128u && stridesRequirements.functionLdsAddresses.empty(), "function LDS: two lane strides must keep the lane term");

    const auto [halfWord, halfWordRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& address = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&lane, &constant(1u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&address, &lane, &active}, 0u);
    });
    require(halfWord == 64u && halfWordRequirements.functionLdsAddresses.empty(), "function LDS: a lane stride that is not whole dwords must keep the lane term");

    const auto [summed, summedRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& scaled = emit(IrOpcode::IMul32, IrType::U32, {&constant(16u), &lane});
        auto& low = emit(IrOpcode::IAdd32, IrType::U32, {&scaled, &constant(4u)});
        auto& other = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& shifted = emit(IrOpcode::ShiftLeftLogical32, IrType::U32, {&other, &constant(4u)});
        auto& high = emit(IrOpcode::IAdd32, IrType::U32, {&constant(1024u), &shifted});
        emit(IrOpcode::WriteSharedU32x2, IrType::Void, {&low, &lane, &lane, &active}, 0u);
        emit(IrOpcode::LoadSharedU32x4, IrType::U32x4, {&high, &active}, 1u);
    });
    {
        std::vector<std::uint32_t> rebased;
        for (const auto& [inst, address] : summedRequirements.functionLdsAddresses) rebased.push_back(address);
        std::sort(rebased.begin(), rebased.end());
        require(rebased == std::vector<std::uint32_t>{4u, 1024u}, "function LDS: lane ids of separate instructions with one stride must share the rebase");
        require(summedRequirements.functionLdsDwords == 384u && summed == 576u, "function LDS: the rebased array must hold the rebased addresses");
    }

    const auto [wide, wideRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& lane = emit(IrOpcode::LaneId, IrType::U32, {});
        auto& masked = emit(IrOpcode::BitwiseAnd32, IrType::U32, {&lane, &constant(7u)});
        auto& scaled = emit(IrOpcode::IMul32, IrType::U32, {&masked, &constant(16u)});
        auto& chosen = emit(IrOpcode::SelectU32, IrType::U32, {&active, &scaled, &constant(0x100u)});
        emit(IrOpcode::LoadSharedU32x4, IrType::U32x4, {&chosen, &active}, 0u);
    });
    require(wide == 128u, "function LDS: a 4-dword access at byte 0x100 must take 68 dwords, rounded to 128");
    require(wideRequirements.functionLdsDwords == 128u, "function LDS: the requirements do not carry the wide access's size");
    require(wideRequirements.functionLdsAddresses.empty(), "function LDS: a selected address must keep the lane term");

    const auto [unbounded, unboundedRequirements] = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        auto& user = emit(IrOpcode::GetUserData, IrType::U32, {&constant(0u)});
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&user, &user, &active}, 0u);
    });
    require(unbounded == FunctionLdsDwordLimit && unboundedRequirements.functionLdsDwords == FunctionLdsDwordLimit, "function LDS: an address without a bound must keep the full array");

    const auto wrapping = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        emit(IrOpcode::LoadSharedU32, IrType::U32, {&constant(0x20u), &active}, 3u);
    }).first;
    require(wrapping == FunctionLdsDwordLimit, "function LDS: an offset that can wrap the address must keep the full array");

    const auto loop = build([](IrProgram& program, IrBlock& block, auto& emit, auto& constant, IrValue& active) {
        auto& phi = program.CreateValue(IrOpcode::Phi, IrType::U32);
        block.AppendInstruction(&phi);
        auto& next = emit(IrOpcode::IAdd32, IrType::U32, {&phi, &constant(4u)});
        phi.AddPhiOperand(&block, &constant(0u));
        phi.AddPhiOperand(&block, &next);
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&phi, &next, &active}, 0u);
    }).first;
    require(loop == FunctionLdsDwordLimit, "function LDS: a loop-carried address must keep the full array");

    const auto joined = build([](IrProgram& program, IrBlock& block, auto& emit, auto& constant, IrValue& active) {
        auto& phi = program.CreateValue(IrOpcode::Phi, IrType::U32);
        block.AppendInstruction(&phi);
        phi.AddPhiOperand(&block, &constant(0x40u));
        phi.AddPhiOperand(&block, &constant(0x3fcu));
        emit(IrOpcode::WriteSharedU32, IrType::Void, {&phi, &constant(1u), &active}, 0u);
    }).first;
    require(joined == 256u, "function LDS: a phi of bounded addresses must take its largest");

    const auto unsized = build([](IrProgram&, IrBlock&, auto& emit, auto& constant, IrValue& active) {
        emit(IrOpcode::LoadShared, IrType::U32, {&constant(0u), &active}, 0u);
    }).first;
    require(unsized == FunctionLdsDwordLimit, "function LDS: an access without a known width must keep the full array");
}

struct NestedRequest {
    std::vector<std::uint32_t> code;
    std::array<std::uint32_t, 2> userData{};
    ShaderRecompiler::RecompileRequest request{};

    NestedRequest(std::vector<std::uint32_t> words, const void* srt) : code(std::move(words)) {
        using namespace ShaderRecompiler;
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(srt));
        userData = {static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
        request.shader = {ShaderStage::Compute, 0x60000u + static_cast<std::uint64_t>(code.size()) * 4u, code, 0, {}};
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 0;
        request.context.userData = userData;
        request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
        request.target = BufferTarget();
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 32;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
    }
};

bool RegionsCover(const std::vector<ShaderRecompiler::MemoryRegion>& regions, const void* pointer, std::size_t bytes) {
    auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
    const auto end = address + bytes;
    while (address < end) {
        const auto region = std::find_if(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& candidate) { return address >= candidate.guestAddress && address < candidate.guestAddress + candidate.bytes.size(); });
        if (region == regions.end()) return false;
        address = region->guestAddress + region->bytes.size();
    }
    return true;
}

bool RegionsTouch(const std::vector<ShaderRecompiler::MemoryRegion>& regions, std::uint64_t address, std::size_t bytes) {
    return std::any_of(regions.begin(), regions.end(), [&](const ShaderRecompiler::MemoryRegion& region) { return region.guestAddress < address + bytes && address < region.guestAddress + region.bytes.size(); });
}

bool GuardRecords(const ShaderRecompiler::ResourceCapture& capture) {
    const auto& plan = *capture.plan;
    const auto& flat = capture.snapshot.flattenedSrt;
    const auto& poison = capture.snapshot.srtPoison;
    const auto flags = plan.srtReads.size();
    const auto guards = plan.guardedSrtSlots.size();
    if (flat.size() != flags + guards + 3u * poison.size()) return false;
    for (std::size_t guard = 0; guard < guards; ++guard) {
        const auto record = std::find_if(poison.begin(), poison.end(), [&](const ShaderRecompiler::SrtReadPoison& entry) { return entry.slot == plan.guardedSrtSlots[guard]; });
        const auto expected = record == poison.end() ? 0u : static_cast<std::uint32_t>(record - poison.begin()) + 1u;
        if (flat[flags + guard] != expected) return false;
    }
    for (std::size_t record = 0; record < poison.size(); ++record) {
        const auto* words = flat.data() + flags + guards + 3u * record;
        if (words[0] != poison[record].pc || words[1] != static_cast<std::uint32_t>(poison[record].address) || words[2] != static_cast<std::uint32_t>(poison[record].address >> 32u) || flat.at(poison[record].slot) != 0u) return false;
    }
    return true;
}

void verifyGuardedNullPointers() {
    using namespace ShaderRecompiler;
    constexpr std::size_t Reserved = 0x10000;
    void* reserved = nullptr;
#ifdef _WIN32
    reserved = VirtualAlloc(nullptr, Reserved, MEM_RESERVE, PAGE_NOACCESS);
#else
    reserved = mmap(nullptr, Reserved, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (reserved == MAP_FAILED) reserved = nullptr;
#endif
    require(reserved != nullptr, "guarded pointer: cannot reserve an inaccessible range");
    const auto unmapped = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(reserved));
    alignas(256) static std::array<std::uint32_t, 64> output{};
    alignas(256) static std::array<std::uint32_t, 16> root{};
    static std::uint32_t payload = 0x3f800000u;
    static std::uint64_t indirect = 0;
    const auto outputAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(output.data()));
    root = {static_cast<std::uint32_t>(outputAddress), static_cast<std::uint32_t>((outputAddress >> 32u) & 0xffffu), static_cast<std::uint32_t>(sizeof(output)), 0x30027facu, 1u};
    const auto point = [&](std::uint64_t address) {
        root[6] = static_cast<std::uint32_t>(address);
        root[7] = static_cast<std::uint32_t>(address >> 32u);
    };
    const auto payloadAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&payload));
    const std::vector<std::uint32_t> branchCode{0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
        0xbf8cc07fu, 0xbf068008u, 0xbf850003u, 0xf4000305u, 0xfa000000u, 0xbf8cc07fu, 0x7e02020cu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u};
    const std::vector<std::uint32_t> chainCode{0xf4080100u, 0xfa000000u, 0xf4000200u, 0xfa000010u, 0xf4040280u, 0xfa000018u, 0xbe8c03ffu, 0x11111111u,
        0xbf8cc07fu, 0xbf068008u, 0xbf850006u, 0xf4040385u, 0xfa000000u, 0xbf8cc07fu, 0xf4000307u, 0xfa000000u, 0xbf8cc07fu, 0x7e02020cu, 0x34040082u,
        0xe0701000u, 0x80010102u, 0xbf810000u};
    const std::vector<std::uint32_t> descriptorCode{0xf4040280u, 0xfa000018u, 0xbf8cc07fu, 0xf4080105u, 0xfa000000u, 0xbf8cc07fu, 0x34040082u, 0xe0701000u,
        0x80010002u, 0xbf810000u};
    const auto slotPc = [](const IrResourcePlan& plan, std::uint32_t slot) { return plan.srtReads.at(slot).value->Flags<MemoryFlags>().pc; };
    const auto faultBinding = [](const RecompileResult& result) {
        return std::any_of(result.bindings.begin(), result.bindings.end(), [](const DescriptorBinding& binding) { return binding.role == DescriptorRole::FaultBuffer; });
    };
    const auto samePipeline = [](const RecompileResult& left, const RecompileResult& right) {
        return left.variantId == right.variantId && left.specializationId == right.specializationId && left.PipelineVariantId() == right.PipelineVariantId() && left.spirv == right.spirv;
    };

    NestedRequest branch(branchCode, root.data());
    point(payloadAddress);
    AgcDriver::ShaderMemory validMemory({});
    const auto valid = validMemory.Capture(branch.request);
    require(valid->snapshot.srtPoison.empty() && valid->plan->guardedSrtSlots.size() == 1u && slotPc(*valid->plan, valid->plan->guardedSrtSlots[0]) == 0x2cu && GuardRecords(*valid), "guarded pointer: a mapped nested pointer was poisoned, or its read has no guard");
    require(valid->snapshot.flattenedSrt.at(valid->plan->guardedSrtSlots[0]) == payload, "guarded pointer: a mapped nested read does not hold its word");
    branch.request.context.memory = validMemory.Regions();
    const auto validResult = Recompile(branch.request, *valid);
    require(validResult->poisonedSrtReads == 0u && faultBinding(*validResult) && validResult->bdaAbiVersion == BdaAbi::Version, "guarded pointer: a guarded program has no fault buffer");

    for (const auto pointer : {std::uint64_t{0}, unmapped}) {
        for (const auto condition : {1u, 0u}) {
            point(pointer);
            root[4] = condition;
            branch.request.context.memory = {};
            AgcDriver::ShaderMemory memory({});
            const auto capture = memory.Capture(branch.request);
            const auto& poison = capture->snapshot.srtPoison;
            require(poison.size() == 1u && poison[0].pc == 0x2cu && poison[0].address == pointer && slotPc(*capture->plan, poison[0].slot) == 0x2cu, "guarded pointer: the nested read was not poisoned at its pc and address");
            require(GuardRecords(*capture), "guarded pointer: the poison record is not where the guard reads it");
            const auto regions = memory.Regions();
            require(!RegionsTouch(regions, pointer, 4u) && RegionsCover(regions, root.data() + 6, 8u), "guarded pointer: the captured regions are wrong");
            branch.request.context.memory = regions;
            const auto result = Recompile(branch.request, *capture);
            require(result->poisonedSrtReads == 1u && faultBinding(*result) && result->bdaAbiVersion == BdaAbi::Version, "guarded pointer: the poisoned capture does not report through the fault buffer");
            require(samePipeline(*result, *validResult), "guarded pointer: poison changed the variant, the specialization or the module");
            const auto replay = Recompile(branch.request);
            require(replay.poisonedSrtReads == 1u && samePipeline(replay, *result), "guarded pointer: a replay of the captured regions did not reproduce the poison");
            verifyResult(*result, replay);
#if ANYPS5_ENABLE_SPIRV_TOOLS
            static_cast<void>(ValidateAndOptimizeSpirv(result->spirv, branch.request.target.vulkanVersion, branch.request.target.spirvVersion));
#endif
        }
    }
    root[4] = 1u;

    NestedRequest chain(chainCode, root.data());
    point(unmapped);
    AgcDriver::ShaderMemory chainMemory({});
    const auto chained = chainMemory.Capture(chain.request);
    const auto& chainPoison = chained->snapshot.srtPoison;
    require(chainPoison.size() == 3u && GuardRecords(*chained), "guarded pointer: a pointer read through an unmapped pointer did not poison its two words and the read through it");
    for (const auto& poison : chainPoison) {
        const auto own = slotPc(*chained->plan, poison.slot);
        const bool pointerWord = own == 0x2cu && (poison.address == unmapped || poison.address == unmapped + 4u);
        const bool throughPointer = own == 0x38u && poison.address == unmapped;
        require(poison.pc == 0x2cu && (pointerWord || throughPointer), "guarded pointer: a derived poison does not name the inaccessible read");
    }
    chain.request.context.memory = chainMemory.Regions();
    const auto chainResult = Recompile(chain.request, *chained);
    indirect = 0;
    point(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(&indirect)));
    AgcDriver::ShaderMemory nullChainMemory({});
    const auto nullChain = nullChainMemory.Capture(chain.request);
    require(nullChain->snapshot.srtPoison.size() == 1u && nullChain->snapshot.srtPoison[0].pc == 0x38u && nullChain->snapshot.srtPoison[0].address == 0u && GuardRecords(*nullChain), "guarded pointer: a null pointer read from memory did not poison the read through it");
    chain.request.context.memory = nullChainMemory.Regions();
    const auto nullChainResult = Recompile(chain.request, *nullChain);
    indirect = payloadAddress;
    AgcDriver::ShaderMemory mappedChainMemory({});
    const auto mappedChain = mappedChainMemory.Capture(chain.request);
    require(mappedChain->snapshot.srtPoison.empty() && mappedChain->plan->guardedSrtSlots.size() == 3u && GuardRecords(*mappedChain), "guarded pointer: a mapped pointer chain was poisoned, or its reads have no guards");
    chain.request.context.memory = mappedChainMemory.Regions();
    const auto mappedChainResult = Recompile(chain.request, *mappedChain);
    require(chainResult->poisonedSrtReads == 3u && nullChainResult->poisonedSrtReads == 1u && mappedChainResult->poisonedSrtReads == 0u, "guarded pointer: a chain result does not count its poisoned reads");
    require(samePipeline(*chainResult, *mappedChainResult) && samePipeline(*nullChainResult, *mappedChainResult), "guarded pointer: a chain's poison changed the variant, the specialization or the module");

    const auto poisonedWords = [](const ResourceCapture& capture, std::uint32_t pc, std::uint64_t base) {
        std::vector<std::uint64_t> addresses;
        for (const auto& entry : capture.snapshot.srtPoison) {
            if (entry.pc != pc || capture.snapshot.flattenedSrt.at(entry.slot) != 0u) return false;
            addresses.push_back(entry.address);
        }
        std::sort(addresses.begin(), addresses.end());
        return addresses == std::vector<std::uint64_t>{base, base + 4u, base + 8u, base + 12u} && GuardRecords(capture);
    };
    const auto zeroBuffer = [](const ResourceCapture& capture) {
        return capture.snapshot.buffers.size() == 1u && std::all_of(capture.snapshot.buffers[0].dwords.begin(), capture.snapshot.buffers[0].dwords.end(), [](std::uint32_t word) { return word == 0u; });
    };
    alignas(256) static std::array<std::uint32_t, 4> zeroTable{};
    NestedRequest descriptor(descriptorCode, root.data());
    point(static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(zeroTable.data())));
    AgcDriver::ShaderMemory zeroMemory({});
    const auto zeroCapture = zeroMemory.Capture(descriptor.request);
    require(zeroCapture->snapshot.srtPoison.empty() && zeroBuffer(*zeroCapture) && GuardRecords(*zeroCapture), "guarded pointer: a zero V# read from mapped memory was poisoned or not read");
    descriptor.request.context.memory = zeroMemory.Regions();
    const auto zeroResult = Recompile(descriptor.request, *zeroCapture);
    for (const auto pointer : {std::uint64_t{0}, unmapped}) {
        point(pointer);
        descriptor.request.context.memory = {};
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(descriptor.request);
        require(poisonedWords(*capture, 0xcu, pointer), "guarded pointer: a V# loaded through a null or unmapped nested pointer did not poison its four words at its pc");
        require(zeroBuffer(*capture), "guarded pointer: a V# loaded through an inaccessible pointer is not bound as zero words");
        const auto regions = memory.Regions();
        descriptor.request.context.memory = regions;
        const auto result = Recompile(descriptor.request, *capture);
        require(result->poisonedSrtReads == 4u && faultBinding(*result) && result->bdaAbiVersion == BdaAbi::Version, "guarded pointer: a capture with a poisoned V# does not report through the fault buffer");
        require(samePipeline(*result, *zeroResult), "guarded pointer: a V# zeroed by poison and a V# read as zeros take different variants, specializations or modules");
        const auto replay = Recompile(descriptor.request);
        require(replay.poisonedSrtReads == 4u && samePipeline(replay, *result), "guarded pointer: a replay did not reproduce the poisoned V#");
    }

    alignas(256) static std::array<std::uint32_t, 8> table{};
    alignas(256) static std::array<std::uint32_t, 4> vertexRoot{};
    table = {0u, 0u, 0u, 0u, static_cast<std::uint32_t>(payloadAddress), static_cast<std::uint32_t>((payloadAddress >> 32u) & 0xffffu), 4u, 0x30027facu};
    const std::vector<std::uint32_t> vertexDescriptorCode{0xf4000084u, 0xfa000000u, 0xf4040104u, 0xfa000008u, 0x7e020280u, 0xbf8cc07fu, 0xbf068002u, 0xbf850006u,
        0xf4080302u, 0xfa000010u, 0xbf8cc07fu, 0xe0300000u, 0x80030100u, 0xbf8c3f70u, 0xf80008cfu, 0x01010101u, 0xbf810000u};
    NestedRequest vertex(vertexDescriptorCode, vertexRoot.data());
    vertex.request.shader.stage = ShaderStage::Vertex;
    vertex.request.context.compute.reset();
    vertex.request.context.vertex = ShaderVertexStageInfo{};
    vertex.request.context.userDataBaseRegister = 8;
    const auto tableAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(table.data()));
    vertexRoot[2] = static_cast<std::uint32_t>(tableAddress);
    vertexRoot[3] = static_cast<std::uint32_t>(tableAddress >> 32u);
    AgcDriver::ShaderMemory mappedVertexMemory({});
    const auto mappedVertex = mappedVertexMemory.Capture(vertex.request);
    require(mappedVertex->snapshot.srtPoison.empty() && mappedVertex->snapshot.buffers.size() == 1u && mappedVertex->snapshot.buffers[0].dwords[0] == table[4], "guarded pointer: a vertex V# behind a mapped pointer was poisoned or not read");
    vertexRoot[2] = 0u;
    vertexRoot[3] = 0u;
    for (const auto guard : {0u, 1u}) {
        vertexRoot[0] = guard;
        vertex.request.context.memory = {};
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(vertex.request);
        require(poisonedWords(*capture, 0x20u, 0x10u), "guarded pointer: a vertex V# behind a branch and a null pointer did not poison its four words at its pc");
        const auto regions = memory.Regions();
        vertex.request.context.memory = regions;
        const auto result = Recompile(vertex.request, *capture);
        require(result->poisonedSrtReads == 4u && faultBinding(*result) && !result->spirv.empty(), "guarded pointer: a vertex capture with a poisoned V# has no fault buffer");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        static_cast<void>(ValidateAndOptimizeSpirv(result->spirv, vertex.request.target.vulkanVersion, vertex.request.target.spirvVersion));
#endif
    }
    NestedRequest rootless(branchCode, nullptr);
    AgcDriver::ShaderMemory rootlessMemory({});
    expectFailure([&] { static_cast<void>(rootlessMemory.Capture(rootless.request)); }, "null or misaligned address", "guarded pointer: a null user-data pointer was accepted");
    point(0u);
#ifdef _WIN32
    VirtualFree(reserved, 0, MEM_RELEASE);
#else
    munmap(reserved, Reserved);
#endif
}

int main(int argc, char** argv) {
    try {
        using namespace ShaderRecompiler;
        if (argc == 2 && std::string_view(argv[1]) == "--bindless") {
            verifyBindlessTable();
            std::cout << "Bindless mapping, indexing capabilities and strict validation passed\n";
            return 0;
        }
        require(argc == 1, "unknown shader memory test arguments");
        verifyRegisterSources();
        verifyEvaluatedValues();
        verifyPureFlatSlots();
        verifyBindlessTable();
        verifyDescriptorPhis();
        verifyProgramCounterRelativeData();
        verifyLanesOutsideHostSubgroup();
        verifyHalfWaveReduction();
        verifyMeshConfiguration();
        verifyPixelInputs();
        verifyPixelRequestSerialization();
        verifyLegacyPixelRequests();
        verifyPixelExportReplay();
        verifyPixelParameterSlots();
        verifyF16PixelParameterSlots();
        verifyComputedTexelOffsets();
        verifyShaderClockScopes();
        verifyInt64AtomicCapabilities();
        verifyUnnormalizedSamplers();
        verifyUnusedUnnormalizedSampler();
        verifyWaveUniformValues();
        verifyTwoLaneUniformValues();
        verifyBdaReadFallbackFunctions();
        verifyFunctionLdsBound();
        verifyGuardedNullPointers();
#if ANYPS5_ENABLE_SPIRV_TOOLS
        const std::vector<std::uint32_t> minimalSpirv{
            0x07230203u, 0x00010000u, 0u, 5u, 0u,
            0x00020011u, 1u,
            0x0003000eu, 0u, 1u,
            0x0005000fu, 5u, 3u, 0x6e69616du, 0u,
            0x00060010u, 3u, 17u, 1u, 1u, 1u,
            0x00020013u, 1u,
            0x00030021u, 2u, 1u,
            0x00050036u, 1u, 3u, 0u, 2u,
            0x000200f8u, 4u,
            0x00010000u,
            0x000100fdu,
            0x00010038u
        };
        const auto optimizedSpirv = ValidateAndOptimizeSpirv(minimalSpirv, 0x00401001u, 0x00010000u);
        require(optimizedSpirv.size() < minimalSpirv.size(), "SPIR-V optimization did not remove the no-op");
        require(optimizedSpirv == ValidateAndOptimizeSpirv(minimalSpirv, 0x00401001u, 0x00010000u), "SPIR-V optimization is not deterministic");
#endif
        const std::array<std::uint32_t, 8> code{0xf4040004u, 0xfa000000u, 0xf4000080u, 0xfa000000u, 0x7e000202u, 0xf80008cfu, 0u, 0xbf810000u};
        struct alignas(4096) GuestTables {
            std::uint32_t payload = 0;
            std::uint64_t table = 0;
        };
        static GuestTables guest;
        auto& payload = guest.payload;
        payload = 0x3f800000u;
        auto& table = guest.table;
        table = reinterpret_cast<std::uintptr_t>(&payload);
        const auto address = reinterpret_cast<std::uintptr_t>(&table);
        const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
        request.target = BufferTarget();
        request.context.waveSize = 64;
        request.context.userDataBaseRegister = 8;
        request.context.userData = userData;
        request.context.vertex = ShaderVertexStageInfo{};
        request.target.vulkanVersion = 0x00401000u;
        request.target.spirvVersion = 0x00010300u;
        request.target.subgroupSize = 64;
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;

        expectFailure([&] { static_cast<void>(Recompile(request)); }, "SrtWalker::EvaluateRuntimeSources", "missing snapshot unexpectedly read live memory");
        AgcDriver::ShaderMemory memory({});
        const auto capture = memory.Capture(request);
        {
            // The payload slot is consumed by the export alone: pure, its leaf traced at the
            // payload's address; the table pointer's words (the payload's address cone) are among
            // the other reads.
            const auto& pure = capture->plan->pureFlatSlots;
            require(std::count(pure.begin(), pure.end(), std::uint8_t{1}) == 1, "the payload slot is not the one pure flat slot");
            const auto& trace = capture->readTrace;
            require(trace.leaves.size() == 1 && trace.leaves[0].second == reinterpret_cast<std::uintptr_t>(&payload), "the pure slot's leaf was not traced at the payload");
            const auto slot = trace.leaves[0].first;
            require(slot < pure.size() && pure[slot] != 0 && capture->snapshot.flattenedSrt.at(slot) == payload, "the traced leaf is not the pure slot");
            require(std::find(trace.otherReads.begin(), trace.otherReads.end(), address) != trace.otherReads.end(), "the table pointer read was not traced among the other reads");
            require(std::find(trace.otherReads.begin(), trace.otherReads.end(), reinterpret_cast<std::uintptr_t>(&payload)) == trace.otherReads.end(), "the payload counts as a walk read");
        }
        auto regions = memory.Regions();
        std::size_t capturedBytes = 0;
        for (const auto& region : regions) capturedBytes += region.bytes.size();
        require(capturedBytes == sizeof(table) + sizeof(payload), "nested pointer reads were not captured");
        request.context.memory = regions;
        const auto first = Recompile(request);
        require(!first.spirv.empty(), "empty compiled shader");
        require(!first.cacheHit, "first shader compilation unexpectedly hit the cache");
        const auto plan = GetResourcePlan(request);
        require(plan == GetResourcePlan(request), "resource plan was rebuilt");
        const auto cached = Recompile(request);
        require(cached.cacheHit, "unchanged shader did not hit the cache");
        verifyResult(first, cached);
        auto relocated = request;
        relocated.shader.codeAddress += 0x1000;
        require(Recompile(relocated).cacheHit, "shader relocation caused recompilation");
        auto changedTarget = request;
        changedTarget.target.subgroupSize = 32;
        require(GetResourcePlan(changedTarget) != plan, "different target reused the source entry");
        std::vector<std::uint32_t> changedCode(code.begin(), code.end());
        changedCode.insert(changedCode.begin(), 0xbf800000u);
        auto changedSource = request;
        changedSource.shader.code = changedCode;
        require(GetResourcePlan(changedSource) != plan, "changed code reused the source entry");
        auto uncached = request;
        uncached.useCache = false;
        require(GetResourcePlan(uncached) != plan, "disabled cache reused the resource plan");
        const auto fresh = Recompile(uncached);
        require(!fresh.cacheHit, "disabled cache reused the compiled variant");
        verifyResult(first, fresh);
        require(!RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(uncached)).request.useCache, "cache policy was lost in serialization");
        auto offsets = uncached;
        offsets.target.nonConstantImageOffsets = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(offsets)).request.target.nonConstantImageOffsets, "non-constant texel offsets were lost in serialization");
        auto srgb = uncached;
        srgb.target.srgbDecodeFormats = 3u;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(srgb)).request.target.srgbDecodeFormats == 3u, "the sRGB formats decoded in the shader were lost in serialization");
        auto narrowClock = uncached;
        narrowClock.target.narrowSubgroupClock = true;
        require(RequestSerializer{}.Deserialize(RequestSerializer{}.Serialize(narrowClock)).request.target.narrowSubgroupClock, "the narrow subgroup clock was lost in serialization");
        auto changedLayout = request;
        changedLayout.layout.pushConstantSizeBytes = 64;
        require(!Recompile(changedLayout).cacheHit, "binding layout change reused an incompatible variant");
        require(Recompile(changedLayout).cacheHit, "new binding layout variant was not cached");
        require(Recompile(request).cacheHit, "compiling a new variant evicted the original");
        auto missingMemory = request;
        missingMemory.context.memory = {};
        expectFailure([&] { static_cast<void>(Recompile(missingMemory)); }, "SrtWalker::EvaluateRuntimeSources", "cache hit bypassed resource validation");
#if ANYPS5_ENABLE_SPIRV_TOOLS
        auto invalidSpirv = first.spirv;
        invalidSpirv[0] = 0;
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(invalidSpirv, request.target.vulkanVersion, request.target.spirvVersion)); }, "SPIR-V validation before optimization failed", "invalid SPIR-V passed validation");
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(first.spirv, 0x00400000u, 0x00010600u)); }, "unsupported Vulkan/SPIR-V target", "incompatible target accepted");
        expectFailure([&] { static_cast<void>(ValidateAndOptimizeSpirv(first.spirv, 0x00405000u, 0x00010600u)); }, "unsupported Vulkan target", "unknown Vulkan target accepted");
#endif
        payload = 0x40000000u;
        AgcDriver::ShaderMemory updatedMemory({});
        const auto updatedCapture = updatedMemory.Capture(request);
        const auto updatedRegions = updatedMemory.Regions();
        auto updated = request;
        updated.context.memory = updatedRegions;
        const auto updatedCached = Recompile(updated);
        require(updatedCached.cacheHit, "dynamic shader data caused recompilation");
        updated.useCache = false;
        verifyResult(updatedCached, Recompile(updated));
        bool changedData = updatedCached.pushConstants != first.pushConstants;
        for (std::size_t i = 0; i < first.bindings.size(); ++i) changedData = changedData || updatedCached.bindings.at(i).guestDescriptor != first.bindings[i].guestDescriptor;
        require(changedData, "cache hit retained stale shader data");
        {
            // Two captures differing only in the payload (the pure slot) recompile to the same
            // variant, with bindings equal apart from that slot's FlattenedSrt word.
            payload = 0x40400000u;
            AgcDriver::ShaderMemory changedMemory({});
            const auto changedCapture = changedMemory.Capture(request);
            auto changed = updated;
            changed.useCache = true;
            changed.context.memory = changedMemory.Regions();
            auto before = updated;
            before.useCache = true;
            before.context.memory = updatedRegions;
            const auto first = Recompile(before, *updatedCapture);
            const auto second = Recompile(changed, *changedCapture);
            require(first->variantId == second->variantId, "a pure slot's value changed the variant");
            require(first->bindings.size() == second->bindings.size(), "a pure slot's value changed the bindings");
            const auto slot = changedCapture->readTrace.leaves.at(0).first;
            for (std::size_t i = 0; i < first->bindings.size(); ++i) {
                const auto& left = first->bindings[i];
                const auto& right = second->bindings[i];
                if (left.role != DescriptorRole::FlattenedSrt) {
                    require(left.guestDescriptor == right.guestDescriptor, "a pure slot's value changed a non-data binding");
                    continue;
                }
                require(left.guestDescriptor.size() == right.guestDescriptor.size() && left.guestDescriptor.at(slot) == 0x40000000u && right.guestDescriptor.at(slot) == 0x40400000u, "the FlattenedSrt binding does not carry the pure slot's value");
                for (std::size_t j = 0; j < left.guestDescriptor.size(); ++j) {
                    if (j != slot) require(left.guestDescriptor[j] == right.guestDescriptor[j], "the FlattenedSrt binding differs beyond the pure slot");
                }
            }
            payload = 0x40000000u;
        }
        auto concurrent = request;
        concurrent.layout.pushConstantSizeBytes = 60;
        std::array<std::future<RecompileResult>, 4> concurrentResults;
        for (auto& future : concurrentResults) future = std::async(std::launch::async, [concurrent] { return Recompile(concurrent); });
        std::uint32_t compilations = 0;
        for (auto& future : concurrentResults) {
            const auto result = future.get();
            if (!result.cacheHit) ++compilations;
        }
        require(compilations == 1, "concurrent requests compiled the same variant repeatedly");
        const auto serialized = RequestSerializer{}.Serialize(request);
        table = 0;
        payload = 0xdeadbeefu;
        verifyResult(first, Recompile(request));
        auto replay = RequestSerializer{}.Deserialize(serialized);
        verifyResult(first, Recompile(replay.request));
        RequestMemoryView view(replay.request.context.memory);
        const auto runtime = view.MakeRuntime(userData, request.shader.codeAddress);
        std::uint32_t captured = 0;
        require(runtime.readMemory(runtime.userContext, reinterpret_cast<std::uintptr_t>(&payload), &captured) && captured == 0x3f800000u, "snapshot changed with live memory");

        request.context.userDataBaseRegister = 0x8c;
        expectFailure([&] { static_cast<void>(PrepareResourceProgram(request)); }, "shader user data exceeds the scalar register bank", "PM4 register address accepted as SGPR base");
        request.context.userDataBaseRegister = 105;
        expectFailure([&] { static_cast<void>(PrepareResourceProgram(request)); }, "shader user data exceeds the scalar register bank", "user data overran scalar register bank");
        request.context.userDataBaseRegister = 8;
        request.context.memory = {};
        AgcDriver::ShaderMemory guarded({});
        const auto poisoned = guarded.Capture(request);
        require(poisoned->snapshot.srtPoison.size() == 1u && poisoned->snapshot.srtPoison[0].pc == 8u && poisoned->snapshot.srtPoison[0].address == 0u && GuardRecords(*poisoned), "null nested pointer was not poisoned at its read");
        const auto guardedRegions = guarded.Regions();
        request.context.memory = guardedRegions;
        const auto poisonedVertex = Recompile(request, *poisoned);
        require(poisonedVertex->poisonedSrtReads == 1u && poisonedVertex->bdaAbiVersion == BdaAbi::Version && poisonedVertex->PipelineVariantId() == first.PipelineVariantId() && std::any_of(poisonedVertex->bindings.begin(), poisonedVertex->bindings.end(), [](const DescriptorBinding& binding) { return binding.role == DescriptorRole::FaultBuffer; }), "a vertex shader reading through a null nested pointer has no fault buffer or another variant");
        request.context.memory = {};
        const std::array<std::uint32_t, 2> nullUserData{};
        request.context.userData = nullUserData;
        AgcDriver::ShaderMemory invalid({});
        expectFailure([&] { invalid.Capture(request); }, "null or misaligned address", "null user-data pointer was accepted");
        std::cout << "Shader memory capture, strict validation and deterministic replay passed\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
