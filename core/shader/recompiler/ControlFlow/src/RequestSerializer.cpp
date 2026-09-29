#include "ControlFlow/RequestSerializer.hpp"
#include <stdexcept>

namespace ShaderRecompiler {

namespace {

constexpr char kBase64Alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64Encode(std::string_view data) {
    std::string result;
    result.reserve(((data.size() + 2u) / 3u) * 4u);
    std::uint64_t i = 0u;
    const std::uint64_t size = data.size();
    while (i + 3u <= size) {
        const std::uint8_t b0 = static_cast<std::uint8_t>(data[i]);
        const std::uint8_t b1 = static_cast<std::uint8_t>(data[i + 1u]);
        const std::uint8_t b2 = static_cast<std::uint8_t>(data[i + 2u]);
        const std::uint32_t chunk = (static_cast<std::uint32_t>(b0) << 16u) | (static_cast<std::uint32_t>(b1) << 8u) | static_cast<std::uint32_t>(b2);
        result.push_back(kBase64Alphabet[(chunk >> 18u) & 0x3Fu]);
        result.push_back(kBase64Alphabet[(chunk >> 12u) & 0x3Fu]);
        result.push_back(kBase64Alphabet[(chunk >> 6u) & 0x3Fu]);
        result.push_back(kBase64Alphabet[chunk & 0x3Fu]);
        i += 3u;
    }
    const std::uint64_t remaining = size - i;
    if (remaining == 1u) {
        const std::uint8_t b0 = static_cast<std::uint8_t>(data[i]);
        const std::uint32_t chunk = static_cast<std::uint32_t>(b0) << 16u;
        result.push_back(kBase64Alphabet[(chunk >> 18u) & 0x3Fu]);
        result.push_back(kBase64Alphabet[(chunk >> 12u) & 0x3Fu]);
        result.push_back('=');
        result.push_back('=');
    } else if (remaining == 2u) {
        const std::uint8_t b0 = static_cast<std::uint8_t>(data[i]);
        const std::uint8_t b1 = static_cast<std::uint8_t>(data[i + 1u]);
        const std::uint32_t chunk = (static_cast<std::uint32_t>(b0) << 16u) | (static_cast<std::uint32_t>(b1) << 8u);
        result.push_back(kBase64Alphabet[(chunk >> 18u) & 0x3Fu]);
        result.push_back(kBase64Alphabet[(chunk >> 12u) & 0x3Fu]);
        result.push_back(kBase64Alphabet[(chunk >> 6u) & 0x3Fu]);
        result.push_back('=');
    }
    return result;
}

std::uint8_t base64DecodeChar(char value) {
    if (value >= 'A' && value <= 'Z') {
        return static_cast<std::uint8_t>(value - 'A');
    }
    if (value >= 'a' && value <= 'z') {
        return static_cast<std::uint8_t>(value - 'a' + 26);
    }
    if (value >= '0' && value <= '9') {
        return static_cast<std::uint8_t>(value - '0' + 52);
    }
    if (value == '+') {
        return 62u;
    }
    if (value == '/') {
        return 63u;
    }
    throw std::runtime_error("RequestSerializer: invalid base64 character");
}

std::string base64Decode(std::string_view text) {
    if (text.size() % 4u != 0u) {
        throw std::runtime_error("RequestSerializer: invalid base64 length");
    }
    std::string result;
    result.reserve((text.size() / 4u) * 3u);
    for (std::uint64_t i = 0u; i < text.size(); i += 4u) {
        const char c0 = text[i];
        const char c1 = text[i + 1u];
        const char c2 = text[i + 2u];
        const char c3 = text[i + 3u];
        if (c0 == '=' || c1 == '=') {
            throw std::runtime_error("RequestSerializer: invalid base64 padding");
        }
        if (c2 == '=' && c3 != '=') {
            throw std::runtime_error("RequestSerializer: invalid base64 padding");
        }
        if ((c2 == '=' || c3 == '=') && i + 4u != text.size()) {
            throw std::runtime_error("RequestSerializer: invalid base64 padding position");
        }
        const std::uint8_t v0 = base64DecodeChar(c0);
        const std::uint8_t v1 = base64DecodeChar(c1);
        const std::uint8_t v2 = c2 == '=' ? 0u : base64DecodeChar(c2);
        const std::uint8_t v3 = c3 == '=' ? 0u : base64DecodeChar(c3);
        const std::uint32_t chunk = (static_cast<std::uint32_t>(v0) << 18u) | (static_cast<std::uint32_t>(v1) << 12u) | (static_cast<std::uint32_t>(v2) << 6u) | static_cast<std::uint32_t>(v3);
        result.push_back(static_cast<char>((chunk >> 16u) & 0xFFu));
        if (c2 != '=') {
            result.push_back(static_cast<char>((chunk >> 8u) & 0xFFu));
        }
        if (c3 != '=') {
            result.push_back(static_cast<char>(chunk & 0xFFu));
        }
    }
    return result;
}

class Writer {
public:
    explicit Writer(std::string& buffer) : m_buffer(buffer) {}

