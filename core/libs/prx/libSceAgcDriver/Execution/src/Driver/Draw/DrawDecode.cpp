#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"

namespace AgcDriver::DriverDetail {

std::shared_ptr<DrawDecode> Driver::decodeDraw(const QueueState& queue, const Submission& submission) {
    using Stage = ShaderRecompiler::ShaderStage;
    using Role = ShaderRecompiler::ProgramRole;
    const auto programAddress = [&](std::uint32_t base) {
        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, base);
        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, base + 1);
        const auto high = readRegister(queue.shader, base + 1);
        require((high & ~0xffu) == 0, "reserved graphics program address bits are set");
        return (static_cast<std::uint64_t>(readRegister(queue.shader, base)) << 8u) | (static_cast<std::uint64_t>(high) << 40u);
    };
    const auto prepare = [&](std::uint64_t address, std::uint8_t type, Stage stage, std::uint32_t rsrc2, std::uint32_t userDataBase) {
        const bool nullPixel = address == 0 && stage == Stage::Fragment;
        if (nullPixel) address = NullPixelProgramAddress();
        auto it = submission.shaders->upper_bound(address);
        require(it != submission.shaders->begin(), "graphics program does not belong to a registered shader");
        --it;
        const auto& snapshot = *it->second;
        require(address - snapshot.codeAddress < snapshot.code.size() * sizeof(std::uint32_t), "graphics program is outside registered shader code");
        require(snapshot.type == type, "graphics program refers to an incompatible shader binary type");
        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, rsrc2);
        const auto resources = nullPixel && !queue.shader.contains(rsrc2) ? 0u : readRegister(queue.shader, rsrc2);
        const auto userCount = ((resources >> 1u) & 0x1fu) | (((resources >> 27u) & 1u) << 5u);
        require(userCount <= 32, "graphics user SGPR count exceeds the register bank");
        const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / sizeof(std::uint32_t));
        DrawProgram result{
            {stage, address, std::span(snapshot.code).subspan(codeOffset), snapshot.headerAddress, snapshot.header},
            userDataBase,
            8,
            {},
            {{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}},
            it->second,
            codeOffset
        };
        for (std::uint32_t i = 0; i < userCount; ++i) {
            Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, userDataBase + i);
            result.userData.push_back(readRegister(queue.shader, userDataBase + i));
        }
        return result;
    };
    {
        auto product = std::make_shared<DrawDecode>();
        product->state = Graphics::DecodeState(queue);
        auto& programs = product->programs;
        auto& roles = product->roles;
        programs.reserve(5);
        roles.reserve(5);
        const auto append = [&](std::uint32_t base, std::uint8_t type, Stage stage, std::uint32_t resources, std::uint32_t users, Role role) {
            programs.push_back(prepare(programAddress(base), type, stage, resources, users));
            roles.push_back(role);
        };
        const auto initializeMerged = [&](DrawProgram& program, std::uint32_t pointerBase, bool pointerRequired) {
            program.firstUserSgpr = 0;
            program.userData.insert(program.userData.begin(), 8, 0);
            Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, pointerBase);
            Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, pointerBase + 1);
            if (!pointerRequired && !queue.shader.contains(pointerBase) && !queue.shader.contains(pointerBase + 1)) return;
            const auto low = readRegister(queue.shader, pointerBase);
            const auto high = readRegister(queue.shader, pointerBase + 1);
            const auto address = static_cast<std::uint64_t>(low) | (static_cast<std::uint64_t>(high) << 32u);
            require(address != 0 || !pointerRequired, "merged shader user-data address is null");
            if (address == 0) return;
            GuestMemory::CheckRange(reinterpret_cast<const void*>(address), 8, 4);
            program.userData[0] = low;
            program.userData[1] = high;
        };
        const auto& graphics = product->state;
        if (graphics.stages.path == Graphics::ShaderPath::Tessellation) {
            append(0x148, 5, Stage::Local, 0x10b, 0x10c, Role::Local);
            append(0x108, 7, Stage::TessellationControl, 0x10b, 0x10c, Role::Hull);
            initializeMerged(programs.back(), 0x102, true);
            append(0x0c8, 2, Stage::TessellationEvaluation, 0x08b, 0x08c, Role::Domain);
        } else if (graphics.stages.path == Graphics::ShaderPath::Geometry) {
            const auto frontAddress = programAddress(0xc8);
            auto snapshot = submission.shaders->upper_bound(frontAddress);
            require(snapshot != submission.shaders->begin(), "geometry front program is not registered");
            --snapshot;
            const auto type = snapshot->second->type;
            require(type == 2 || type == 4, "invalid geometry front binary type");
            append(0xc8, type, Stage::Mesh, 0x8b, 0x8c, Role::Main);
            initializeMerged(programs.back(), 0x82, type == 4);
            if (type == 4) append(0x88, 6, Stage::Mesh, 0x8b, 0x8c, Role::GeometryBack);
        } else {
            append(0xc8, 2, Stage::Vertex, 0x8b, 0x8c, Role::Main);
        }
        const bool nullPixel = Graphics::PixelProgramUnset(queue);
        if (nullPixel) {
            const auto rejection = Graphics::NullPixelProgramRejection(queue);
            require(rejection.empty(), rejection.c_str());
        }
        append(0x008, 1, Stage::Fragment, 0x00b, 0x00c, Role::Fragment);
        programs.back().firstUserSgpr = 0;
        product->pixel = Graphics::DecodePixelStageInfo(queue.context, Graphics::ExportMappings(graphics), nullPixel);
        return product;
    }
}

void Driver::resolveDrawDecode(const QueueState& queue, const Submission& submission, std::shared_ptr<const DrawDecode>& decode, bool registerKey, std::uint64_t drawKey, bool profile) {
    if (decode == nullptr || verifyDrawRecipe()) {
        std::vector<Graphics::RegisterRead> readLog;
        struct LogScope {
            explicit LogScope(std::vector<Graphics::RegisterRead>* log) { Graphics::RegisterReadLog() = log; }
            ~LogScope() { Graphics::RegisterReadLog() = nullptr; }
        } logScope(verifyDrawRecipe() && registerKey ? &readLog : nullptr);
        auto fresh = decodeDraw(queue, submission);
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
