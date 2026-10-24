#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include <algorithm>
#include <spirv/unified1/spirv.hpp>

namespace AgcDriver::DriverDetail {

namespace {

std::uint32_t ReadGraphicsRegister(const Registers& registers, std::uint32_t offset) {
    const auto found = registers.find(offset);
    return found == registers.end() ? 0u : found->second;
}

}

void ReadUserWords(const QueueState& queue, DrawProgram& program, bool staticAbi) {
    if (!program.nullPixel) Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, program.resourceRegister);
    const auto resources = program.nullPixel ? 0u : ReadGraphicsRegister(queue.shader, program.resourceRegister);
    const auto userCount = ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
    require(userCount <= 32, "graphics user SGPR count exceeds the register bank");
    program.userData.clear();
    program.userData.reserve(userCount + 8u);
    for (std::uint32_t i = 0; i < userCount; ++i) {
        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, program.userDataBase + i);
        program.userData.push_back(staticAbi ? ReadGraphicsRegister(queue.shader, program.userDataBase + i) : readUserData(queue.shader, program.userDataBase + i));
    }
    if ((program.binary.stage == ShaderRecompiler::ShaderStage::Vertex || program.binary.stage == ShaderRecompiler::ShaderStage::Fragment) && program.snapshot->header.empty() && program.snapshot->userDataTailRegister) program.tailContinuation = ReadUserDataTailContinuation(program.binary.code, program.firstUserSgpr, program.userData);
}

ShaderRecompiler::ShaderVertexStageInfo DecodeOrderedVertexStageInfo(std::span<const std::byte> header, std::uint64_t headerAddress, std::span<const std::uint32_t> userData, std::vector<Graphics::DecodeRead>& reads, std::uint32_t hiddenUserWords) {
    const auto decode = [&] {
        reads.clear();
        return Graphics::DecodeVertexStageInfo(header, headerAddress, userData, &reads, false, hiddenUserWords);
    };
    if (!DrawPipeline::Active()) return decode();
    auto& queue = DrawPipeline::Queue0();
    const auto watch = queue.WatchWrites();
    auto info = decode();
    if (watch.items == 0 && !queue.Busy() && queue.Retired() == watch.retired) return info;
    std::vector<DrawPipeline::Range> ranges;
    ranges.reserve(reads.size());
    for (const auto& read : reads) ranges.emplace_back(read.address, read.address + read.size);
    if (!queue.WrittenSince(watch, ranges)) return info;
    queue.NoteDecodeDrain();
    queue.Drain(DrawPipeline::DrainReason::Capture);
    return decode();
}

void InitializeMerged(const QueueState& queue, DrawProgram& program, std::uint32_t pointerBase, bool pointerRequired, bool staticAbi) {
    program.merged = true;
    program.mergedPointer = pointerBase;
    program.mergedPointerRequired = pointerRequired;
    program.firstUserSgpr = 0;
    program.userData.insert(program.userData.begin(), 8, 0);
    program.hiddenUserWords = 8;
    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, pointerBase);
    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, pointerBase + 1);
    if (staticAbi) return;
    if (!pointerRequired && !queue.shader.contains(pointerBase) && !queue.shader.contains(pointerBase + 1)) return;
    const auto low = ReadGraphicsRegister(queue.shader, pointerBase);
    const auto high = ReadGraphicsRegister(queue.shader, pointerBase + 1);
    const auto address = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u);
    require(address != 0 || !pointerRequired, "merged shader user-data address is null");
    if (address == 0) return;
    if (pointerRequired) GuestMemory::CheckRange(reinterpret_cast<const void*>(address), 8, 4);
    program.userData[0] = low;
    program.userData[1] = high;
}

void AppendProgramMemory(std::vector<ShaderRecompiler::MemoryRegion>& memory, const std::vector<DrawProgram>& programs) {
    for (const auto& program : programs) {
        for (const auto& region : program.memory) {
            if (region.bytes.empty()) continue;
            const bool listed = std::any_of(memory.begin(), memory.end(), [&](const ShaderRecompiler::MemoryRegion& known) { return known.guestAddress == region.guestAddress && known.bytes.data() == region.bytes.data() && known.bytes.size() == region.bytes.size(); });
            if (!listed) memory.push_back(region);
        }
    }
}

