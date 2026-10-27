#include "Translation/ShaderInputInfoBuilder.hpp"
#include "prx/libc/include/HostThreadLocal.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {
IrShaderStage _toIrShaderStage(ShaderStageKind stage) {
    switch (stage) {
    case ShaderStageKind::Vertex: return IrShaderStage::Vertex;
    case ShaderStageKind::Local: return IrShaderStage::Local;
    case ShaderStageKind::TessellationControl: return IrShaderStage::TessellationControl;
    case ShaderStageKind::TessellationEvaluation: return IrShaderStage::TessellationEvaluation;
    case ShaderStageKind::Mesh: return IrShaderStage::Mesh;
    default: throw std::runtime_error("ShaderInputInfoBuilder: unexpected vertex-family stage");
    }
}

void _detectVertexBuffers(ShaderVertexInputInfo& info) {
    info.buffersNum = 0;
    for (int ri = 0; ri < info.resourcesNum; ++ri) {
        const auto& r = info.resources[ri];
        const std::uint16_t stride = static_cast<std::uint16_t>((r.fields[1] >> 16u) & 0x3fffu);
        const std::uint64_t base = (static_cast<std::uint64_t>(r.fields[0]) | (static_cast<std::uint64_t>(r.fields[1]) << 32u)) & 0xffffffffffffull;
        const std::uint32_t numRecords = r.fields[2];
        bool merged = false;
        for (int bi = 0; bi < info.buffersNum; ++bi) {
            auto& b = info.buffers[bi];
            if (b.stride != stride || b.fetchIndex != static_cast<std::uint32_t>(info.resourcesDst[ri].fetchIndex)) continue;
            const auto low = base < b.addr ? base : b.addr;
            const auto offset1 = base - low;
            const auto offset2 = b.addr - low;
            if (offset1 >= stride || offset2 >= stride) continue;
            if (b.numRecords != numRecords) throw std::runtime_error("ShaderInputInfoBuilder: merged vertex buffers disagree on record count");
            b.addr = low;
            if (b.attrNum >= ShaderVertexInputBuffer::MaxAttributes) throw std::runtime_error("ShaderInputInfoBuilder: vertex buffer attribute count exceeds the supported domain");
            b.attrIndices[b.attrNum++] = ri;
            merged = true;
            break;
        }
        if (merged) continue;
        if (info.buffersNum >= ShaderVertexInputInfo::MaxResources) throw std::runtime_error("ShaderInputInfoBuilder: vertex buffer count exceeds the supported domain");
        auto& b = info.buffers[info.buffersNum++];
        b.addr = base;
        b.stride = stride;
        b.numRecords = numRecords;
        b.fetchIndex = static_cast<std::uint32_t>(info.resourcesDst[ri].fetchIndex);
        b.attrNum = 1;
        b.attrIndices[0] = ri;
    }
    for (int bi = 0; bi < info.buffersNum; ++bi) {
        auto& b = info.buffers[bi];
        for (int ri = 0; ri < b.attrNum; ++ri) {
            const auto& r = info.resources[b.attrIndices[ri]];
            const std::uint64_t base = (static_cast<std::uint64_t>(r.fields[0]) | (static_cast<std::uint64_t>(r.fields[1]) << 32u)) & 0xffffffffffffull;
            b.attrOffsets[ri] = static_cast<std::uint32_t>(base - b.addr);
        }
    }
}

void _validateTessellation(const TessellationConfiguration* tessellation) {
    if (tessellation == nullptr) throw std::runtime_error("ShaderInputInfoBuilder: tessellation configuration is missing");
    if (tessellation->inputControlPoints == 0u || tessellation->inputControlPoints > 32u || tessellation->outputControlPoints == 0u || tessellation->outputControlPoints > 32u) throw std::runtime_error("ShaderInputInfoBuilder: invalid tessellation control-point counts");
    if ((tessellation->domain != 1u && tessellation->domain != 2u) || (tessellation->partitioning != 0u && tessellation->partitioning != 2u && tessellation->partitioning != 3u) || (tessellation->outputTopology != 2u && tessellation->outputTopology != 3u)) throw std::runtime_error("ShaderInputInfoBuilder: unsupported tessellation configuration");
    if (tessellation->patchesPerGroup == 0u || tessellation->patchesPerGroup > 256u) throw std::runtime_error("ShaderInputInfoBuilder: invalid tessellation patches per group");
}

