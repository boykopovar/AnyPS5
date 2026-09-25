#include "Translation/ShaderInputInfoBuilder.hpp"
#include "IntermediateRepresentation/IrMetadata.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

thread_local ShaderPixelInputInfo pixelStorage;
thread_local ShaderComputeInputInfo computeStorage;
thread_local ShaderVertexInputInfo vertexStorage;

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

}

ShaderStageInputInfo BuildShaderStageInputInfo(ShaderStageKind stage, const GuestContext& context, std::uint32_t hostSubgroupSize) {
    switch (stage) {
    case ShaderStageKind::Compute: {
        if (!context.compute.has_value()) {
            throw std::runtime_error("ShaderInputInfoBuilder: GuestContext.compute is not set");
        }
        const auto& compute = *context.compute;
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
        computeStorage.threadIdsNum = static_cast<int>(compute.threadIdComponentCount);
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
        pixelStorage = ShaderPixelInputInfo{};
        for (std::uint32_t i = 0; i < 32; ++i) {
            pixelStorage.interpolatorSettings[i] = pixel.interpolatorSettings[i];
        }
        pixelStorage.inputNum = pixel.interpolatorCount;
        if (pixel.hasPerspectiveCenterVgpr) {
            pixelStorage.psPerspectiveCenterVgpr = pixel.perspectiveCenterVgpr;
        }
        for (std::uint32_t i = 0; i < 8; ++i) {
            pixelStorage.targetOutputMode[i] = pixel.targetOutputMode[i];
            pixelStorage.targetExportMapping[i].packed = pixel.targetExportMapping[i];
        }
        pixelStorage.psPosX = pixel.posX;
        pixelStorage.psPosY = pixel.posY;
        pixelStorage.psPosZ = pixel.posZ;
        pixelStorage.psPosW = pixel.posW;
        pixelStorage.psFrontFace = pixel.frontFace;
        pixelStorage.psAncillary = pixel.ancillary;
        pixelStorage.psNoPerspective = pixel.noPerspective;
        pixelStorage.psPixelKillEnable = pixel.pixelKillEnable;
        pixelStorage.psDepthExportEnable = pixel.depthExportEnable;
        pixelStorage.psSampleMaskExportEnable = pixel.sampleMaskExportEnable;
        pixelStorage.psSampleShading = pixel.sampleShading;
        pixelStorage.psEarlyZ = pixel.earlyZ;
        pixelStorage.psExecuteOnNoop = pixel.executeOnNoop;
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
        vertexStorage = ShaderVertexInputInfo{};
        vertexStorage.logicalStage = _toIrShaderStage(stage);
        vertexStorage.fetchEmbedded = vertex.fetchEmbedded;
        vertexStorage.fetchExternal = false;
        vertexStorage.fetchAttribReg = static_cast<int>(vertex.fetchAttribReg);
        vertexStorage.fetchBufferReg = static_cast<int>(vertex.fetchBufferReg);
        vertexStorage.resourcesNum = static_cast<int>(vertex.resourcesNum);
        for (std::uint32_t i = 0; i < vertex.resourcesNum; ++i) {
            vertexStorage.resources[i].fields = vertex.resources[i].fields;
            vertexStorage.resourcesDst[i].registerStart = vertex.resourcesDst[i].registerStart;
            vertexStorage.resourcesDst[i].registersNum = vertex.resourcesDst[i].registersNum;
            vertexStorage.resourcesDst[i].attrId = vertex.resourcesDst[i].attrId;
            vertexStorage.resourcesDst[i].fetchIndex = vertex.resourcesDst[i].fetchIndex;
        }
        _detectVertexBuffers(vertexStorage);
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

}