    void WriteU8(std::uint8_t value) { m_buffer.push_back(static_cast<char>(value)); }
    void WriteBool(bool value) { WriteU8(value ? 1u : 0u); }

    void WriteU32(std::uint32_t value) {
        for (std::uint32_t i = 0; i < 4u; i++) {
            WriteU8(static_cast<std::uint8_t>(value >> (i * 8u)));
        }
    }

    void WriteI32(std::int32_t value) { WriteU32(static_cast<std::uint32_t>(value)); }

    void WriteU64(std::uint64_t value) {
        for (std::uint32_t i = 0; i < 8u; i++) {
            WriteU8(static_cast<std::uint8_t>(value >> (i * 8u)));
        }
    }

    void WriteBytes(std::span<const std::byte> bytes) {
        WriteU64(bytes.size());
        for (const std::byte b : bytes) {
            WriteU8(static_cast<std::uint8_t>(b));
        }
    }

    void WriteU32Span(std::span<const std::uint32_t> values) {
        WriteU64(values.size());
        for (const std::uint32_t value : values) {
            WriteU32(value);
        }
    }

    void WriteString(std::string_view text) {
        WriteU64(text.size());
        m_buffer.append(text);
    }

private:
    std::string& m_buffer;
};

class Reader {
public:
    explicit Reader(std::string_view data) : m_data(data) {}

    std::uint8_t ReadU8() {
        Require(1u);
        return static_cast<std::uint8_t>(m_data[m_offset++]);
    }

    bool ReadBool() { return ReadU8() != 0u; }

    std::uint32_t ReadU32() {
        std::uint32_t value = 0u;
        for (std::uint32_t i = 0; i < 4u; i++) {
            value |= static_cast<std::uint32_t>(ReadU8()) << (i * 8u);
        }
        return value;
    }

    std::int32_t ReadI32() { return static_cast<std::int32_t>(ReadU32()); }

    std::uint64_t ReadU64() {
        std::uint64_t value = 0u;
        for (std::uint32_t i = 0; i < 8u; i++) {
            value |= static_cast<std::uint64_t>(ReadU8()) << (i * 8u);
        }
        return value;
    }

    std::vector<std::byte> ReadBytes() {
        const auto count = ReadU64();
        Require(count);
        std::vector<std::byte> result(count);
        for (std::uint64_t i = 0; i < count; i++) {
            result[i] = static_cast<std::byte>(ReadU8());
        }
        return result;
    }

    std::vector<std::uint32_t> ReadU32Vector() {
        const auto count = ReadU64();
        Require(count, 4u);
        std::vector<std::uint32_t> result;
        result.reserve(count);
        for (std::uint64_t i = 0; i < count; i++) {
            result.push_back(ReadU32());
        }
        return result;
    }

    std::string ReadString() {
        const auto count = ReadU64();
        Require(count);
        std::string result(m_data.substr(m_offset, count));
        m_offset += count;
        return result;
    }

