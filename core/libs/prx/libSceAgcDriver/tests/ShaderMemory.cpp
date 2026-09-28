#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "Optimization/RequestMemoryView.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "Optimization/SrtWalker/SrtFlatSlotClasses.hpp"
#if ANYPS5_ENABLE_SPIRV_TOOLS
#include "SpirvBackend/SpirvOptimizer.hpp"
#endif
#include <algorithm>
#include <array>
#include <iostream>
#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void expectFailure(TAction action, const char* expected, const char* message) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        require(std::string(error.what()).find(expected) != std::string::npos, "unexpected failure reason");
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
    std::array<std::array<std::uint32_t, 8>, 4> heap{};
    const auto makeTexture = [&](std::uint32_t entry, const Texture& texture) {
        const auto base = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(texture.bytes.data()));
        heap[entry] = {static_cast<std::uint32_t>(base >> 8u), static_cast<std::uint32_t>((base >> 40u) & 0xffu) | (Format8888UNorm << 20u) | (3u << 30u), 3u << 14u, 0xfacu | (Type2D << 28u), 0u, 0u, 0u, 0u};
    };
    makeTexture(0, textures[0]);
    makeTexture(1, textures[1]);
    heap[3] = heap[0];
    std::array<std::array<std::uint32_t, 4>, 3> materials{{{0u, 1u, 0u, 0u}, {0u, 0u, 0u, 0u}, {0u, 3u, 0u, 0u}}};
    std::array<std::uint32_t, 4> output{};
    const auto bufferDescriptor = [](const void* base, std::uint32_t stride, std::uint32_t records) {
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(base));
        return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>((address >> 32u) & 0xffffu) | (stride << 16u), records, 0xfacu};
    };
    std::array<std::uint32_t, 16> srt{};
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
    const std::array<std::uint32_t, 1> capabilities{29u};
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
        request.target.fragmentShaderBarycentricEnabled = false;
        request.layout.pushConstantSizeBytes = 128;
        return request;
    };
    const auto covered = [](const std::vector<MemoryRegion>& regions, const void* pointer, std::size_t bytes) {
        const auto address = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
        return std::any_of(regions.begin(), regions.end(), [&](const MemoryRegion& region) { return address >= region.guestAddress && address + bytes <= region.guestAddress + region.bytes.size(); });
    };
    const auto mappingOf = [&](const ResourceSnapshot& snapshot) {
        require(snapshot.flattenedSrt.size() >= 1u + 2u * slots, "bindless: the mapping block is missing from the flattened SRT");
        return std::vector<std::uint32_t>(snapshot.flattenedSrt.end() - static_cast<std::ptrdiff_t>(1u + 2u * slots), snapshot.flattenedSrt.end());
    };
    const auto tableRoot = [&](const ResourceCapture& capture, std::uint32_t direct) {
        require(capture.specialization.images.size() == direct + slots - 1u, "bindless: the specialization does not hold the table slots");
        require(capture.snapshot.images.size() == direct + slots - 1u, "bindless: the snapshot does not hold the table slots");
        std::uint32_t root = ImageResource::NoIndirectImage;
        for (std::uint32_t i = 0; i < direct; i++) {
            if (capture.specialization.images[i].indirectRoot == i) root = i;
        }
        require(root != ImageResource::NoIndirectImage, "bindless: no table root");
        for (std::uint32_t i = direct; i < capture.specialization.images.size(); i++) require(capture.specialization.images[i].indirectRoot == root, "bindless: an extra image is not the root's slot");
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

    AgcDriver::ShaderMemory memory({});
    const auto capture = memory.Capture(request);
    const auto root = tableRoot(*capture, direct);
    require(capture->snapshot.images[root].dwords == heap[0] && capture->snapshot.images[direct].dwords == heap[1] && capture->snapshot.images[direct + 1u].dwords == heap[3], "bindless: the slots do not hold the keyed entries");
    for (std::uint32_t i = direct + 2u; i < capture->snapshot.images.size(); i++) require(capture->snapshot.images[i].dwords == heap[0], "bindless: a pad slot is not a copy of slot 0");
    const auto mapping = mappingOf(capture->snapshot);
    require(std::vector<std::uint32_t>(mapping.begin(), mapping.begin() + 7) == std::vector<std::uint32_t>{3u, 0u, 0u, 1u, 1u, 3u, 2u}, "bindless: the (key, slot) mapping is wrong");
    require(capture->specialization.images[root].indirectMappingOffset + mapping.size() == capture->snapshot.flattenedSrt.size(), "bindless: the mapping offset does not name the block");
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
    require(uniform.dynamicIndexing && !uniform.switched, "bindless: the SPIR-V does not index the image array dynamically");
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
    const std::array<std::uint32_t, 3> indexingCapabilities{29u, 5301u, 5307u};
    const std::array<std::string_view, 1> indexingExtensions{"SPV_EXT_descriptor_indexing"};
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
    require(nullCapture->snapshot.images[direct + 1u].dwords == heap[0], "bindless: a null entry's slot is not the pad");
    materials[2][1] = 3u;

    // Mode T: every entry keeps its slot; the null entry's slot holds the pad and its key is
    // left out of the mapping.
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
    require(wholeCapture->snapshot.images[wholeRoot].dwords == heap[0] && wholeCapture->snapshot.images[wholeDirect].dwords == heap[1] && wholeCapture->snapshot.images[wholeDirect + 1u].dwords == heap[0] && wholeCapture->snapshot.images[wholeDirect + 2u].dwords == heap[3], "bindless: mode T slots are wrong");
    whole.context.memory = wholeMemory.Regions();
    require(!Recompile(whole, *wholeCapture)->spirv.empty(), "bindless: mode T did not compile");

    // A table wider than the slots without a material pattern is rejected.
    fillSrt(100u);
    AgcDriver::ShaderMemory wideMemory({});
    expectFailure([&] { static_cast<void>(wideMemory.Capture(whole)); }, "bindless image table has 100 entries", "bindless: a wide table was bound");
    fillSrt(4u);
}

}

int main() {
    try {
        using namespace ShaderRecompiler;
        verifyRegisterSources();
        verifyPureFlatSlots();
        verifyBindlessTable();
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
        std::uint32_t payload = 0x3f800000u;
        std::uint64_t table = reinterpret_cast<std::uintptr_t>(&payload);
        const auto address = reinterpret_cast<std::uintptr_t>(&table);
        const std::array<std::uint32_t, 2> userData{static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u)};
        RecompileRequest request{};
        request.shader = {ShaderStage::Vertex, 0x10000u, code, 0, {}};
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
        AgcDriver::ShaderMemory invalid({});
        expectFailure([&] { invalid.Capture(request); }, "null or misaligned address", "null nested pointer was accepted");
        std::cout << "Shader memory capture, strict validation and deterministic replay passed\n";
        return 0;
    } catch (const std::exception& error) {
        const std::string message(error.what());
        std::cerr << message.substr(0, message.find("RecompileRequest:")) << '\n';
        return 1;
    }
}
