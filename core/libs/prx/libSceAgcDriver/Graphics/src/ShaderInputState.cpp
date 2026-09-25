#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

namespace AgcDriver::Graphics {
namespace {

constexpr std::uint32_t computeNumThreadX = 0x207;
constexpr std::uint32_t computeNumThreadY = 0x208;
constexpr std::uint32_t computeNumThreadZ = 0x209;
constexpr std::uint32_t computePgmRsrc2 = 0x213;

constexpr std::uint32_t spiPsInputCntl0 = 0x191;
constexpr std::uint32_t spiPsInputEna = 0x1B3;
constexpr std::uint32_t spiPsInputAddr = 0x1B4;
constexpr std::uint32_t spiPsInControl = 0x1B6;
constexpr std::uint32_t dbShaderControl = 0x203;
constexpr std::uint32_t spiShaderColFormat = 0x1C5;

std::uint32_t read(const Registers& registers, std::uint32_t offset) {
    const auto it = registers.find(offset);
    if (it == registers.end()) {
        char text[64];
        std::snprintf(text, sizeof(text), "AGC graphics: missing register at DWORD 0x%x", offset);
        throw std::runtime_error(text);
    }
    return it->second;
}

template <typename T> T _readHeaderPod(std::span<const std::byte> header, std::uint64_t headerAddress, const void* pointer) {
    if (pointer == nullptr) throw std::runtime_error("AGC graphics: null AGC header pointer");
    const auto address = reinterpret_cast<std::uint64_t>(pointer);
    if (address < headerAddress) throw std::runtime_error("AGC graphics: AGC header pointer precedes the shader header");
    const auto offset = address - headerAddress;
    if (offset + sizeof(T) > header.size()) throw std::runtime_error("AGC graphics: AGC header pointer is outside the registered shader header");
    T value;
    std::memcpy(&value, header.data() + offset, sizeof(T));
    return value;
}

template <typename T> void _readHeaderArray(std::span<const std::byte> header, std::uint64_t headerAddress, const void* pointer, std::uint32_t count, T* destination) {
    if (count == 0) return;
    if (pointer == nullptr) throw std::runtime_error("AGC graphics: null AGC header array pointer");
    const auto address = reinterpret_cast<std::uint64_t>(pointer);
    if (address < headerAddress) throw std::runtime_error("AGC graphics: AGC header array pointer precedes the shader header");
    const auto offset = address - headerAddress;
    const auto bytes = static_cast<std::uint64_t>(count) * sizeof(T);
    if (offset + bytes > header.size()) throw std::runtime_error("AGC graphics: AGC header array is outside the registered shader header");
    std::memcpy(destination, header.data() + offset, bytes);
}

}

ShaderRecompiler::ShaderComputeStageInfo DecodeComputeStageInfo(const Registers& shader) {
    const auto numThreadX = read(shader, computeNumThreadX);
    const auto numThreadY = read(shader, computeNumThreadY);
    const auto numThreadZ = read(shader, computeNumThreadZ);
    if (numThreadX == 0 || numThreadY == 0 || numThreadZ == 0) {
        throw std::runtime_error("AGC graphics: COMPUTE_NUM_THREAD_X/Y/Z must be nonzero");
    }
    const auto rsrc2 = read(shader, computePgmRsrc2);
    if ((rsrc2 & 0x1u) != 0) {
        throw std::runtime_error("AGC graphics: COMPUTE_PGM_RSRC2.SCRATCH_EN is unsupported");
    }
    // Debug aid: APS5_LDS_SLACK=<dwords> grows every dispatch's LDS allocation by that much, to tell
    // whether a program depends on addresses past its declared allocation.
    static const std::uint32_t ldsSlack = [] { const char* text = std::getenv("APS5_LDS_SLACK"); return text ? static_cast<std::uint32_t>(std::strtoul(text, nullptr, 0)) : 0u; }();
    return ShaderRecompiler::ShaderComputeStageInfo{
        {numThreadX, numThreadY, numThreadZ},
        std::min(((rsrc2 >> 15u) & 0x1FFu) * 128u + ldsSlack, 16384u),
        {((rsrc2 >> 7u) & 0x1u) != 0, ((rsrc2 >> 8u) & 0x1u) != 0, ((rsrc2 >> 9u) & 0x1u) != 0},
        ((rsrc2 >> 10u) & 0x1u) != 0,
        ((rsrc2 >> 11u) & 0x3u) + 1u
    };
}

ShaderRecompiler::ShaderPixelStageInfo DecodePixelStageInfo(const Registers& context, bool hasColorTarget, std::uint8_t colorComponentMapping) {
    const auto inControl = read(context, spiPsInControl);
    const auto inputNum = inControl & 0x3Fu;
    if (inputNum > 32u) {
        throw std::runtime_error("AGC graphics: SPI_PS_IN_CONTROL input count exceeds 32");
    }
    const auto ena = read(context, spiPsInputEna);
    const auto addr = read(context, spiPsInputAddr);
    const auto activeInputs = ena & addr;
    constexpr std::uint32_t knownMask = 0x1u | 0x2u | 0x10u | 0x20u | 0x100u | 0x200u | 0x400u | 0x800u | 0x1000u | 0x2000u;
    if ((activeInputs & ~knownMask) != 0) {
        throw std::runtime_error("AGC graphics: unsupported SPI_PS_INPUT_ENA/ADDR bit combination");
    }
    std::array<std::uint32_t, 32> interpolatorSettings{};
    for (std::uint32_t i = 0; i < inputNum; ++i) {
        interpolatorSettings[i] = read(context, spiPsInputCntl0 + i);
    }
    const auto shaderControl = read(context, dbShaderControl);
    // Bits 9 and 11 are EXEC_ON_HIER_FAIL and ALPHA_TO_MASK_DISABLE on GFX10; neither changes what the
    // recompiled pixel shader computes.
    if (((shaderControl >> 13u) & 0x3u) != 0) {
        throw std::runtime_error("AGC graphics: DB_SHADER_CONTROL.CONSERVATIVE_Z_EXPORT is unsupported");
    }
    const auto colFormat = read(context, spiShaderColFormat);
    std::array<std::uint8_t, 8> targetOutputMode{};
    for (std::uint32_t i = 0; i < 8u; ++i) {
        targetOutputMode[i] = static_cast<std::uint8_t>((colFormat >> (4u * i)) & 0xFu);
    }
    const bool hasPerspectiveCenterVgpr = (activeInputs & 0x2u) != 0;
    const bool pixelKillEnable = ((shaderControl >> 6u) & 0x1u) != 0;
    const bool depthExportEnable = (shaderControl & 0x1u) != 0;
    const bool sampleMaskExportEnable = ((shaderControl >> 8u) & 0x1u) != 0;
    const auto zOrder = (shaderControl >> 4u) & 0x3u;
    std::array<std::uint8_t, 8> targetExportMapping{};
    targetExportMapping.fill(0xe4u);
    if (hasColorTarget) {
        targetExportMapping[0] = colorComponentMapping;
    }
    return ShaderRecompiler::ShaderPixelStageInfo{
        inputNum,
        interpolatorSettings,
        (inControl & 0x8000u) != 0,
        hasPerspectiveCenterVgpr ? ((activeInputs & 0x1u) ? 2u : 0u) : 0u,
        hasPerspectiveCenterVgpr,
        (activeInputs & 0x100u) != 0,
        (activeInputs & 0x200u) != 0,
        (activeInputs & 0x400u) != 0,
        (activeInputs & 0x800u) != 0,
        (activeInputs & 0x1000u) != 0,
        (activeInputs & 0x2000u) != 0,
        (activeInputs & 0x11u) == 0x11u,
        (activeInputs & 0x20u) != 0,
        pixelKillEnable,
        depthExportEnable,
        sampleMaskExportEnable,
        zOrder == 1u && !pixelKillEnable && !depthExportEnable && !sampleMaskExportEnable,
        ((shaderControl >> 10u) & 0x1u) != 0,
        targetOutputMode,
        targetExportMapping
    };
}

ShaderRecompiler::ShaderVertexStageInfo DecodeVertexStageInfo(std::span<const std::byte> header, std::uint64_t headerAddress, std::span<const std::uint32_t> userData) {
    if (header.size() < sizeof(Shader)) throw std::runtime_error("AGC graphics: shader header is smaller than the fixed AGC header");
    Shader shader;
    std::memcpy(&shader, header.data(), sizeof(Shader));
    ShaderRecompiler::ShaderVertexStageInfo info{};
    if (shader.user_data == nullptr) throw std::runtime_error("AGC graphics: missing AGC user-data header");
    const auto userDataHeader = _readHeaderPod<ShaderUserData>(header, headerAddress, shader.user_data);
    if (userDataHeader.direct_resource_count > ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT) throw std::runtime_error("AGC graphics: AGC direct-resource count exceeds the known resource domain");
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> directOffsets{};
    directOffsets.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    if (userDataHeader.direct_resource_count != 0) _readHeaderArray(header, headerAddress, userDataHeader.direct_resource_offset, userDataHeader.direct_resource_count, directOffsets.data());
    std::int32_t vertexBufferReg = -1;
    std::int32_t vertexAttribReg = -1;
    for (std::uint32_t type = 0; type < userDataHeader.direct_resource_count; ++type) {
        const auto reg = directOffsets[type];
        if (reg == ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET) continue;
        if (type == static_cast<std::uint32_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)) vertexBufferReg = reg;
        if (type == static_cast<std::uint32_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)) vertexAttribReg = reg;
    }
    if (vertexAttribReg < 0) return info;
    if (vertexBufferReg < 0) throw std::runtime_error("AGC graphics: vertex attribute table requires a vertex buffer table");
    if (static_cast<std::uint32_t>(vertexBufferReg) + 1u >= userData.size()) throw std::runtime_error("AGC graphics: vertex buffer table pointer exceeds the user-SGPR domain");
    if (static_cast<std::uint32_t>(vertexAttribReg) + 1u >= userData.size()) throw std::runtime_error("AGC graphics: vertex attribute table pointer exceeds the user-SGPR domain");
    if (shader.num_input_semantics == 0 || shader.num_input_semantics > ShaderRecompiler::ShaderVertexStageInfo::MaxResources) throw std::runtime_error("AGC graphics: vertex semantic count is outside the supported domain");
    if (shader.input_semantics == nullptr) throw std::runtime_error("AGC graphics: missing vertex input semantics");
    std::array<ShaderSemantic, ShaderRecompiler::ShaderVertexStageInfo::MaxResources> semantics{};
    _readHeaderArray(header, headerAddress, shader.input_semantics, shader.num_input_semantics, semantics.data());
    const auto attribTableAddr = static_cast<std::uint64_t>(userData[static_cast<std::uint32_t>(vertexAttribReg)]) | (static_cast<std::uint64_t>(userData[static_cast<std::uint32_t>(vertexAttribReg) + 1u]) << 32u);
    const auto bufferTableAddr = static_cast<std::uint64_t>(userData[static_cast<std::uint32_t>(vertexBufferReg)]) | (static_cast<std::uint64_t>(userData[static_cast<std::uint32_t>(vertexBufferReg) + 1u]) << 32u);
    if (attribTableAddr == 0) throw std::runtime_error("AGC graphics: null vertex attribute table address");
    if (bufferTableAddr == 0) throw std::runtime_error("AGC graphics: null vertex buffer table address");
    info.fetchEmbedded = true;
    info.fetchAttribReg = static_cast<std::uint32_t>(vertexAttribReg);
    info.fetchBufferReg = static_cast<std::uint32_t>(vertexBufferReg);
    for (std::uint32_t i = 0; i < shader.num_input_semantics; ++i) {
        const auto& semantic = semantics[i];
        if (semantic.static_vb_index == 1 || semantic.static_attribute == 1) throw std::runtime_error("AGC graphics: statically bound vertex attributes are not implemented");
        std::array<std::byte, 4> attribWordBytes{};
        AgcDriver::GuestMemory::Read(attribTableAddr + static_cast<std::uint64_t>(semantic.semantic) * 4u, attribWordBytes, 4);
        std::uint32_t attribWord;
        std::memcpy(&attribWord, attribWordBytes.data(), 4);
        const auto index = attribWord & 0x1fu;
        const auto format = (attribWord >> 5u) & 0x1ffu;
        const auto offset = (attribWord >> 14u) & 0xfffu;
        const auto fetchIndex = (attribWord >> 26u) & 0x1u;
        if (index >= ShaderRecompiler::ShaderVertexStageInfo::MaxResources) throw std::runtime_error("AGC graphics: vertex buffer index exceeds the supported domain");
        std::array<std::byte, 16> sharpBytes{};
        AgcDriver::GuestMemory::Read(bufferTableAddr + static_cast<std::uint64_t>(index) * 16u, sharpBytes, 4);
        std::array<std::uint32_t, 4> sharp{};
        std::memcpy(sharp.data(), sharpBytes.data(), 16);
        if (info.resourcesNum >= ShaderRecompiler::ShaderVertexStageInfo::MaxResources) throw std::runtime_error("AGC graphics: vertex resource count exceeds the supported domain");
        auto& resource = info.resources[info.resourcesNum];
        auto& destination = info.resourcesDst[info.resourcesNum];
        resource.fields = sharp;
        destination.registerStart = static_cast<std::int32_t>(semantic.hardware_mapping);
        destination.registersNum = static_cast<std::int32_t>(semantic.size_in_elements);
        destination.attrId = static_cast<std::int32_t>(semantic.semantic);
        destination.fetchIndex = fetchIndex;
        if (format != 0u) {
            const auto bufferFormat = format >> 2u;
            const auto channels = (format & 0x3u) + 1u;
            const auto dstSelY = channels > 1u ? 5u : 0u;
            const auto dstSelZ = channels > 2u ? 6u : 0u;
            const auto dstSelW = channels > 3u ? 7u : 1u;
            const auto dstSel = 4u | (dstSelY << 3u) | (dstSelZ << 6u) | (dstSelW << 9u);
            resource.fields[3] = (resource.fields[3] & ~((0x7fu << 12u) | 0xfffu)) | (bufferFormat << 12u) | dstSel;
        }
        if (offset != 0u) {
            const auto base = ((static_cast<std::uint64_t>(resource.fields[0]) | (static_cast<std::uint64_t>(resource.fields[1]) << 32u)) & 0xffffffffffffull) + offset;
            resource.fields[0] = static_cast<std::uint32_t>(base & 0xffffffffu);
            resource.fields[1] = (resource.fields[1] & 0xffff0000u) | static_cast<std::uint32_t>((base >> 32u) & 0xffffu);
        }
        ++info.resourcesNum;
    }
    return info;
}

}