    void Require(std::uint64_t count, std::uint64_t elementSize = 1u) const {
        if (count > (m_data.size() - m_offset) / elementSize) {
            throw std::runtime_error("RequestSerializer: truncated data");
        }
    }

private:
    std::string_view m_data;
    std::uint64_t m_offset = 0u;
};

ShaderStage readShaderStage(Reader& reader) {
    const auto value = reader.ReadU8();
    if (value > static_cast<std::uint8_t>(ShaderStage::Mesh)) {
        throw std::runtime_error("RequestSerializer: invalid ShaderStage value");
    }
    return static_cast<ShaderStage>(value);
}

ProgramRole readProgramRole(Reader& reader) {
    const auto value = reader.ReadU8();
    if (value > static_cast<std::uint8_t>(ProgramRole::Fragment)) {
        throw std::runtime_error("RequestSerializer: invalid ProgramRole value");
    }
    return static_cast<ProgramRole>(value);
}

void writeShaderBinary(Writer& writer, const ShaderBinary& binary) {
    writer.WriteU8(static_cast<std::uint8_t>(binary.stage));
    writer.WriteU64(binary.codeAddress);
    writer.WriteU32Span(binary.code);
    writer.WriteU64(binary.headerAddress);
    writer.WriteBytes(binary.header);
}

ShaderBinary readShaderBinary(Reader& reader, std::vector<std::uint32_t>& codeStorage, std::vector<std::byte>& headerStorage) {
    ShaderBinary binary{};
    binary.stage = readShaderStage(reader);
    binary.codeAddress = reader.ReadU64();
    codeStorage = reader.ReadU32Vector();
    binary.code = codeStorage;
    binary.headerAddress = reader.ReadU64();
    headerStorage = reader.ReadBytes();
    binary.header = headerStorage;
    return binary;
}

void writeComputeInfo(Writer& writer, const ShaderComputeStageInfo& info) {
    for (const std::uint32_t value : info.numThreads) {
        writer.WriteU32(value);
    }
    writer.WriteU32(info.ldsSizeDwords);
    for (const bool value : info.groupIdEnable) {
        writer.WriteBool(value);
    }
    writer.WriteBool(info.tgSizeEnable);
    writer.WriteU32(info.threadIdComponentCount);
}

ShaderComputeStageInfo readComputeInfo(Reader& reader) {
    ShaderComputeStageInfo info{};
    for (std::uint32_t& value : info.numThreads) {
        value = reader.ReadU32();
    }
    info.ldsSizeDwords = reader.ReadU32();
    for (auto value : {0u, 1u, 2u}) {
        info.groupIdEnable[value] = reader.ReadBool();
    }
    info.tgSizeEnable = reader.ReadBool();
    info.threadIdComponentCount = reader.ReadU32();
    return info;
}

void writePixelInfo(Writer& writer, const ShaderPixelStageInfo& info) {
    writer.WriteU32(info.interpolatorCount);
    for (const std::uint32_t value : info.interpolatorSettings) {
        writer.WriteU32(value);
    }
    writer.WriteBool(info.wave32);
    writer.WriteU32(info.perspectiveCenterVgpr);
    writer.WriteBool(info.hasPerspectiveCenterVgpr);
    writer.WriteBool(info.posX);
    writer.WriteBool(info.posY);
    writer.WriteBool(info.posZ);
    writer.WriteBool(info.posW);
    writer.WriteBool(info.frontFace);
    writer.WriteBool(info.ancillary);
    writer.WriteBool(info.sampleShading);
    writer.WriteBool(info.noPerspective);
    writer.WriteBool(info.pixelKillEnable);
    writer.WriteBool(info.depthExportEnable);
    writer.WriteBool(info.sampleMaskExportEnable);
    writer.WriteBool(info.earlyZ);
    writer.WriteBool(info.executeOnNoop);
    for (const std::uint8_t value : info.targetOutputMode) {
        writer.WriteU8(value);
    }
}

ShaderPixelStageInfo readPixelInfo(Reader& reader) {
    ShaderPixelStageInfo info{};
    info.interpolatorCount = reader.ReadU32();
    for (std::uint32_t& value : info.interpolatorSettings) {
        value = reader.ReadU32();
    }
    info.wave32 = reader.ReadBool();
    info.perspectiveCenterVgpr = reader.ReadU32();
    info.hasPerspectiveCenterVgpr = reader.ReadBool();
    info.posX = reader.ReadBool();
    info.posY = reader.ReadBool();
    info.posZ = reader.ReadBool();
    info.posW = reader.ReadBool();
    info.frontFace = reader.ReadBool();
    info.ancillary = reader.ReadBool();
    info.sampleShading = reader.ReadBool();
    info.noPerspective = reader.ReadBool();
    info.pixelKillEnable = reader.ReadBool();
    info.depthExportEnable = reader.ReadBool();
    info.sampleMaskExportEnable = reader.ReadBool();
    info.earlyZ = reader.ReadBool();
    info.executeOnNoop = reader.ReadBool();
    for (std::uint8_t& value : info.targetOutputMode) {
        value = reader.ReadU8();
    }
    return info;
}

void writeVertexInfo(Writer& writer, const ShaderVertexStageInfo& info) {
    for (const auto& resource : info.resources) {
        for (const std::uint32_t value : resource.fields) {
            writer.WriteU32(value);
        }
    }
    for (const auto& destination : info.resourcesDst) {
        writer.WriteI32(destination.registerStart);
        writer.WriteI32(destination.registersNum);
        writer.WriteI32(destination.attrId);
        writer.WriteU32(destination.fetchIndex);
    }
    writer.WriteU32(info.resourcesNum);
    writer.WriteU32(info.fetchAttribReg);
    writer.WriteU32(info.fetchBufferReg);
    writer.WriteBool(info.fetchEmbedded);
}

ShaderVertexStageInfo readVertexInfo(Reader& reader) {
    ShaderVertexStageInfo info{};
    for (auto& resource : info.resources) {
        for (std::uint32_t& value : resource.fields) {
            value = reader.ReadU32();
        }
    }
    for (auto& destination : info.resourcesDst) {
        destination.registerStart = reader.ReadI32();
        destination.registersNum = reader.ReadI32();
        destination.attrId = reader.ReadI32();
        destination.fetchIndex = reader.ReadU32();
    }
    info.resourcesNum = reader.ReadU32();
    info.fetchAttribReg = reader.ReadU32();
    info.fetchBufferReg = reader.ReadU32();
    info.fetchEmbedded = reader.ReadBool();
    return info;
}

void writeMeshTargetLimits(Writer& writer, const MeshTargetLimits& limits) {
    for (const std::uint32_t value : limits.maxWorkgroupSize) {
        writer.WriteU32(value);
    }
    writer.WriteU32(limits.maxWorkgroupInvocations);
    writer.WriteU32(limits.maxSharedMemoryBytes);
    writer.WriteU32(limits.maxOutputVertices);
    writer.WriteU32(limits.maxOutputPrimitives);
    writer.WriteU32(limits.maxOutputComponents);
    writer.WriteU32(limits.maxOutputMemoryBytes);
    writer.WriteU32(limits.outputPerVertexGranularity);
    writer.WriteU32(limits.outputPerPrimitiveGranularity);
}

MeshTargetLimits readMeshTargetLimits(Reader& reader) {
    MeshTargetLimits limits{};
    for (std::uint32_t& value : limits.maxWorkgroupSize) {
        value = reader.ReadU32();
    }
    limits.maxWorkgroupInvocations = reader.ReadU32();
    limits.maxSharedMemoryBytes = reader.ReadU32();
    limits.maxOutputVertices = reader.ReadU32();
    limits.maxOutputPrimitives = reader.ReadU32();
    limits.maxOutputComponents = reader.ReadU32();
    limits.maxOutputMemoryBytes = reader.ReadU32();
    limits.outputPerVertexGranularity = reader.ReadU32();
    limits.outputPerPrimitiveGranularity = reader.ReadU32();
    return limits;
}

void writeTessellationTargetLimits(Writer& writer, const TessellationTargetLimits& limits) {
    writer.WriteU32(limits.maxPatchSize);
    writer.WriteU32(limits.maxControlPerVertexInputComponents);
    writer.WriteU32(limits.maxControlPerVertexOutputComponents);
    writer.WriteU32(limits.maxControlPerPatchOutputComponents);
    writer.WriteU32(limits.maxControlTotalOutputComponents);
    writer.WriteU32(limits.maxEvaluationInputComponents);
    writer.WriteU32(limits.maxEvaluationOutputComponents);
}

TessellationTargetLimits readTessellationTargetLimits(Reader& reader) {
    TessellationTargetLimits limits{};
    limits.maxPatchSize = reader.ReadU32();
    limits.maxControlPerVertexInputComponents = reader.ReadU32();
    limits.maxControlPerVertexOutputComponents = reader.ReadU32();
    limits.maxControlPerPatchOutputComponents = reader.ReadU32();
    limits.maxControlTotalOutputComponents = reader.ReadU32();
    limits.maxEvaluationInputComponents = reader.ReadU32();
    limits.maxEvaluationOutputComponents = reader.ReadU32();
    return limits;
}

void writeMeshConfiguration(Writer& writer, const MeshConfiguration& configuration) {
    writer.WriteU32(configuration.inputPrimitive);
    writer.WriteU32(configuration.primitivesPerGroup);
    writer.WriteU32(configuration.verticesPerGroup);
    writer.WriteU32(configuration.maxVertices);
    writer.WriteU32(configuration.maxPrimitives);
    writer.WriteU32(configuration.threadsPerGroup);
    writer.WriteU32(configuration.ldsSizeDwords);
    writer.WriteU32(configuration.provokingVertex);
}

MeshConfiguration readMeshConfiguration(Reader& reader) {
    MeshConfiguration configuration{};
    configuration.inputPrimitive = reader.ReadU32();
    configuration.primitivesPerGroup = reader.ReadU32();
    configuration.verticesPerGroup = reader.ReadU32();
    configuration.maxVertices = reader.ReadU32();
    configuration.maxPrimitives = reader.ReadU32();
    configuration.threadsPerGroup = reader.ReadU32();
    configuration.ldsSizeDwords = reader.ReadU32();
    configuration.provokingVertex = reader.ReadU32();
    return configuration;
}

void writeTessellationConfiguration(Writer& writer, const TessellationConfiguration& configuration) {
    writer.WriteU32(configuration.inputControlPoints);
    writer.WriteU32(configuration.outputControlPoints);
    writer.WriteU32(configuration.domain);
    writer.WriteU32(configuration.partitioning);
    writer.WriteU32(configuration.outputTopology);
}

TessellationConfiguration readTessellationConfiguration(Reader& reader) {
    TessellationConfiguration configuration{};
    configuration.inputControlPoints = reader.ReadU32();
    configuration.outputControlPoints = reader.ReadU32();
    configuration.domain = reader.ReadU32();
    configuration.partitioning = reader.ReadU32();
    configuration.outputTopology = reader.ReadU32();
    return configuration;
}

void writeGuestContext(Writer& writer, const GuestContext& context) {
    writer.WriteU32(context.waveSize);
    writer.WriteU32(context.userDataBaseRegister);
    writer.WriteU32Span(context.userData);
    writer.WriteBool(context.compute.has_value());
    if (context.compute.has_value()) {
        writeComputeInfo(writer, *context.compute);
    }
    writer.WriteBool(context.pixel.has_value());
    if (context.pixel.has_value()) {
        writePixelInfo(writer, *context.pixel);
    }
    writer.WriteBool(context.vertex.has_value());
    if (context.vertex.has_value()) {
        writeVertexInfo(writer, *context.vertex);
    }
    writer.WriteU64(context.memory.size());
    for (const auto& region : context.memory) {
        writer.WriteU64(region.guestAddress);
        writer.WriteBytes(region.bytes);
    }
}

GuestContext readGuestContext(Reader& reader, DeserializedRequest& result) {
    GuestContext context{};
    context.waveSize = reader.ReadU32();
    context.userDataBaseRegister = reader.ReadU32();
    result.userData = reader.ReadU32Vector();
    context.userData = result.userData;
    if (reader.ReadBool()) {
        result.compute = readComputeInfo(reader);
        context.compute = result.compute;
    }
    if (reader.ReadBool()) {
        result.pixel = readPixelInfo(reader);
        context.pixel = result.pixel;
    }
    if (reader.ReadBool()) {
        result.vertex = readVertexInfo(reader);
        context.vertex = result.vertex;
    }
    const auto regionCount = reader.ReadU64();
    reader.Require(regionCount, 16u);
    result.memoryBytesStorage.reserve(regionCount);
    result.memory.reserve(regionCount);
    for (std::uint64_t i = 0; i < regionCount; i++) {
        MemoryRegion region{};
        region.guestAddress = reader.ReadU64();
        result.memoryBytesStorage.push_back(reader.ReadBytes());
        region.bytes = result.memoryBytesStorage.back();
        result.memory.push_back(region);
    }
    context.memory = result.memory;
    return context;
}

void writeSpirvTarget(Writer& writer, const SpirvTarget& target) {
    writer.WriteU32(target.vulkanVersion);
    writer.WriteU32(target.spirvVersion);
    writer.WriteU32(target.subgroupSize);
    writer.WriteU32(target.bdaAbiVersion);
    writer.WriteU32Span(target.supportedCapabilities);
    writer.WriteU64(target.supportedExtensions.size());
    for (const std::string_view extension : target.supportedExtensions) {
        writer.WriteString(extension);
    }
    writer.WriteBool(target.fragmentShaderBarycentricEnabled);
    for (const std::uint32_t value : target.maxWorkgroupSize) {
        writer.WriteU32(value);
    }
    writer.WriteU32(target.maxWorkgroupInvocations);
    writer.WriteU32(target.maxWorkgroupSharedMemoryBytes);
    writer.WriteBool(target.mesh.has_value());
    if (target.mesh.has_value()) {
        writeMeshTargetLimits(writer, *target.mesh);
    }
    writer.WriteBool(target.tessellation.has_value());
    if (target.tessellation.has_value()) {
        writeTessellationTargetLimits(writer, *target.tessellation);
    }
}

SpirvTarget readSpirvTarget(Reader& reader, DeserializedRequest& result) {
    SpirvTarget target{};
    target.vulkanVersion = reader.ReadU32();
    target.spirvVersion = reader.ReadU32();
    target.subgroupSize = reader.ReadU32();
    target.bdaAbiVersion = reader.ReadU32();
    result.supportedCapabilities = reader.ReadU32Vector();
    target.supportedCapabilities = result.supportedCapabilities;
    const auto extensionCount = reader.ReadU64();
    reader.Require(extensionCount, 8u);
    result.supportedExtensionStorage.reserve(extensionCount);
    result.supportedExtensions.reserve(extensionCount);
    for (std::uint64_t i = 0; i < extensionCount; i++) {
        result.supportedExtensionStorage.push_back(reader.ReadString());
        result.supportedExtensions.push_back(result.supportedExtensionStorage.back());
    }
    target.supportedExtensions = result.supportedExtensions;
    target.fragmentShaderBarycentricEnabled = reader.ReadBool();
    for (std::uint32_t& value : target.maxWorkgroupSize) {
        value = reader.ReadU32();
    }
    target.maxWorkgroupInvocations = reader.ReadU32();
    target.maxWorkgroupSharedMemoryBytes = reader.ReadU32();
    if (reader.ReadBool()) {
        result.mesh = readMeshTargetLimits(reader);
        target.mesh = result.mesh;
    }
    if (reader.ReadBool()) {
        result.tessellation = readTessellationTargetLimits(reader);
        target.tessellation = result.tessellation;
    }
    return target;
}

void writeBindingLayout(Writer& writer, const BindingLayout& layout) {
    writer.WriteU32(layout.descriptorSet);
    writer.WriteU32(layout.firstBinding);
    writer.WriteU32(layout.pushConstantOffsetBytes);
    writer.WriteU32(layout.pushConstantSizeBytes);
}

BindingLayout readBindingLayout(Reader& reader) {
    BindingLayout layout{};
    layout.descriptorSet = reader.ReadU32();
    layout.firstBinding = reader.ReadU32();
    layout.pushConstantOffsetBytes = reader.ReadU32();
    layout.pushConstantSizeBytes = reader.ReadU32();
    return layout;
}

void writeGraphicsCompileContext(Writer& writer, const GraphicsCompileContext& graphics) {
    writer.WriteU32(graphics.firstUserSgpr);
    writer.WriteU64(graphics.linkedPrograms.size());
    for (const auto& program : graphics.linkedPrograms) {
        writer.WriteU8(static_cast<std::uint8_t>(program.role));
        writeShaderBinary(writer, program.binary);
        writer.WriteU32(program.userDataBaseRegister);
        writer.WriteU32(program.firstUserSgpr);
        writer.WriteU32Span(program.userData);
    }
    writer.WriteBool(graphics.mesh.has_value());
    if (graphics.mesh.has_value()) {
        writeMeshConfiguration(writer, *graphics.mesh);
    }
    writer.WriteBool(graphics.tessellation.has_value());
    if (graphics.tessellation.has_value()) {
        writeTessellationConfiguration(writer, *graphics.tessellation);
    }
    writer.WriteU64(graphics.draw.indexAddress);
    writer.WriteU32(graphics.draw.indexCount);
    writer.WriteU32(graphics.draw.indexElementBytes);
    writer.WriteU32(graphics.draw.instanceCount);
}

GraphicsCompileContext readGraphicsCompileContext(Reader& reader, DeserializedGraphicsCompileContext& storage) {
    GraphicsCompileContext graphics{};
    graphics.firstUserSgpr = reader.ReadU32();
    const auto programCount = reader.ReadU64();
    reader.Require(programCount, 50u);
    storage.linkedProgramStorage.reserve(programCount);
    storage.linkedPrograms.reserve(programCount);
    for (std::uint64_t i = 0; i < programCount; i++) {
        LinkedProgram program{};
        program.role = readProgramRole(reader);
        DeserializedLinkedProgram programStorage{};
        program.binary = readShaderBinary(reader, programStorage.code, programStorage.header);
        program.userDataBaseRegister = reader.ReadU32();
        program.firstUserSgpr = reader.ReadU32();
        programStorage.userData = reader.ReadU32Vector();
        storage.linkedProgramStorage.push_back(std::move(programStorage));
        const auto& storedProgram = storage.linkedProgramStorage.back();
        program.binary.code = storedProgram.code;
        program.binary.header = storedProgram.header;
        program.userData = storedProgram.userData;
        storage.linkedPrograms.push_back(program);
    }
    graphics.linkedPrograms = storage.linkedPrograms;
    if (reader.ReadBool()) {
        storage.mesh = readMeshConfiguration(reader);
        graphics.mesh = storage.mesh;
    }
    if (reader.ReadBool()) {
        storage.tessellation = readTessellationConfiguration(reader);
        graphics.tessellation = storage.tessellation;
    }
    storage.draw.indexAddress = reader.ReadU64();
    storage.draw.indexCount = reader.ReadU32();
    storage.draw.indexElementBytes = reader.ReadU32();
    storage.draw.instanceCount = reader.ReadU32();
    graphics.draw = storage.draw;
    return graphics;
}

}

std::string RequestSerializer::Serialize(const RecompileRequest& request) const {
    std::string buffer;
    Writer writer(buffer);
    writer.WriteU32(0x41505335u);
    writer.WriteU32(3u);
    writeShaderBinary(writer, request.shader);
    writeGuestContext(writer, request.context);
    writeSpirvTarget(writer, request.target);
    writeBindingLayout(writer, request.layout);
    writer.WriteBool(request.graphics.has_value());
    if (request.graphics.has_value()) {
        writeGraphicsCompileContext(writer, *request.graphics);
    }
    writer.WriteBool(request.useCache);
    // Version 3: a compute request's partial-group size.
    if (request.context.compute.has_value()) {
        for (const std::uint32_t value : request.context.compute->partialThreads) {
            writer.WriteU32(value);
        }
    }
    return base64Encode(buffer);
}

DeserializedRequest RequestSerializer::Deserialize(std::string_view text) const {
    const std::string decoded = base64Decode(text);
    Reader reader(decoded);
    if (reader.ReadU32() != 0x41505335u) throw std::runtime_error("invalid recompile request signature");
    const auto version = reader.ReadU32();
    if (version < 1u || version > 3u) throw std::runtime_error("unsupported recompile request serialization version");
    DeserializedRequest result{};
    result.request.shader = readShaderBinary(reader, result.shaderCode, result.shaderHeader);
    result.request.context = readGuestContext(reader, result);
    result.request.target = readSpirvTarget(reader, result);
    result.request.layout = readBindingLayout(reader);
    if (reader.ReadBool()) {
        result.graphicsStorage = std::make_unique<DeserializedGraphicsCompileContext>();
        result.request.graphics = readGraphicsCompileContext(reader, *result.graphicsStorage);
    }
    if (version >= 2u) result.request.useCache = reader.ReadBool();
    if (version >= 3u && result.request.context.compute.has_value()) {
        for (std::uint32_t& value : result.request.context.compute->partialThreads) {
            value = reader.ReadU32();
        }
    }
    return result;
}

}