void _validateMesh(const MeshConfiguration* mesh) {
    if (mesh == nullptr) throw std::runtime_error("ShaderInputInfoBuilder: a mesh-stage program has no mesh configuration");
    if (mesh->threadsPerGroup == 0u || mesh->maxVertices == 0u || mesh->maxPrimitives == 0u || (mesh->outputPrimitiveSize != 1u && mesh->outputPrimitiveSize != 2u && mesh->outputPrimitiveSize != 3u) || mesh->provokingVertex >= mesh->outputPrimitiveSize) throw std::runtime_error("ShaderInputInfoBuilder: invalid mesh configuration");
}

}

ShaderStageInputInfo BuildShaderStageInputInfo(ShaderStageKind stage, const GuestContext& context, std::uint32_t hostSubgroupSize, const MeshConfiguration* mesh, const TessellationConfiguration* tessellation) {
    switch (stage) {
    case ShaderStageKind::Compute: {
        if (!context.compute.has_value()) {
            throw std::runtime_error("ShaderInputInfoBuilder: GuestContext.compute is not set");
        }
        const auto& compute = *context.compute;
        struct ComputeStorage {};
        auto& computeStorage = HostThreadLocal<ShaderComputeInputInfo, ComputeStorage>();
        computeStorage = ShaderComputeInputInfo{};
        computeStorage.threadsNum[0] = compute.numThreads[0];
        computeStorage.threadsNum[1] = compute.numThreads[1];
        computeStorage.threadsNum[2] = compute.numThreads[2];
        computeStorage.ldsSizeDwords = compute.ldsSizeDwords;
        computeStorage.waveSize = context.waveSize;
        computeStorage.hostSubgroupSize = hostSubgroupSize;
        computeStorage.groupId[0] = compute.groupIdEnable[0];
        computeStorage.groupId[1] = compute.groupIdEnable[1];
        computeStorage.groupId[2] = compute.groupIdEnable[2];
        computeStorage.tgSizeEn = compute.tgSizeEnable;
        computeStorage.scratchSizeDwords = compute.scratchDwords;
        computeStorage.threadIdsNum = static_cast<int>(compute.threadIdComponentCount);
        computeStorage.partialGroups = compute.PartialGroups();
        // Workgroup ids (and the thread-group size word) follow the user SGPRs.
        computeStorage.workgroupRegister = static_cast<int>(context.userDataBaseRegister + context.userData.size());
        ShaderStageInputInfo result;
        result.compute = &computeStorage;
        return result;
    }
    case ShaderStageKind::Pixel: {
        if (!context.pixel.has_value()) {
            throw std::runtime_error("ShaderInputInfoBuilder: GuestContext.pixel is not set");
        }
        const auto& pixel = *context.pixel;
        struct PixelStorage {};
        auto& pixelStorage = HostThreadLocal<ShaderPixelInputInfo, PixelStorage>();
        pixelStorage = ShaderPixelInputInfo{};
        for (std::uint32_t i = 0; i < 32; ++i) {
            pixelStorage.interpolatorSettings[i] = pixel.interpolatorSettings[i];
        }
        pixelStorage.inputNum = pixel.interpolatorCount;
        const auto place = [&](PixelInput input, bool loaded) {
            if (!loaded) return;
            if ((pixel.inputAddr & PixelInputBit(input)) == 0u) {
                throw std::runtime_error("ShaderInputInfoBuilder: a loaded pixel input is missing from SPI_PS_INPUT_ADDR");
            }
            pixelStorage.psInputVgpr[static_cast<std::uint32_t>(input)] = PixelInputVgpr(pixel.inputAddr, input);
        };
        place(PixelInput::PerspectiveSample, (pixel.inputAddr & PixelInputBit(PixelInput::PerspectiveSample)) != 0u);
        place(PixelInput::PerspectiveCenter, pixel.hasPerspectiveCenterVgpr);
        place(PixelInput::PerspectiveCentroid, pixel.perspectiveCentroid);
        place(PixelInput::LinearSample, (pixel.inputAddr & PixelInputBit(PixelInput::LinearSample)) != 0u);
        place(PixelInput::LinearCenter, pixel.noPerspective);
        place(PixelInput::LinearCentroid, pixel.linearCentroid);
        place(PixelInput::PositionX, pixel.posX);
        place(PixelInput::PositionY, pixel.posY);
        place(PixelInput::PositionZ, pixel.posZ);
        place(PixelInput::PositionW, pixel.posW);
        place(PixelInput::FrontFace, pixel.frontFace);
        place(PixelInput::Ancillary, pixel.ancillary);
        place(PixelInput::SampleCoverage, (pixel.inputAddr & PixelInputBit(PixelInput::SampleCoverage)) != 0u);
        place(PixelInput::LineStipple, (pixel.inputAddr & PixelInputBit(PixelInput::LineStipple)) != 0u);
        place(PixelInput::PositionFixedPoint, (pixel.inputAddr & PixelInputBit(PixelInput::PositionFixedPoint)) != 0u);
        for (std::uint32_t i = 0; i < 8; ++i) {
            pixelStorage.targetOutputMode[i] = pixel.targetOutputMode[i];
        }
        pixelStorage.psPosX = pixel.posX;
        pixelStorage.psPosY = pixel.posY;
        pixelStorage.psPosZ = pixel.posZ;
        pixelStorage.psPosW = pixel.posW;
        pixelStorage.psFrontFace = pixel.frontFace;
        pixelStorage.psFrontFaceAllBits = pixel.frontFaceAllBits;
        pixelStorage.psAncillary = pixel.ancillary;
        pixelStorage.psNoPerspective = pixel.noPerspective;
        pixelStorage.psPixelKillEnable = pixel.pixelKillEnable;
        pixelStorage.psDepthExportEnable = pixel.depthExportEnable;
        pixelStorage.psSampleMaskExportEnable = pixel.sampleMaskExportEnable;
        pixelStorage.psStencilRefExportEnable = pixel.stencilRefExportEnable;
        pixelStorage.psStencilOperationValueExport = pixel.stencilOperationValueExport;
        pixelStorage.psSampleShading = pixel.sampleShading;
        pixelStorage.psEarlyZ = pixel.earlyZ;
        pixelStorage.psExecuteOnNoop = pixel.executeOnNoop;
        pixelStorage.psConservativeZExport = pixel.conservativeZExport;
        pixelStorage.psOrderedPixelShader = pixel.orderedPixelShader;
        if (pixel.ditheredAlphaToCoverage && !pixel.alphaToCoverage) throw std::runtime_error("dithered alpha-to-coverage requires enabled coverage");
        pixelStorage.psAlphaToCoverage = pixel.alphaToCoverage;
        pixelStorage.psDitheredAlphaToCoverage = pixel.ditheredAlphaToCoverage;
        pixelStorage.psDitheredAlphaSamples = pixel.ditheredAlphaSamples;
        pixelStorage.quadPixelMask = pixel.quadPixelMask;
        pixelStorage.quadSampleMask = pixel.quadSampleMask;
        pixelStorage.psIteratesSamples = pixel.iteratesSamples;
        pixelStorage.psPositionLocation = pixel.positionLocation;
        pixelStorage.sampleLocations = pixel.sampleLocations;
        if (pixel.targetMerge.mask != 0u && (pixel.targetMerge.target >= 8u || (pixel.targetMerge.samples != 1u && pixel.targetMerge.samples != 2u && pixel.targetMerge.samples != 4u && pixel.targetMerge.samples != 8u))) throw std::runtime_error("ShaderInputInfoBuilder: invalid color target merge");
        pixelStorage.mergeTarget = pixel.targetMerge.target;
        pixelStorage.mergeMask = pixel.targetMerge.mask;
        pixelStorage.mergeSamples = pixel.targetMerge.samples;
        pixelStorage.mergeDepthStencilWrites = pixel.targetMerge.depthStencilWrites;
        pixelStorage.mergeDepthStencilTests = pixel.targetMerge.depthStencilTests;
        pixelStorage.sampleOrder = pixel.sampleOrder;
        ShaderStageInputInfo result;
        result.pixel = &pixelStorage;
        return result;
    }
    case ShaderStageKind::Vertex:
    case ShaderStageKind::Local:
    case ShaderStageKind::TessellationControl:
    case ShaderStageKind::TessellationEvaluation:
    case ShaderStageKind::Mesh: {
        if (!context.vertex.has_value()) {
            throw std::runtime_error("ShaderInputInfoBuilder: GuestContext.vertex is not set");
        }
        const auto& vertex = *context.vertex;
        if (vertex.resourcesNum > vertex.resources.size()) throw std::runtime_error("ShaderInputInfoBuilder: invalid vertex resource count");
        struct VertexStorage {};
        auto& vertexStorage = HostThreadLocal<ShaderVertexInputInfo, VertexStorage>();
        vertexStorage = ShaderVertexInputInfo{};
        vertexStorage.logicalStage = _toIrShaderStage(stage);
        vertexStorage.hostSubgroupSize = hostSubgroupSize;
        vertexStorage.fetchEmbedded = vertex.fetchEmbedded;
        vertexStorage.paClVsOutCntl = vertex.paClVsOutCntl;
        vertexStorage.depthRemap = {vertex.depthRemapMode};
        vertexStorage.pointList = vertex.pointList;
        vertexStorage.pointSize = vertex.pointSize;
        vertexStorage.pointSizeMin = vertex.pointSizeMin;
        vertexStorage.pointSizeMax = vertex.pointSizeMax;
        vertexStorage.fetchExternal = false;
        vertexStorage.fetchAttribReg = static_cast<int>(vertex.fetchAttribReg);
        vertexStorage.fetchBufferReg = static_cast<int>(vertex.fetchBufferReg);
        vertexStorage.resourcesNum = static_cast<int>(vertex.resourcesNum);
        for (std::uint32_t i = 0; i < static_cast<std::uint32_t>(vertexStorage.resourcesNum); ++i) {
            vertexStorage.resources[i].fields = vertex.resources[i].fields;
            if (vertex.fetchEmbedded) vertexStorage.resources[i].fields = {0u, 0u, 0u, (static_cast<std::uint32_t>(IrBufferFormat::Format32_32_32_32Float) << 12u) | ShaderImageIdentitySwizzle};
            vertexStorage.resourcesDst[i].registerStart = vertex.resourcesDst[i].registerStart;
            vertexStorage.resourcesDst[i].registersNum = vertex.resourcesDst[i].registersNum;
            vertexStorage.resourcesDst[i].attrId = vertex.resourcesDst[i].attrId;
            vertexStorage.resourcesDst[i].fetchIndex = vertex.fetchEmbedded ? 0u : vertex.resourcesDst[i].fetchIndex;
        }
        _detectVertexBuffers(vertexStorage);
        if (stage == ShaderStageKind::Local || stage == ShaderStageKind::TessellationControl || stage == ShaderStageKind::TessellationEvaluation) {
            _validateTessellation(tessellation);
            auto& target = vertexStorage.tess;
            target.inputControlPoints = tessellation->inputControlPoints;
            target.outputControlPoints = tessellation->outputControlPoints;
            target.domain = tessellation->domain;
            target.partitioning = tessellation->partitioning;
            target.outputTopology = tessellation->outputTopology;
            target.patchesPerGroup = tessellation->patchesPerGroup;
            target.primitiveGroupSize = tessellation->primitiveGroupSize;
            target.breakAtInstance = tessellation->breakAtInstance;
            target.instancePatches = tessellation->instancePatches;
            target.drawParameters = tessellation->drawParameters;
            target.mergedHull = tessellation->mergedHull;
            target.geometryItemDwords = stage == ShaderStageKind::TessellationEvaluation && mesh != nullptr ? mesh->esgsItemSize : 0u;
        }
        if (stage == ShaderStageKind::Mesh) {
            _validateMesh(mesh);
            auto& target = vertexStorage.mesh;
            target.threadsNum[0] = mesh->threadsPerGroup;
            target.threadsNum[1] = 1u;
            target.threadsNum[2] = 1u;
            target.ldsSizeDwords = mesh->ldsSizeDwords;
            target.waveSize = context.waveSize;
            target.hostSubgroupSize = hostSubgroupSize;
            target.inputPrimitive = mesh->inputPrimitive;
            target.primitivesPerGroup = mesh->primitivesPerGroup;
            target.verticesPerGroup = mesh->verticesPerGroup;
            target.maxVertices = mesh->maxVertices;
            target.maxPrimitives = mesh->maxPrimitives;
            target.provokingVertex = mesh->provokingVertex;
            target.esgsItemSize = mesh->esgsItemSize;
            target.outputPrimitiveSize = mesh->outputPrimitiveSize;
            if (target.outputPrimitiveSize == 1u) vertexStorage.pointList = true;
            target.capturedInput = tessellation != nullptr;
        }
        ShaderStageInputInfo result;
        result.vertex = &vertexStorage;
        return result;
    }
    case ShaderStageKind::Unknown:
    case ShaderStageKind::Fetch:
        throw std::runtime_error("ShaderInputInfoBuilder: unexpected stage");
    }
    throw std::runtime_error("ShaderInputInfoBuilder: unexpected stage");
}

