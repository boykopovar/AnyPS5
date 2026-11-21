#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWRECORDS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWRECORDS_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRecordReport.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/Recipe.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libc/include/HostThreadSlot.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace AgcDriver::DriverDetail {

struct LeanShape;

struct DrawRecordKey {
    std::uint64_t shape = 0;
    std::uint64_t indexAddress = 0;
    std::uint32_t indexCount = 0;
    std::uint32_t indexSize = 0;
    std::uint32_t instanceCount = 0;
    std::uint32_t flags = 0;
    std::uint32_t firstVertex = 0;
    std::uint32_t firstInstance = 0;
    bool indexed = false;

    static DrawRecordKey For(std::uint64_t shape, const Pm4::DrawParameters& parameters);
    std::uint64_t Probe() const;
    bool operator==(const DrawRecordKey&) const = default;
};

using DrawRecordHash = std::array<std::uint64_t, 2>;

DrawRecordHash HashDrawRecordWords(std::span<const std::uint32_t> first, std::span<const std::uint32_t> second);

struct DrawRecordStageView {
    std::uint64_t variant = 0;
    std::uint32_t stage = 0;
    std::uint32_t pushOffset = 0;
    std::vector<std::uint32_t> bindings;
    std::vector<std::uint32_t> vertexAttributes;
    std::vector<ShaderRecompiler::RegisteredImageView> views;
};

struct DrawRecordView {
    std::vector<DrawRecordStageView> stages;
    std::array<std::byte, Graphics::PipelinePushConstantBytes> push{};
    bool pushed = false;
    std::array<std::uint64_t, 6> parameters{};
    std::vector<DrawPipeline::Range> reads;
    std::vector<DrawPipeline::Range> writes;
    std::vector<std::uint32_t> contentKey;
    std::optional<std::uint32_t> fragmentOutputs;

    void Capture(std::span<const Graphics::CompiledShader> shaders, const Pm4::DrawParameters& drawParameters);
};

struct DrawRecordSink {
    bool queued = false;
    bool verify = false;
    std::uint64_t shape = 0;
    std::vector<std::uint32_t> contentKey;
    std::vector<std::uint32_t> vertexLayout;
    std::vector<std::uint32_t> vertexInputs;
    std::optional<std::uint32_t> fragmentOutputs;
    std::vector<DrawPipeline::Range> writes;
    DrawRecordView today;

    void Reset();
    void NoteVertices(std::span<const std::optional<ShaderRecompiler::ShaderVertexStageInfo>> infos);
    DrawRecordHash Content() const { return HashDrawRecordWords(contentKey, vertexLayout); }
    DrawRecordHash Inputs() const { return HashDrawRecordWords(vertexInputs, {}); }
};

void DrawVertexLayout(std::span<const std::optional<ShaderRecompiler::ShaderVertexStageInfo>> infos, std::vector<std::uint32_t>& layout, std::vector<std::uint32_t>& inputs);

struct DrawRecordSinkTag {};
using DrawRecordSinkSlot = HostThreadSlot<DrawRecordSink*, DrawRecordSinkTag>;

struct DrawRecord : std::enable_shared_from_this<DrawRecord> {
    DrawRecordKey key;
    std::uint64_t probe = 0;
    std::shared_ptr<const LeanShape> shape;
    std::uint64_t deviceSerial = 0;
    std::uint64_t builtPresent = 0;
    DrawRecordHash content{};
    DrawRecordHash inputs{};
    std::vector<std::uint32_t> contentKey;
    std::vector<std::uint32_t> vertexLayout;
    std::vector<std::uint32_t> vertexInputs;
    std::optional<std::uint32_t> fragmentOutputs;
    std::vector<DrawPipeline::Range> writes;
    std::shared_ptr<DrawRecordCommit> commit = std::make_shared<DrawRecordCommit>();

    static std::shared_ptr<DrawRecord> Build(const DrawRecordKey& key, std::shared_ptr<const LeanShape> shape, std::uint64_t deviceSerial, std::uint64_t present, const DrawRecordSink& sink);
    std::size_t Bytes() const;
};

struct DrawRecordHit {
    std::shared_ptr<const DrawRecord> record;
    std::vector<ShaderRecompiler::StageSideWords> sides;
    std::vector<Graphics::CompiledShader> stages;
    Pm4::DrawParameters parameters{};
    std::vector<DrawPipeline::Range> reads;
    std::vector<Graphics::GuestMemorySnapshot> snapshots;
    bool direct = false;
    std::shared_ptr<const void> owner;
    std::vector<std::shared_ptr<const ShaderRecompiler::ResourceCapture>> captures;
    std::optional<Graphics::ShaderResources::PreparedBuffers> preparedBuffers;
    std::optional<Graphics::PreparedInputs> preparedInputs;
};

namespace DrawRecordFlags {

constexpr std::uint16_t VerifyDue = 8;
constexpr std::uint16_t Direct = 64;
constexpr std::uint16_t DirectBuffers = 128;
constexpr std::uint16_t DirectInputs = 256;

}

}

#endif
