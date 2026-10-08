#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRegisterKey.hpp"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

namespace AgcDriver::DriverDetail {

namespace {

bool runtimeRegister(std::uint32_t offset) {
    return offset - 0x00cu < 32u || offset - 0x08cu < 32u || offset - 0x10cu < 32u || offset == 0x082u || offset == 0x083u || offset == 0x102u || offset == 0x103u;
}

const auto& registerMasks() {
    static const auto masks = [] {
        std::array<std::vector<std::uint64_t>, 3> bits;
        for (const auto& range : Graphics::DrawKeyRegisters) {
            auto& bank = bits[static_cast<std::size_t>(range.bank)];
            bank.resize(std::max(bank.size(), (static_cast<std::size_t>(range.first) + range.count + 63) / 64));
            for (auto offset = range.first; offset < range.first + range.count; ++offset) {
                if (range.bank == Graphics::RegisterBank::Shader && runtimeRegister(offset)) continue;
                bank[offset / 64] |= std::uint64_t{1} << (offset % 64);
            }
        }
        std::array<std::shared_ptr<const Registers::Mask>, 3> result;
        for (std::size_t i = 0; i < bits.size(); ++i) result[i] = std::make_shared<const Registers::Mask>(std::move(bits[i]));
        return result;
    }();
    return masks;
}

}

DrawRegisterStateKey RegisterStateKey(const QueueState& queue, bool allUserWords, bool incremental) {
    const auto& masks = registerMasks();
    const auto fingerprint = [&](const Registers& registers, Graphics::RegisterBank bank) {
        const auto& mask = masks[static_cast<std::size_t>(bank)];
        return incremental ? registers.Fingerprint(mask) : registers.RecomputeFingerprint(*mask);
    };
    auto shape = (0xcbf29ce484222325ull ^ fingerprint(queue.context, Graphics::RegisterBank::Context)) * 0x100000001b3ull;
    shape = (shape ^ fingerprint(queue.shader, Graphics::RegisterBank::Shader)) * 0x100000001b3ull;
    shape = (shape ^ fingerprint(queue.userConfig, Graphics::RegisterBank::UserConfig)) * 0x100000001b3ull;
    auto exact = shape;
    const auto words = [&](std::uint32_t base, std::uint32_t count) {
        for (auto it = queue.shader.lower_bound(base); it != queue.shader.end() && it->first < base + count; ++it) {
            exact = (exact ^ it->first) * 0x100000001b3ull;
            exact = (exact ^ it->second) * 0x100000001b3ull;
        }
    };
    for (const auto base : {0x00cu, 0x08cu, 0x10cu}) {
        const auto resources = queue.shader.find(base - 1);
        const auto count = resources == queue.shader.end() ? 0u : ((resources->second >> 1u) & 0x1fu) | (((resources->second >> 27u) & 1u) << 5u);
        words(base, allUserWords ? 32u : std::min(count, 32u));
    }
    words(0x082u, 2);
    words(0x102u, 2);
    return {exact, shape};
}

std::uint64_t Driver::drawRegisterKey(const QueueState& queue, const ShaderRegistry& registry, std::uint64_t deviceSerial, std::uint64_t* shape) {
    static const bool allUserWords = std::getenv("APS5_DRAW_KEY_ALL_USER_WORDS") != nullptr;
    static const bool verify = std::getenv("APS5_VERIFY_REGISTER_KEYS") != nullptr;
    const auto state = RegisterStateKey(queue, allUserWords);
    if (verify && state != RegisterStateKey(queue, allUserWords, false)) throw std::runtime_error("incremental register fingerprint disagrees with full state");
    auto key = state.exact;
    auto shapeKey = state.shape;
    const auto mix = [&](std::uint64_t value) {
        key = (key ^ value) * 0x100000001b3ull;
        shapeKey = (shapeKey ^ value) * 0x100000001b3ull;
    };
    mix(deviceSerial);
    for (const auto base : {0x008u, 0x088u, 0x0c8u, 0x108u, 0x148u}) {
        const auto low = queue.shader.find(base);
        const auto high = queue.shader.find(base + 1);
        if (low == queue.shader.end() || high == queue.shader.end()) {
            mix(0);
            continue;
        }
        const auto address = (static_cast<std::uint64_t>(low->second) << 8u) | (static_cast<std::uint64_t>(high->second & 0xffu) << 40u);
        auto it = registry.upper_bound(address);
        if (it == registry.begin()) {
            mix(1);
            continue;
        }
        --it;
        mix(reinterpret_cast<std::uintptr_t>(it->second.get()));
        mix(address - it->second->codeAddress);
    }
    if (shape != nullptr) *shape = shapeKey;
    return key;
}

bool Driver::sameVertexInfo(const ShaderRecompiler::ShaderVertexStageInfo& a, const ShaderRecompiler::ShaderVertexStageInfo& b) {
    if (a.resourcesNum != b.resourcesNum || a.fetchAttribReg != b.fetchAttribReg || a.fetchBufferReg != b.fetchBufferReg || a.fetchEmbedded != b.fetchEmbedded) return false;
    for (std::uint32_t i = 0; i < a.resourcesNum && i < a.resources.size(); ++i) {
        if (a.resources[i].fields != b.resources[i].fields) return false;
        const auto& x = a.resourcesDst[i];
        const auto& y = b.resourcesDst[i];
        if (x.registerStart != y.registerStart || x.registersNum != y.registersNum || x.attrId != y.attrId || x.fetchIndex != y.fetchIndex) return false;
    }
    return true;
}

bool Driver::samePlan(const DrawPlan& a, const DrawPlan& b) {
    const auto& s = a.state;
    const auto& t = b.state;
    const auto sameColor = [](const Graphics::ColorTarget& x, const Graphics::ColorTarget& y) {
        return x.address == y.address && x.extent.width == y.extent.width && x.extent.height == y.extent.height && x.format == y.format && x.bytes == y.bytes && x.componentMapping == y.componentMapping && x.tileMode == y.tileMode && x.elementBytes == y.elementBytes && x.dccAddress == y.dccAddress && x.dccAlphaOnMsb == y.dccAlphaOnMsb && x.slot == y.slot && x.exportIndex == y.exportIndex;
    };
    const auto sameBlend = [](const VkPipelineColorBlendAttachmentState& x, const VkPipelineColorBlendAttachmentState& y) {
        return x.blendEnable == y.blendEnable && x.srcColorBlendFactor == y.srcColorBlendFactor && x.dstColorBlendFactor == y.dstColorBlendFactor && x.colorBlendOp == y.colorBlendOp && x.srcAlphaBlendFactor == y.srcAlphaBlendFactor && x.dstAlphaBlendFactor == y.dstAlphaBlendFactor && x.alphaBlendOp == y.alphaBlendOp && x.colorWriteMask == y.colorWriteMask;
    };
    const auto sameMesh = [](const std::optional<ShaderRecompiler::MeshConfiguration>& x, const std::optional<ShaderRecompiler::MeshConfiguration>& y) {
        if (x.has_value() != y.has_value()) return false;
        if (!x) return true;
        return x->inputPrimitive == y->inputPrimitive && x->primitivesPerGroup == y->primitivesPerGroup && x->verticesPerGroup == y->verticesPerGroup && x->maxVertices == y->maxVertices && x->maxPrimitives == y->maxPrimitives && x->threadsPerGroup == y->threadsPerGroup && x->ldsSizeDwords == y->ldsSizeDwords && x->provokingVertex == y->provokingVertex && x->esgsItemSize == y->esgsItemSize;
    };
    const auto sameTess = [](const std::optional<ShaderRecompiler::TessellationConfiguration>& x, const std::optional<ShaderRecompiler::TessellationConfiguration>& y) {
        if (x.has_value() != y.has_value()) return false;
        if (!x) return true;
        return x->inputControlPoints == y->inputControlPoints && x->outputControlPoints == y->outputControlPoints && x->domain == y->domain && x->partitioning == y->partitioning && x->outputTopology == y->outputTopology;
    };
    if (s.stages.path != t.stages.path || s.stages.registerValue != t.stages.registerValue || s.stages.vertexWaveSize != t.stages.vertexWaveSize || s.stages.fragmentWaveSize != t.stages.fragmentWaveSize || !sameMesh(s.stages.mesh, t.stages.mesh) || !sameTess(s.stages.tessellation, t.stages.tessellation)) return false;
    if (!sameColor(s.color, t.color) || s.colors.size() != t.colors.size() || s.blends.size() != t.blends.size()) return false;
    for (std::size_t i = 0; i < s.colors.size(); ++i) {
        if (!sameColor(s.colors[i], t.colors[i])) return false;
    }
    for (std::size_t i = 0; i < s.blends.size(); ++i) {
        if (!sameBlend(s.blends[i], t.blends[i])) return false;
    }
    if (s.hasColorTarget != t.hasColorTarget || s.rectList != t.rectList || s.renderExtent.width != t.renderExtent.width || s.renderExtent.height != t.renderExtent.height || s.topology != t.topology || s.negativeOneToOne != t.negativeOneToOne || s.depthClamp != t.depthClamp || s.cullMode != t.cullMode || s.frontFace != t.frontFace || !sameBlend(s.blend, t.blend) || s.blendConstants != t.blendConstants) return false;
    if (std::memcmp(&s.viewport, &t.viewport, sizeof(VkViewport)) != 0 || std::memcmp(&s.scissor, &t.scissor, sizeof(VkRect2D)) != 0) return false;
    const auto& p = a.pixel;
    const auto& q = b.pixel;
    if (p.interpolatorCount != q.interpolatorCount || p.interpolatorSettings != q.interpolatorSettings || p.wave32 != q.wave32 || p.inputAddr != q.inputAddr || p.hasPerspectiveCenterVgpr != q.hasPerspectiveCenterVgpr || p.perspectiveCentroid != q.perspectiveCentroid || p.posX != q.posX || p.posY != q.posY || p.posZ != q.posZ || p.posW != q.posW || p.frontFace != q.frontFace || p.ancillary != q.ancillary || p.sampleShading != q.sampleShading || p.noPerspective != q.noPerspective || p.linearCentroid != q.linearCentroid || p.pixelKillEnable != q.pixelKillEnable || p.depthExportEnable != q.depthExportEnable || p.sampleMaskExportEnable != q.sampleMaskExportEnable || p.earlyZ != q.earlyZ || p.executeOnNoop != q.executeOnNoop || p.conservativeZExport != q.conservativeZExport || p.targetOutputMode != q.targetOutputMode || p.targetExportMapping != q.targetExportMapping) return false;
    if (a.roles != b.roles || a.programs.size() != b.programs.size()) return false;
    for (std::size_t i = 0; i < a.programs.size(); ++i) {
        const auto& x = a.programs[i];
        const auto& y = b.programs[i];
        if (x.binary.stage != y.binary.stage || x.binary.codeAddress != y.binary.codeAddress || x.userDataBase != y.userDataBase || x.firstUserSgpr != y.firstUserSgpr || x.snapshot != y.snapshot || x.codeOffset != y.codeOffset || x.resourceRegister != y.resourceRegister || x.nullPixel != y.nullPixel || x.merged != y.merged || x.mergedPointer != y.mergedPointer || x.mergedPointerRequired != y.mergedPointerRequired) return false;
    }
    return true;
}

}
