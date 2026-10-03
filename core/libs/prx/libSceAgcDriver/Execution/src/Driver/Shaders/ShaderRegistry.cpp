#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "CacheKey.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <cstdlib>
#include <cstring>
#include <stdexcept>

namespace AgcDriver::DriverDetail {

bool FailureMemo() {
    static const bool memo = std::getenv("APS5_NO_FAILURE_MEMO") == nullptr;
    return memo;
}

std::shared_ptr<const ShaderRecompiler::SourceHandle> SourceHandleFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, std::uint64_t deviceSerial, const ShaderRecompiler::RecompileRequest& request, bool bypass, const std::string** poisoned) {
    static const bool enabled = std::getenv("APS5_NO_SOURCE_HANDLE_CACHE") == nullptr && std::getenv("APS5_NO_CAPTURE_REUSE") == nullptr;
    static const bool verify = std::getenv("APS5_VERIFY_SOURCE_HANDLE") != nullptr;
    if (!enabled || bypass || ShaderRecompiler::DebugProbeActive() || !request.useCache) return nullptr;
    std::uint64_t key = 0xcbf29ce484222325ull;
    for (const auto value : {static_cast<std::uint64_t>(codeOffset), deviceSerial, ShaderRecompiler::RecompileCacheKey::ContextHash(request)}) {
        key ^= value;
        key *= 0x100000001b3ull;
    }
    auto& memos = *snapshot.handles;
    {
        std::lock_guard lock(memos.mutex);
        for (const auto& entry : memos.entries) {
            if (entry.key != key || (entry.handle == nullptr && entry.failure == nullptr)) continue;
            if (entry.handle == nullptr) {
                if (verify) {
                    bool resolved = true;
                    try {
                        static_cast<void>(ShaderRecompiler::ResolveSource(request));
                    } catch (const std::exception&) {
                        resolved = false;
                    }
                    if (resolved) throw std::runtime_error("AGC driver: source handle memo poisoned but the source resolved");
                }
                ShaderMemory::CountHandleMemo(true);
                if (poisoned == nullptr) throw std::runtime_error(*entry.failure);
                *poisoned = entry.failure.get();
                return nullptr;
            }
            if (verify && ShaderRecompiler::ResolveSource(request)->source != entry.handle->source) throw std::runtime_error("AGC driver: source handle memo answered a different source");
            ShaderMemory::CountHandleMemo(true);
            return entry.handle;
        }
    }
    std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
    try {
        handle = ShaderRecompiler::ResolveSource(request);
    } catch (const std::exception& error) {
        if (FailureMemo() && request.shader.stage == ShaderRecompiler::ShaderStage::Compute) {
            std::lock_guard lock(memos.mutex);
            memos.entries[memos.next] = {key, nullptr, std::make_shared<const std::string>(error.what())};
            memos.next = (memos.next + 1) % memos.entries.size();
            memos.poisoned.fetch_add(1, std::memory_order_relaxed);
        }
        throw;
    }
    ShaderMemory::CountHandleMemo(false);
    if (handle == nullptr) return nullptr;
    std::lock_guard lock(memos.mutex);
    memos.entries[memos.next] = {key, handle, nullptr};
    memos.next = (memos.next + 1) % memos.entries.size();
    return handle;
}

void Driver::RegisterShader(const Shader* shader) {
    CheckFailure();
    GuestMemory::CheckRange(shader, sizeof(Shader), alignof(Shader));
    require(shader->file_header == 0x34333231u && shader->version == 0x18u, "invalid shader header");
    require(shader->header_size >= sizeof(Shader), "shader header is smaller than its fixed fields");
    require(shader->shader_size != 0 && (shader->shader_size & 3u) == 0, "invalid shader size");
    GuestMemory::CheckRange(shader, shader->header_size, alignof(Shader));
    const auto* code = const_cast<const void*>(shader->code);
    GuestMemory::CheckRange(code, shader->shader_size, 256);
    ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(code), reinterpret_cast<std::uintptr_t>(shader), shader->type, {}, {}};
    snapshot.code.resize(shader->shader_size / sizeof(std::uint32_t));
    std::memcpy(snapshot.code.data(), code, shader->shader_size);
    snapshot.header.resize(shader->header_size);
    std::memcpy(snapshot.header.data(), shader, shader->header_size);

    static const char* traceRegs = std::getenv("APS5_TRACE_SHADER_REGS");
    if (traceRegs != nullptr && (std::string(traceRegs) == "all" || std::strtoull(traceRegs, nullptr, 16) == snapshot.codeAddress)) {
        std::fprintf(stderr, "[shader] 0x%llx type %u cx", static_cast<unsigned long long>(snapshot.codeAddress), shader->type);
        for (std::uint32_t i = 0; i < shader->num_cx_registers && shader->cx_registers != nullptr; ++i) std::fprintf(stderr, " %x=%08x", shader->cx_registers[i].offset, shader->cx_registers[i].value);
        std::fprintf(stderr, " sh");
        for (std::uint32_t i = 0; i < shader->num_sh_registers && shader->sh_registers != nullptr; ++i) std::fprintf(stderr, " %x=%08x", shader->sh_registers[i].offset, shader->sh_registers[i].value);
        std::fprintf(stderr, "\n");
    }
    std::lock_guard lock(mutex);
    rethrowFailure();
    const auto address = snapshot.codeAddress;

    if (shaders == nullptr) shaders = std::make_shared<ShaderRegistry>();
    else if (shaders.use_count() != 1) shaders = std::make_shared<ShaderRegistry>(*shaders);
    auto registered = std::make_shared<const ShaderSnapshot>(std::move(snapshot));
    shaders->insert_or_assign(address, registered);
    preparation.Enqueue(*shader, std::move(registered));
}

}