std::vector<std::pair<std::size_t, std::size_t>> UserDataWords(const std::vector<DrawProgram>& programs, const std::vector<ShaderRecompiler::ProgramRole>& roles, std::uint32_t location) {
    using Role = ShaderRecompiler::ProgramRole;
    require(roles.size() == programs.size(), "draw program roles are incomplete");
    std::vector<std::pair<std::size_t, std::size_t>> words;
    if (location == 0x280u) return words;
    for (std::size_t i = 0; i < programs.size(); ++i) {
        if (roles[i] == Role::Fragment || roles[i] == Role::GeometryBack || location < programs[i].userDataBase) continue;
        const auto word = location - programs[i].userDataBase + (8u - programs[i].firstUserSgpr);
        if (word < programs[i].userData.size()) words.emplace_back(i, static_cast<std::size_t>(word));
    }
    return words;
}

void DecodeGraphicsPrograms(DrawDecode& decoded, const QueueState& queue, const ShaderRegistry& registry, bool staticAbi, bool includeFragment) {
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;
    const auto programAddress = [&](std::uint32_t base) {
        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, base);
        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, base + 1);
        const auto high = ReadGraphicsRegister(queue.shader, base + 1);
        require((high & ~0xffu) == 0, "reserved graphics program address bits are set");
        return (static_cast<std::uint64_t>(ReadGraphicsRegister(queue.shader, base)) << 8u) | (static_cast<std::uint64_t>(high) << 40u);
    };
    const bool pixelSkipped = Graphics::PixelProgramSkipped(queue);
    const auto prepare = [&](std::uint64_t address, std::uint8_t type, Stage stage, std::uint32_t rsrc2, std::uint32_t userDataBase) {
        const bool nullPixel = stage == Stage::Fragment && (address == 0 || pixelSkipped);
        if (nullPixel) address = NullPixelProgramAddress();
        const auto program = ProgramSnapshot(registry, address, {type});
        const auto& snapshot = *program;
        require((address - snapshot.codeAddress) % sizeof(std::uint32_t) == 0, "graphics entry point is not dword aligned");
        require(snapshot.header.empty() || snapshot.type == type, "graphics program refers to an incompatible shader binary type");
        if (!nullPixel && snapshot.header.empty()) NoteRawGraphicsDraw(snapshot);
        const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
        DrawProgram result{
            {stage, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header},
            userDataBase,
            stage == Stage::Fragment ? 0u : 8u,
            {},
            {{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}},
            program,
            codeOffset
        };
        result.resourceRegister = rsrc2;
        result.nullPixel = nullPixel;
        ReadUserWords(queue, result, staticAbi);
        return result;
    };
    {
        auto* product = &decoded;
        auto& programs = product->programs;
        auto& roles = product->roles;
        programs.reserve(5);
        roles.reserve(5);
        const auto append = [&](std::uint32_t base, std::uint8_t type, Stage stage, std::uint32_t resources, std::uint32_t users, Role role) {
            programs.push_back(prepare(programAddress(base), type, stage, resources, users));
            roles.push_back(role);
        };
        auto& graphics = product->state;
        if (graphics.stages.path == Graphics::ShaderPath::Tessellation) {
            const auto localAddress = programAddress(0x148);
            const auto local = !staticAbi || localAddress != 0 ? ProgramSnapshot(registry, localAddress, {3, 5}) : nullptr;
            const bool mergedHull = local != nullptr && !local->header.empty() && local->type == 3;
            require(graphics.stages.tessellation.has_value(), "tessellation draw has no tessellation configuration");
            graphics.stages.tessellation->mergedHull = mergedHull;
            if (mergedHull) {
                append(0x148, 3, Stage::Local, 0x10b, 0x10c, Role::Local);
                append(0x148, 3, Stage::TessellationControl, 0x10b, 0x10c, Role::Hull);
            } else {
                if (local != nullptr) append(0x148, 5, Stage::Local, 0x10b, 0x10c, Role::Local);
                append(0x108, 7, Stage::TessellationControl, 0x10b, 0x10c, Role::Hull);
            }
            InitializeMerged(queue, programs.back(), 0x102, !mergedHull, staticAbi);
            append(0x0c8, 2, Stage::TessellationEvaluation, 0x08b, 0x08c, Role::Domain);
        } else if (graphics.stages.path == Graphics::ShaderPath::Geometry) {
            const auto frontAddress = programAddress(0xc8);
            const auto front = ProgramSnapshot(registry, frontAddress, {2, 4});
            const auto type = front->header.empty() ? std::uint8_t{2} : front->type;
            require(type == 2 || type == 4, "invalid geometry front binary type");
            append(0xc8, type, Stage::Mesh, 0x8b, 0x8c, Role::Main);
            InitializeMerged(queue, programs.back(), 0x82, type == 4, staticAbi);
            if (type == 4) append(0x88, 6, Stage::Mesh, 0x8b, 0x8c, Role::GeometryBack);
        } else {
            append(0xc8, 2, Stage::Vertex, 0x8b, 0x8c, Role::Main);
        }
        if (includeFragment) {
            if (pixelSkipped) {
                const auto rejection = Graphics::NullPixelProgramRejection(queue);
                require(rejection.empty(), rejection.c_str());
            }
            append(0x008, 1, Stage::Fragment, 0x00b, 0x00c, Role::Fragment);
        }
    }
}