void ValidateVertexStageInputs(ShaderStageKind stage, const GuestContext& context, const MeshConfiguration* mesh, const TessellationConfiguration* tessellation) {
    if (!context.vertex.has_value()) {
        throw std::runtime_error("ShaderInputInfoBuilder: GuestContext.vertex is not set");
    }
    const auto& vertex = *context.vertex;
    if (vertex.resourcesNum > vertex.resources.size()) throw std::runtime_error("ShaderInputInfoBuilder: invalid vertex resource count");
    struct Buffer {
        std::uint64_t addr;
        std::uint32_t stride;
        std::uint32_t numRecords;
        std::uint32_t fetchIndex;
        int attrNum;
    };
    std::array<Buffer, ShaderVertexInputInfo::MaxResources> buffers;
    int buffersNum = 0;
    const std::array<std::uint32_t, 4> embedded{};
    for (std::uint32_t ri = 0; ri < vertex.resourcesNum; ++ri) {
        const auto& fields = vertex.fetchEmbedded ? embedded : vertex.resources[ri].fields;
        const std::uint16_t stride = static_cast<std::uint16_t>((fields[1] >> 16u) & 0x3fffu);
        const std::uint64_t base = (static_cast<std::uint64_t>(fields[0]) | (static_cast<std::uint64_t>(fields[1]) << 32u)) & 0xffffffffffffull;
        const std::uint32_t numRecords = fields[2];
        const std::uint32_t fetchIndex = vertex.fetchEmbedded ? 0u : vertex.resourcesDst[ri].fetchIndex;
        bool merged = false;
        for (int bi = 0; bi < buffersNum; ++bi) {
            auto& b = buffers[bi];
            if (b.stride != stride || b.fetchIndex != fetchIndex) continue;
            const auto low = base < b.addr ? base : b.addr;
            if (base - low >= stride || b.addr - low >= stride) continue;
            if (b.numRecords != numRecords) throw std::runtime_error("ShaderInputInfoBuilder: merged vertex buffers disagree on record count");
            b.addr = low;
            if (b.attrNum >= ShaderVertexInputBuffer::MaxAttributes) throw std::runtime_error("ShaderInputInfoBuilder: vertex buffer attribute count exceeds the supported domain");
            ++b.attrNum;
            merged = true;
            break;
        }
        if (merged) continue;
        if (buffersNum >= ShaderVertexInputInfo::MaxResources) throw std::runtime_error("ShaderInputInfoBuilder: vertex buffer count exceeds the supported domain");
        buffers[buffersNum++] = {base, stride, numRecords, fetchIndex, 1};
    }
    if (stage == ShaderStageKind::Local || stage == ShaderStageKind::TessellationControl || stage == ShaderStageKind::TessellationEvaluation) {
        _validateTessellation(tessellation);
    }
    if (stage == ShaderStageKind::Mesh) {
        _validateMesh(mesh);
    }
}

}
