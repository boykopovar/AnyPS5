#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "CacheKey.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <cstdlib>
#include <cstring>
#include <list>
#include <stdexcept>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

namespace AgcDriver::DriverDetail {

std::shared_ptr<const ShaderSnapshot> ReadRawComputeShader(std::uint64_t address) {
    GuestMemory::CheckRange(reinterpret_cast<const void*>(address), sizeof(std::uint32_t), 256);
    static std::mutex cacheMutex;
    static std::list<std::shared_ptr<const ShaderSnapshot>> cache;
    static std::size_t cacheBytes = 0;
    std::shared_ptr<const ShaderSnapshot> cached;
    {
        std::lock_guard lock(cacheMutex);
        const auto found = std::find_if(cache.begin(), cache.end(), [address](const auto& entry) { return entry->codeAddress == address; });
        if (found != cache.end()) cached = *found;
    }
    if (cached) {
        const auto code = std::as_bytes(std::span(cached->code));
        GuestMemory::FlushGpuWrites(address, code.size());
        if (GuestMemory::CompareMapped(address, code) == GuestMemory::Compare::Equal) {
            std::lock_guard lock(cacheMutex);
            const auto found = std::find(cache.begin(), cache.end(), cached);
            if (found != cache.end()) cache.splice(cache.begin(), cache, found);
            return cached;
        }
    }
    constexpr std::size_t limit = 1024 * 1024;
    const auto ranges = GuestMemory::CommittedRanges(address, limit);
    std::uint64_t end = address;
    for (const auto& range : ranges) {
        if (range.first != end) break;
        end = range.second;
    }
    const auto available = static_cast<std::size_t>(end - address) / sizeof(std::uint32_t);
    ShaderSnapshot snapshot{address, 0, 0, {}, {}};
    while (snapshot.code.size() < available) {
        const auto previous = snapshot.code.size();
        snapshot.code.resize(std::min(available, std::max<std::size_t>(64, previous * 2)));
        GuestMemory::Read(address + previous * sizeof(std::uint32_t),
            std::as_writable_bytes(std::span(snapshot.code).subspan(previous)), alignof(std::uint32_t));
        try {
            const auto decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(snapshot.code);
            const auto& last = decoded.instructions.back();
            snapshot.code.resize(last.programCounter / sizeof(std::uint32_t) + last.wordCount);
            auto result = std::make_shared<const ShaderSnapshot>(std::move(snapshot));
            std::lock_guard lock(cacheMutex);
            const auto found = std::find_if(cache.begin(), cache.end(), [address](const auto& entry) { return entry->codeAddress == address; });
            if (found != cache.end()) {
                if ((*found)->code == result->code) {
                    result = *found;
                    cache.splice(cache.begin(), cache, found);
                    return result;
                }
                cacheBytes -= (*found)->code.size() * sizeof(std::uint32_t);
                cache.erase(found);
            }
            const auto bytes = result->code.size() * sizeof(std::uint32_t);
            while (!cache.empty() && (cache.size() >= 64 || cacheBytes + bytes > 8 * 1024 * 1024)) {
                cacheBytes -= cache.back()->code.size() * sizeof(std::uint32_t);
                cache.pop_back();
            }
            cache.push_front(result);
            cacheBytes += bytes;
            return result;
        } catch (const std::out_of_range&) {
            if (snapshot.code.size() == available) break;
        }
    }
    throw std::runtime_error("AGC driver: raw compute program has no reachable end within mapped code or the size limit");
}

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
    if (request.graphics.has_value()) {
        for (const auto& linked : request.graphics->linkedPrograms) {
            for (const auto value : {static_cast<std::uint64_t>(linked.role), linked.binary.codeAddress}) {
                key ^= value;
                key *= 0x100000001b3ull;
            }
        }
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
        if (FailureMemo()) {
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

alignas(256) static const std::uint32_t NullPixelCode[64] = {0xbf810000u};
static const Shader NullPixelShader = [] {
    Shader shader{};
    shader.file_header = 0x34333231u;
    shader.version = 0x18u;
    shader.code = NullPixelCode;
    shader.header_size = sizeof(Shader);
    shader.shader_size = sizeof(NullPixelCode);
    shader.type = 1;
    return shader;
}();

std::uint64_t NullPixelProgramAddress() {
    return reinterpret_cast<std::uintptr_t>(NullPixelCode);
}

std::uint64_t SnapshotCodeHash(const ShaderSnapshot& snapshot, std::size_t codeOffset) {
    static const bool enabled = [] { const char* text = std::getenv("APS5_CODE_HASH_MEMO"); return text == nullptr || std::strcmp(text, "0") != 0; }();
    if (!enabled) return 0;
    auto& memo = *snapshot.handles;
    {
        std::lock_guard lock(memo.codeHashMutex);
        for (const auto& entry : memo.codeHashes) {
            if (entry.offset == codeOffset) return entry.hash;
        }
    }
    const auto hash = ShaderRecompiler::RecompileCacheKey::HashCode(std::span(snapshot.code).subspan(codeOffset));
    std::lock_guard lock(memo.codeHashMutex);
    memo.codeHashes[memo.nextCodeHash] = {codeOffset, hash};
    memo.nextCodeHash = (memo.nextCodeHash + 1) % memo.codeHashes.size();
    return hash;
}

void Driver::RegisterShader(const Shader* shader) {
    CheckFailure();
    GuestMemory::CheckRange(shader, sizeof(Shader), 1);
    Shader fields;
    std::memcpy(&fields, static_cast<const void*>(shader), sizeof(Shader));
    require(fields.file_header == 0x34333231u && fields.version == 0x18u, "invalid shader header");
    require(fields.header_size >= sizeof(Shader), "shader header is smaller than its fixed fields");
    require(fields.shader_size != 0 && (fields.shader_size & 3u) == 0, "invalid shader size");
    GuestMemory::CheckRange(shader, fields.header_size, 1);
    const auto* code = const_cast<const void*>(fields.code);
    GuestMemory::CheckRange(code, fields.shader_size, 256);
    ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(code), reinterpret_cast<std::uintptr_t>(shader), fields.type, {}, {}};
    snapshot.code.resize(fields.shader_size / sizeof(std::uint32_t));
    std::memcpy(snapshot.code.data(), code, fields.shader_size);
    snapshot.header.resize(fields.header_size);
    std::memcpy(snapshot.header.data(), static_cast<const void*>(shader), fields.header_size);

    static const char* traceRegs = std::getenv("APS5_TRACE_SHADER_REGS");
    if (traceRegs != nullptr && (std::string(traceRegs) == "all" || std::strtoull(traceRegs, nullptr, 16) == snapshot.codeAddress)) {
        const auto print = [](const ShaderRegister* registers, std::uint32_t count) {
            for (std::uint32_t i = 0; i < count && registers != nullptr; ++i) {
                ShaderRegister value;
                std::memcpy(&value, static_cast<const void*>(registers + i), sizeof(value));
                std::fprintf(stderr, " %x=%08x", value.offset, value.value);
            }
        };
        std::fprintf(stderr, "[shader] 0x%llx type %u cx", static_cast<unsigned long long>(snapshot.codeAddress), fields.type);
        print(fields.cx_registers, fields.num_cx_registers);
        std::fprintf(stderr, " sh");
        print(fields.sh_registers, fields.num_sh_registers);
        std::fprintf(stderr, "\n");
    }
    std::lock_guard lock(mutex);
    rethrowFailure();
    const auto address = snapshot.codeAddress;

    if (shaders == nullptr) shaders = std::make_shared<ShaderRegistry>();
    else if (shaders.use_count() != 1) shaders = std::make_shared<ShaderRegistry>(*shaders);
    shaders->insert_or_assign(address, std::make_shared<const ShaderSnapshot>(std::move(snapshot)));
    if (shaders->find(NullPixelProgramAddress()) == shaders->end()) {
        ShaderSnapshot null{NullPixelProgramAddress(), reinterpret_cast<std::uintptr_t>(&NullPixelShader), NullPixelShader.type, {}, {}};
        null.code.assign(std::begin(NullPixelCode), std::end(NullPixelCode));
        null.header.resize(sizeof(Shader));
        std::memcpy(null.header.data(), &NullPixelShader, sizeof(Shader));
        shaders->insert_or_assign(NullPixelProgramAddress(), std::make_shared<const ShaderSnapshot>(std::move(null)));
    }
}

}