std::shared_ptr<DrawDecode> Driver::decodeDraw(const QueueState& queue, const Submission& submission, const Graphics::Context& context) {
    auto product = std::make_shared<DrawDecode>();
    product->state = Graphics::DecodeState(queue, VertexWorkgroupRequired(queue, *submission.shaders));
    DecodeGraphicsPrograms(*product, queue, *submission.shaders, false, true);
    product->pixel = Graphics::DecodePixelStageInfo(queue.context, Graphics::ExportMappings(product->state), Graphics::PixelProgramSkipped(queue));
    Graphics::NormalizeUnusedViewportDepth(product->state, product->pixel);
    Graphics::ApplyDepthRemap(context, product->state);
    product->pixel.targetExportPacking = Graphics::ExportPackings(product->state);
    product->pixel.dualSourceBlend = product->state.dualSourceBlend;
    product->pixel.reversedBlend = product->state.reversedBlend;
    product->pixel.targetMerge = Graphics::ExportTargetMerge(product->state);
    product->pixel.alphaToCoverage = product->state.alphaToCoverage;
    product->pixel.quadPixelMask = Graphics::QuadPixelMask(product->state);
    if (product->state.sampleLocationGrid) {
        const auto localDevice = device.Load();
        if (localDevice != nullptr) product->pixel.sampleOrder = localDevice->HostSampleOrder(product->state);
    }
    product->pixel.quadSampleMask = Graphics::QuadSampleMasks(product->state, product->pixel.sampleOrder);
    if (product->pixel.stencilRefExportEnable && Graphics::StencilReferenceUnused(product->state)) {
        const auto localDevice = device.Load();
        const auto capabilities = localDevice == nullptr ? std::vector<std::uint32_t>{} : localDevice->Target().supportedCapabilities;
        if (std::ranges::find(capabilities, static_cast<std::uint32_t>(spv::CapabilityStencilExportEXT)) == capabilities.end()) product->pixel.stencilRefExportEnable = false;
    }
    if (product->pixel.ancillary) product->pixel.rasterPrimitive = Graphics::RasterPrimitiveOf(product->state);
    if (product->pixel.ancillary) product->pixel.layerExported = product->state.layerExports != 0;
    product->pixel.ditheredAlphaToCoverage = product->state.ditheredAlphaToCoverage;
    product->pixel.iteratesSamples = product->state.perSampleShading;
    product->pixel.sampleLocations = Graphics::PixelSampleLocations(product->state, product->pixel);
    return product;
}

void Driver::resolveDrawDecode(const QueueState& queue, const Submission& submission, const Graphics::Context& context, std::shared_ptr<const DrawDecode>& decode, bool registerKey, std::uint64_t drawKey, bool profile) {
    if (decode == nullptr || verifyDrawRecipe()) {
        std::vector<Graphics::RegisterRead> readLog;
        const Graphics::RegisterReadLogScope logScope(verifyDrawRecipe() && registerKey ? &readLog : nullptr);
        auto fresh = decodeDraw(queue, submission, context);
        if (verifyDrawRecipe() && registerKey) {
            std::uint64_t facadeMismatches = 0;
            for (const auto read : readLog) {
                if (Graphics::DrawKeyCovers(read)) continue;
                ++facadeMismatches;
                static std::atomic<std::uint64_t> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: the decoders read %s register 0x%x, which DrawKeyRegisters lacks\n", Graphics::RegisterBankName(read.bank), read.offset);
            }
            std::uint64_t decodeMismatches = 0;
            if (decode != nullptr && !sameDecode(*decode, *fresh)) {
                decodeMismatches = 1;
                static std::atomic<std::uint64_t> reports{0};
                if (reports.fetch_add(1) < 20) std::fprintf(stderr, "[draw-cache] verify: the entry's decode differs from a fresh decode (key 0x%llx, target 0x%llx)\n", static_cast<unsigned long long>(drawKey), static_cast<unsigned long long>(fresh->state.color.address));
            }
            std::lock_guard cacheLock(drawCacheMutex);
            drawEntryCounters.facadeMismatches += facadeMismatches;
            if (decode != nullptr) ++drawEntryCounters.verifyDecodes;
            drawEntryCounters.verifyDecodeMismatches += decodeMismatches;
        }
        decode = std::move(fresh);
    } else if (profile) {
        std::lock_guard cacheLock(drawCacheMutex);
        ++drawEntryCounters.decodeSkipped;
    }
}

}
