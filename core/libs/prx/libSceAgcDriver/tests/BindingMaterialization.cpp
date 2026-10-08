#include "Optimization/DescriptorBindingBuilder.hpp"
#include "ShaderDiskCache.hpp"
#include <algorithm>
#include <cstring>
#include <iostream>
#include <stdexcept>

namespace {
using namespace ShaderRecompiler;
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
template<class TAction> void rejects(TAction&& function) {
    bool failed = false;
    try { function(); } catch (const std::exception&) { failed = true; }
    require(failed, "invalid binding input was accepted");
}
struct Fixture {
    ShaderInfo info;
    ResourceSnapshot snapshot;
    BindingAllocationResult plan;
    DescriptorBindingBuilder builder;
    Fixture(bool push) {
        info.buffers = {{.read = true}, {.written = true}, {.atomic = true}};
        info.images = {{.dimension = RdnaImageDimension::Dim2D, .read = true}, {.dimension = RdnaImageDimension::Dim2D, .atomic = true, .depthCompare = true}};
        info.samplers = {{.forcePointFiltering = true}, {.depthCompare = true}};
        info.images[1].atomic64 = true;
        info.samplers[0].uses = SamplerUseExplicitLod;
        info.sampledPairs = {{0, 0, 0x10}};
        snapshot.buffers = {{{10,11,12,13},4}, {{20,21,22,23},4}, {{30,31,32,33},4}};
        snapshot.images = {{{40,41,42,43,44,45,46,47},8}, {{50,51,52,53,54,55,56,57},8}};
        snapshot.samplers = {{{60,61,0xffffffffu,63},4}, {{70,71,72,73},4}};
        snapshot.userData = {100,101,102,103};
        snapshot.flattenedSrt = {200,201,202};
        plan.layout.userDataRegisters = {19,16};
        plan.layout.memoryOffsetDword = 2;
        plan.layout.memoryOffsetCount = 4;
        plan.layout.dispatchThreadLimit = true;
        plan.layout.descriptors = {{DescriptorBindingKind::Buffers,{2,0,1}}, {static_cast<DescriptorBindingKind>(FirstImageBinding),{1,0}}, {DescriptorBindingKind::Samplers,{1,0}}, {DescriptorBindingKind::FlattenedSrt,{}}, {DescriptorBindingKind::Gds,{}}, {DescriptorBindingKind::BdaPagetable,{}}, {DescriptorBindingKind::FaultBuffer,{}}};
        if (push) plan.layout.pushDataStartDword = 4;
        else plan.layout.descriptors.push_back({DescriptorBindingKind::ShaderData,{}});
        builder.Prepare(plan, info, IrShaderStage::Pixel);
    }
    MaterializedBindings Materialize() const { return builder.Materialize(plan, info, 16, snapshot, {5,6,7}); }
};
void materialization(bool push) {
    Fixture f(push);
    for (const auto& binding : f.plan.bindings) require(binding.guestDescriptor.empty(), "reflection retains live descriptor words");
    require(f.plan.pushConstants.empty(), "reflection retains push data");
    auto a = f.Materialize();
    require(a.bindings[0].Usage().bufferAtomic == std::vector<bool>{true,false,false}, "buffer atomic metadata changed");
    require(a.bindings[0].Usage().bufferWritten == std::vector<bool>{true,false,true}, "buffer write proof changed");
    require(a.bindings[1].Usage().imageWritten == std::vector<bool>{true,false}, "image atomic write proof changed");
    require(a.bindings[1].Usage().imageDepthCompare == std::vector<bool>{true,false}, "image comparison metadata changed");
    require(a.bindings[1].Usage().imageAtomic == std::vector<bool>{true,false}, "image atomic metadata changed");
    require(a.bindings[1].Usage().imageAtomic64 == std::vector<bool>{true,false}, "64-bit image atomic metadata changed");
    require(a.bindings[1].Usage().imageSamplers == std::vector<std::uint32_t>{0,2}, "image-sampler mapping ignored binding order");
    require(a.bindings[1].imageShape == DescriptorImageShape::Image2D, "image shape changed");
    require(a.bindings[2].Usage().samplerDepthCompare == std::vector<bool>{true,false}, "sampler comparison metadata changed");
    require(a.bindings[0].guestDescriptor == std::vector<std::uint32_t>{30,31,32,33,10,11,12,13,20,21,22,23}, "buffer resource ordering changed");
    require(a.bindings[1].guestDescriptor == std::vector<std::uint32_t>{50,51,52,53,54,55,56,57,40,41,42,43,44,45,46,47}, "image resource ordering changed");
    require(a.bindings[2].guestDescriptor == std::vector<std::uint32_t>{70,71,72,73,60,61,0xf50fffffu,63}, "point filtering or sampler ordering changed");
    require(a.bindings[3].guestDescriptor == f.snapshot.flattenedSrt, "flattened SRT changed");
    const std::vector<std::uint32_t> expectedData{103,100,0,5,6,7};
    if (push) {
        require(a.pushConstants.size() == expectedData.size() * 4, "push data size changed");
        require(std::memcmp(a.pushConstants.data(), expectedData.data(), a.pushConstants.size()) == 0, "push data contents changed");
    } else {
        require(a.pushConstants.empty(), "buffer shader data also pushed");
        require(a.bindings.back().guestDescriptor == expectedData, "buffer shader data changed");
    }
    for (std::size_t i = 4; i < 7; ++i) require(a.bindings[i].guestDescriptor.empty(), "synthetic binding has guest descriptor words");
    f.snapshot.buffers[2].dwords[0] = 999;
    f.snapshot.userData[3] = 888;
    f.snapshot.flattenedSrt[0] = 777;
    f.snapshot.samplers[0].dwords[2] = 0;
    auto b = f.Materialize();
    require(b.bindings[0].guestDescriptor[0] == 999 && a.bindings[0].guestDescriptor[0] == 30, "draw descriptors alias mutable storage");
    require(b.bindings[3].guestDescriptor[0] == 777 && a.bindings[3].guestDescriptor[0] == 200, "flattened SRT snapshots alias");
    require(b.bindings[2].guestDescriptor[6] == 0x01000000u, "point filtering adds a mip filter without mipmaps");
    for (std::size_t i = 0; i < 3; ++i) require(a.bindings[i].usage == b.bindings[i].usage && a.bindings[i].usage == f.plan.bindings[i].usage, "immutable reflection was rebuilt");
    f.plan.bindings.clear();
    require(a.bindings[0].Usage().bufferWritten == std::vector<bool>{true,false,true}, "retained draw lost reflection owner");
    rejects([&] { (void)f.Materialize(); });
}
void changingSamplerCoordinates() {
    Fixture f(true);
    auto normalized = f.Materialize();
    f.snapshot.samplers[0].dwords[0] |= 1u << 15u;
    auto unnormalized = f.Materialize();
    require(normalized.bindings[1].imageUnnormalized == std::vector<bool>{false,false}, "retained image coordinate metadata changed");
    require(normalized.bindings[2].samplerUnnormalized == std::vector<bool>{false,false}, "retained sampler coordinate metadata changed");
    require(unnormalized.bindings[1].imageUnnormalized == std::vector<bool>{false,true}, "live image coordinate metadata not materialized");
    require(unnormalized.bindings[2].samplerUnnormalized == std::vector<bool>{false,true}, "live sampler coordinate metadata not materialized");
    require(normalized.bindings[1].usage == unnormalized.bindings[1].usage && normalized.bindings[2].usage == unnormalized.bindings[2].usage, "sampler change rebuilt immutable metadata");
    f.builder.Populate(f.plan, f.info, IrShaderStage::Pixel, 16, f.snapshot, {5,6,7});
    auto repeated = f.Materialize();
    require(repeated.bindings[1].imageUnnormalized == unnormalized.bindings[1].imageUnnormalized && repeated.bindings[2].samplerUnnormalized == unnormalized.bindings[2].samplerUnnormalized, "rematerialization appended coordinate metadata");
    f.snapshot.samplers[1].dwords[0] |= 1u << 15u;
    rejects([&] { (void)f.Materialize(); });
}
void cacheRoundTrip() {
    Fixture f(true);
    CompiledVariant variant;
    variant.bindings = f.plan;
    variant.info.info = f.info;
    variant.info.stage = IrShaderStage::Pixel;
    variant.info.userDataBase = 16;
    const std::array<std::byte, 1> key{std::byte{42}};
    const auto bytes = ShaderDiskCache::EncodeEntry(key, variant);
    CompiledVariant decoded;
    require(ShaderDiskCache::DecodeEntry(bytes, key, decoded) == ShaderDiskCache::LoadStatus::Loaded, "prepared reflection cache entry rejected");
    auto result = f.builder.Materialize(decoded.bindings, decoded.info.info, decoded.info.userDataBase, f.snapshot, {5,6,7});
    auto expected = f.Materialize();
    require(result.pushConstants == expected.pushConstants, "cached reflection changed push data");
    require(result.bindings.size() == expected.bindings.size(), "cached reflection changed binding count");
    for (std::size_t i = 0; i < result.bindings.size(); ++i) {
        const auto& a = result.bindings[i];
        const auto& b = expected.bindings[i];
        require(a.guestDescriptor == b.guestDescriptor && a.kind == b.kind && a.role == b.role && a.binding == b.binding && a.count == b.count && a.imageShape == b.imageShape, "cached reflection changed descriptors");
        require(a.Usage().bufferWritten == b.Usage().bufferWritten && a.Usage().bufferAtomic == b.Usage().bufferAtomic && a.Usage().imageWritten == b.Usage().imageWritten && a.Usage().imageAtomic == b.Usage().imageAtomic && a.Usage().imageDepthCompare == b.Usage().imageDepthCompare && a.Usage().samplerDepthCompare == b.Usage().samplerDepthCompare, "cached reflection changed access metadata");
        require(decoded.bindings.bindings[i].guestDescriptor.empty(), "cached reflection retained live words");
        require(a.Usage().imageAtomic64 == b.Usage().imageAtomic64 && a.Usage().imageSamplers == b.Usage().imageSamplers && a.samplerUnnormalized == b.samplerUnnormalized && a.imageUnnormalized == b.imageUnnormalized, "cached reflection lost merged image metadata");
    }
}
void malformed() {
    Fixture f(true);
    f.snapshot.buffers[0].dwordCount = 3;
    rejects([&] { (void)f.Materialize(); });
    f.snapshot.buffers[0].dwordCount = 4;
    f.snapshot.images[0].dwordCount = 4;
    rejects([&] { (void)f.Materialize(); });
    f.snapshot.images[0].dwordCount = 8;
    f.snapshot.flattenedSrt.clear();
    rejects([&] { (void)f.Materialize(); });
    f.snapshot.flattenedSrt = {0};
    rejects([&] { (void)f.builder.Materialize(f.plan, f.info, 16, f.snapshot, {}); });
    f.snapshot.userData.clear();
    rejects([&] { (void)f.Materialize(); });
    f.plan.layout.descriptors.push_back({DescriptorBindingKind::ShaderData,{}});
    rejects([&] { f.builder.Prepare(f.plan, f.info, IrShaderStage::Pixel); });
    DescriptorBinding unproved{};
    require(unproved.Usage().bufferWritten.empty(), "missing access metadata invents a write proof");
}
}
int main() {
    try {
        materialization(true);
        materialization(false);
        malformed();
        cacheRoundTrip();
        changingSamplerCoordinates();
        std::cout << "binding materialization tests passed\n";
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
}
