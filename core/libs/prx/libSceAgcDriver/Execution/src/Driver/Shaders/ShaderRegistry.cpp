#include "prx/libc/include/general/LogMacros.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "CacheKey.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "CompiledVariant.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstdio>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <functional>
#include <list>
#include <pthread.h>
#include <stdexcept>
#include <thread>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

namespace AgcDriver::DriverDetail {

std::shared_ptr<const ShaderRecompiler::SourceHandle> PrepareShaderWithDiagnostics(const ShaderRecompiler::RecompileRequest& request, ShaderRecompiler::ShaderPreparationContext* preparation) {
    try {
        return ShaderRecompiler::PrepareShader(request, preparation);
    } catch (...) {
        try {
            if (std::getenv("APS5_DUMP_SHADERS") != nullptr) static_cast<void>(Driver::dumpRequest(request.shader.codeAddress, request));
        } catch (...) {}
        throw;
    }
}

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

namespace {

void AwaitRegisteredPreparation(PreparedShaders& prepared, std::unique_lock<std::mutex>& lock) {
    prepared.settled.wait(lock, [&] { return !prepared.pending; });
    if (prepared.failure != nullptr) std::rethrow_exception(prepared.failure);
}

}

struct ShaderPreparationTransaction::State {
    struct Change {
        std::shared_ptr<PreparedShaders> destination;
        PreparedShaderState prepared;
        bool edited = false;
        std::size_t baseEntries = 0;
        std::size_t baseAbis = 0;
    };
    static std::mutex writers;
    static thread_local State* active;
    std::unique_lock<std::mutex> lock{writers};
    std::map<PreparedShaders*, Change> changes;
    bool failed = false;
};

std::mutex ShaderPreparationTransaction::State::writers;
thread_local ShaderPreparationTransaction::State* ShaderPreparationTransaction::State::active = nullptr;

ShaderPreparationTransaction::ShaderPreparationTransaction() {
    if (State::active == nullptr) {
        state = std::make_unique<State>();
        State::active = state.get();
    }
    root = State::active;
}

ShaderPreparationTransaction::~ShaderPreparationTransaction() {
    if (!committed) root->failed = true;
    if (state != nullptr) State::active = nullptr;
}

PreparedShaderState& ShaderPreparationTransaction::Edit(const ShaderSnapshot& snapshot) {
    require(!committed, "shader preparation transaction is already committed");
    auto& changes = root->changes;
    const auto found = changes.find(snapshot.prepared.get());
    if (found != changes.end()) {
        found->second.edited = true;
        return found->second.prepared;
    }
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    return changes.emplace(snapshot.prepared.get(), State::Change{snapshot.prepared, *snapshot.prepared, true, snapshot.prepared->entries.size(), snapshot.prepared->registeredAbis.size()}).first->second.prepared;
}

const PreparedShaderState& ShaderPreparationTransaction::Read(const ShaderSnapshot& snapshot) const {
    require(!committed, "shader preparation transaction is already committed");
    auto& changes = root->changes;
    const auto found = changes.find(snapshot.prepared.get());
    if (found != changes.end()) return found->second.prepared;
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    return changes.emplace(snapshot.prepared.get(), State::Change{snapshot.prepared, *snapshot.prepared, false, snapshot.prepared->entries.size(), snapshot.prepared->registeredAbis.size()}).first->second.prepared;
}

void ShaderPreparationTransaction::Commit() {
    require(!committed && !root->failed, "shader preparation transaction was aborted");
    if (state != nullptr) {
        std::vector<std::unique_lock<std::mutex>> locks;
        locks.reserve(state->changes.size());
        for (const auto& [key, change] : state->changes) locks.emplace_back(change.destination->mutex);
        for (auto& [key, change] : state->changes) {
            if (!change.edited) continue;
            auto& destination = *change.destination;
            for (auto index = std::min(change.baseEntries, destination.entries.size()); index < destination.entries.size(); ++index) {
                const auto& entry = destination.entries[index];
                if (std::ranges::none_of(change.prepared.entries, [&](const auto& existing) { return existing.codeOffset == entry.codeOffset && existing.handle == entry.handle; })) change.prepared.entries.push_back(entry);
            }
            for (auto index = std::min(change.baseAbis, destination.registeredAbis.size()); index < destination.registeredAbis.size(); ++index) {
                const auto& abi = destination.registeredAbis[index];
                if (std::ranges::find(change.prepared.registeredAbis, abi) == change.prepared.registeredAbis.end()) change.prepared.registeredAbis.push_back(abi);
            }
            destination.entries.swap(change.prepared.entries);
            destination.registeredAbis.swap(change.prepared.registeredAbis);
            destination.graphicsAbis.swap(change.prepared.graphicsAbis);
            destination.rectangles.swap(change.prepared.rectangles);
            destination.rectangleProgress.swap(change.prepared.rectangleProgress);
            destination.fragments.swap(change.prepared.fragments);
            std::swap(destination.rectangleRequested, change.prepared.rectangleRequested);
        }
    }
    committed = true;
}

bool SameHeader(const ShaderSnapshot& snapshot, const Shader* shader) {
    if (snapshot.headerAddress == reinterpret_cast<std::uintptr_t>(shader)) return true;
    if (snapshot.header.size() < sizeof(Shader)) return false;
    GuestMemory::CheckRange(shader, sizeof(Shader), 1);
    Shader used;
    Shader registered;
    std::memcpy(&used, static_cast<const void*>(shader), sizeof(Shader));
    std::memcpy(&registered, snapshot.header.data(), sizeof(Shader));
    used.user_data = registered.user_data;
    return std::memcmp(&used, &registered, offsetof(Shader, num_sh_registers) + sizeof(Shader::num_sh_registers)) == 0;
}

void PublishRegisteredShader(std::shared_ptr<ShaderRegistry>& registry, const std::shared_ptr<const ShaderSnapshot>& snapshot) {
    ShaderPreparationTransaction transaction;
    if (registry != nullptr) {
        const auto found = registry->find(snapshot->codeAddress);
        if (found != registry->end()) {
            const auto& current = *found->second;
            if (current.headerAddress == snapshot->headerAddress && current.type == snapshot->type && current.code == snapshot->code && current.header == snapshot->header) {
                transaction.Commit();
                return;
            }
        }
    }
    auto next = registry != nullptr ? std::make_shared<ShaderRegistry>(*registry) : std::make_shared<ShaderRegistry>();
    next->insert_or_assign(snapshot->codeAddress, snapshot);
    registry = std::move(next);
    transaction.Commit();
}

namespace {

bool PreparedAtUse(const ShaderSnapshot& snapshot, const ShaderRecompiler::RecompileRequest& request) {
    if (snapshot.codeAddress == NullPixelProgramAddress() && request.shader.stage == ShaderRecompiler::ShaderStage::Fragment && request.context.waveSize == 32u) {
        APS5_LOG_ERR("The null pixel program has no wave%u artifact for this draw; preparing it at draw", request.context.waveSize);
        return true;
    }
    if (!snapshot.header.empty()) return snapshot.prepared->deferred;
    if (snapshot.type != 0 || request.shader.stage != ShaderRecompiler::ShaderStage::Compute) throw std::runtime_error("AGC driver: unregistered program is not a compute shader");
    APS5_LOG_ERR("Compute shader 0x%llx was not registered; preparing its artifact at dispatch", static_cast<unsigned long long>(snapshot.codeAddress));
    return true;
}

}

std::shared_ptr<const ShaderRecompiler::SourceHandle> SourceHandleFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request) {
    struct PreparedKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, PreparedKeyStorage>();
    ShaderRecompiler::BuildPreparedShaderKey(request, key);
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    for (const auto& entry : snapshot.prepared->entries) {
        if (entry.codeOffset == codeOffset && ShaderRecompiler::MatchesPreparedShader(request, *entry.handle, key)) return entry.handle;
    }
    if (PreparedAtUse(snapshot, request)) {
        auto handle = PrepareShaderWithDiagnostics(request);
        snapshot.prepared->entries.push_back({codeOffset, handle});
        return handle;
    }
    std::string layouts;
    for (const auto& entry : snapshot.prepared->entries) {
        if (entry.codeOffset != codeOffset || entry.handle == nullptr || entry.handle->artifact == nullptr) continue;
        const auto& layout = entry.handle->artifact->layout;
        layouts += " [" + std::to_string(layout.pushConstantOffsetBytes) + "," + std::to_string(layout.pushConstantSizeBytes) + "]";
    }
    throw std::runtime_error("AGC driver: prepared shader artifact is missing for the requested static ABI: address=" + std::to_string(request.shader.codeAddress) + " stage=" + std::to_string(static_cast<std::uint32_t>(request.shader.stage)) + " wave=" + std::to_string(request.context.waveSize) + " pushOffset=" + std::to_string(request.layout.pushConstantOffsetBytes) + " pushCapacity=" + std::to_string(request.layout.pushConstantSizeBytes) + " preparedLayouts=" + layouts);
}

ShaderRecompiler::RectListShaders PreparedRectangle(const ShaderSnapshot& snapshot, std::uint64_t vertexId, std::uint64_t fragmentId) {
    std::lock_guard lock(snapshot.prepared->mutex);
    for (const auto& entry : snapshot.prepared->rectangles) {
        if (entry.vertexId == vertexId && entry.fragmentId == fragmentId) {
            require(entry.shaders != nullptr, "prepared rectangle artifact is missing");
            return *entry.shaders;
        }
    }
    throw std::runtime_error("AGC driver: prepared rectangle artifacts are missing");
}

ShaderRecompiler::RectListShaders DrawRectangle(const ShaderSnapshot& front, const std::shared_ptr<const ShaderSnapshot>& fragment, std::uint64_t vertexId, std::uint64_t fragmentId, const ShaderRecompiler::SpirvTarget& target) {
    const auto prepared = [&] {
        std::lock_guard lock(front.prepared->mutex);
        return std::ranges::any_of(front.prepared->rectangles, [&](const auto& entry) { return entry.vertexId == vertexId && entry.fragmentId == fragmentId; });
    };
    if (prepared()) return PreparedRectangle(front, vertexId, fragmentId);
    APS5_LOG_ERR("Rect-list draw of shader 0x%llx has no rectangle for fragment shader 0x%llx; preparing it at draw", static_cast<unsigned long long>(front.codeAddress), static_cast<unsigned long long>(fragment->codeAddress));
    ResolvePreparedGraphics(front, fragment, 17, target);
    return PreparedRectangle(front, vertexId, fragmentId);
}

ShaderRecompiler::PreparedShaderInvocation InvocationFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request) {
    require(codeOffset < snapshot.code.size(), "prepared shader code offset is outside the snapshot");
    const auto code = std::span(snapshot.code).subspan(codeOffset);
    require(request.shader.code.data() == code.data() && request.shader.code.size() == code.size(), "prepared invocation does not refer to registered code");
    auto invocationRequest = request;
    struct PreparedKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, PreparedKeyStorage>();
    ShaderRecompiler::BuildPreparedShaderKey(request, key);
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    for (const auto& entry : snapshot.prepared->entries) {
        if (entry.codeOffset != codeOffset) continue;
        invocationRequest.shader.code = ShaderRecompiler::GetPreparedCode(*entry.handle);
        if (auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(invocationRequest, entry.handle, key)) return std::move(*invocation);
    }
    if (PreparedAtUse(snapshot, request)) {
        auto handle = PrepareShaderWithDiagnostics(request);
        invocationRequest = request;
        invocationRequest.shader.code = ShaderRecompiler::GetPreparedCode(*handle);
        auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(invocationRequest, handle, key);
        if (!invocation.has_value()) throw std::runtime_error("AGC driver: raw compute artifact does not match its invocation");
        snapshot.prepared->entries.push_back({codeOffset, std::move(handle)});
        return std::move(*invocation);
    }
    std::string layouts;
    for (const auto& entry : snapshot.prepared->entries) {
        if (entry.codeOffset != codeOffset || entry.handle == nullptr || entry.handle->artifact == nullptr) continue;
        const auto& layout = entry.handle->artifact->layout;
        layouts += " [" + std::to_string(layout.pushConstantOffsetBytes) + "," + std::to_string(layout.pushConstantSizeBytes) + "]";
    }
    throw std::runtime_error("AGC driver: prepared shader artifact is missing for the requested static ABI: address=" + std::to_string(request.shader.codeAddress) + " stage=" + std::to_string(static_cast<std::uint32_t>(request.shader.stage)) + " wave=" + std::to_string(request.context.waveSize) + " pushOffset=" + std::to_string(request.layout.pushConstantOffsetBytes) + " pushCapacity=" + std::to_string(request.layout.pushConstantSizeBytes) + " preparedLayouts=" + layouts);
}
std::optional<ShaderRecompiler::ShaderFloatMode> RegisteredFloatMode(const ShaderSnapshot& snapshot) {
    if (snapshot.registeredState == nullptr) return std::nullopt;
    std::uint32_t rsrc1;
    std::uint32_t fp16OverflowBit;
    switch (snapshot.type) {
    case 0: rsrc1 = 0x212; fp16OverflowBit = 26; break;
    case 1: rsrc1 = 0x00a; fp16OverflowBit = 29; break;
    case 2: case 4: case 6: rsrc1 = 0x08a; fp16OverflowBit = 31; break;
    case 5: case 7: rsrc1 = 0x10a; fp16OverflowBit = 30; break;
    default: return std::nullopt;
    }
    const auto found = snapshot.registeredState->shader.find(rsrc1);
    if (found == snapshot.registeredState->shader.end()) return std::nullopt;
    const auto value = found->second;
    return ShaderRecompiler::ShaderFloatMode{(value >> 12u) & 0xffu, ((value >> 21u) & 1u) != 0u, ((value >> 23u) & 1u) != 0u, ((value >> fp16OverflowBit) & 1u) != 0u};
}

namespace {

template<typename TValue>
std::vector<TValue> ReadHeaderArray(const ShaderSnapshot& snapshot, const TValue* pointer, std::size_t count) {
    if (count == 0) return {};
    const auto address = reinterpret_cast<std::uintptr_t>(pointer);
    if (address < snapshot.headerAddress || address - snapshot.headerAddress > snapshot.header.size()) throw std::runtime_error("AGC driver: shader metadata is outside the registered header");
    const auto offset = static_cast<std::size_t>(address - snapshot.headerAddress);
    if (count > (snapshot.header.size() - offset) / sizeof(TValue)) throw std::runtime_error("AGC driver: truncated shader metadata");
    std::vector<TValue> result(count);
    std::memcpy(result.data(), snapshot.header.data() + offset, count * sizeof(TValue));
    return result;
}

Shader ReadHeader(const ShaderSnapshot& snapshot) {
    Shader header;
    std::memcpy(&header, snapshot.header.data(), sizeof(header));
    return header;
}

RegisteredShaderState DecodeRegisteredState(const ShaderSnapshot& snapshot) {
    const auto header = ReadHeader(snapshot);
    RegisteredShaderState state{{}, InitialContextRegisters(), {{0x24a, 0}, {0x24b, 0}}};
    for (const auto reg : ReadHeaderArray(snapshot, header.sh_registers, header.num_sh_registers)) {
        state.shader.insert_or_assign(reg.offset, reg.value);
    }
    for (const auto reg : ReadHeaderArray(snapshot, header.cx_registers, header.num_cx_registers)) {
        state.context.insert_or_assign(reg.offset, reg.value);
    }
    if (header.specials != nullptr) {
        const auto special = ReadHeaderArray(snapshot, header.specials, 1).front();
        state.context[special.vgt_shader_stages_en.offset] = special.vgt_shader_stages_en.value;
        if (special.vgt_gs_out_prim_type.offset != 0) state.context[special.vgt_gs_out_prim_type.offset] = special.vgt_gs_out_prim_type.value;
        state.userConfig[special.ge_cntl.offset] = special.ge_cntl.value;
        state.userConfig[special.ge_user_vgpr_en.offset] = special.ge_user_vgpr_en.value;
    }
    return state;
}

std::uint32_t RegisterValue(const Registers& registers, std::uint32_t offset) {
    const auto found = registers.find(offset);
    if (found == registers.end()) throw std::runtime_error("AGC driver: missing static shader ABI register " + std::to_string(offset));
    return found->second;
}

void BuildRegisteredAbiKey(const QueueState& state, const VulkanDevice& device, std::vector<std::uint64_t>& key) {
    key.clear();
    key.reserve(7u + state.shader.size() + state.context.size() + state.userConfig.size());
    key.insert(key.end(), {device.Serial(), ShaderRecompiler::DebugProbeActive(), ShaderRecompiler::RayTracingStrict(), ShaderRecompiler::RayTracingMiss()});
    for (const auto* registers : {&state.shader, &state.context, &state.userConfig}) {
        key.push_back(registers->size());
        for (const auto& [offset, value] : *registers) key.push_back((static_cast<std::uint64_t>(offset) << 32u) | value);
    }
}

struct RegisteredPreparation {
    std::size_t codeOffset = 0;
    ShaderRecompiler::RdnaProgram decoded;
    ShaderRecompiler::SwappcInfo swappc{};
    std::vector<std::uint32_t> userData;
    std::array<ShaderRecompiler::MemoryRegion, 2> memory{};
    std::optional<ShaderRecompiler::RecompileRequest> request;
    std::vector<std::uint64_t> abiKey;
};

std::unique_ptr<RegisteredPreparation> PlanRegistered(const ShaderSnapshot& snapshot, const VulkanDevice& device, const QueueState& state, bool registration, bool* deferred = nullptr) {
    using Stage = ShaderRecompiler::ShaderStage;
    const auto header = ReadHeader(snapshot);
    std::uint32_t programRegister;
    std::uint32_t resourceRegister;
    std::uint32_t firstUser = 0;
    Stage stage;
    switch (snapshot.type) {
    case 0: stage = Stage::Compute; programRegister = 0x20c; resourceRegister = 0x213; break;
    case 1: stage = Stage::Fragment; programRegister = 0x008; resourceRegister = 0x00b; break;
    case 2: stage = Stage::Vertex; programRegister = 0x0c8; resourceRegister = 0x08b; firstUser = 8; break;
    case 4: stage = Stage::Mesh; programRegister = 0x0c8; resourceRegister = 0x08b; break;
    case 5: stage = Stage::Local; programRegister = 0x148; resourceRegister = 0x10b; firstUser = 8; break;
    case 6: stage = Stage::Mesh; programRegister = 0x088; resourceRegister = 0x08b; break;
    case 7: stage = Stage::TessellationControl; programRegister = 0x108; resourceRegister = 0x10b; break;
    default:
        if (registration) {
            APS5_LOG_ERR("Shader 0x%llx has unsupported registered shader type %u; it is not prepared at registration", static_cast<unsigned long long>(snapshot.codeAddress), static_cast<unsigned>(snapshot.type));
            return nullptr;
        }
        throw std::runtime_error("AGC driver: unsupported registered shader type " + std::to_string(snapshot.type));
    }
    const auto defer = [&](const char* reason) {
        APS5_LOG_ERR("Shader 0x%llx could not be prepared at registration (%s); preparing it when it is used", static_cast<unsigned long long>(snapshot.codeAddress), reason);
        *deferred = true;
        return std::unique_ptr<RegisteredPreparation>{};
    };
    if (deferred != nullptr && (!state.shader.contains(programRegister) || !state.shader.contains(programRegister + 1))) return defer("its program address is not in the shader registers");
    const auto high = RegisterValue(state.shader, programRegister + 1);
    if ((high & ~0xffu) != 0) throw std::runtime_error("AGC driver: invalid registered program address");
    const auto address = (static_cast<std::uint64_t>(RegisterValue(state.shader, programRegister)) << 8u) | (static_cast<std::uint64_t>(high) << 40u);
    if (address < snapshot.codeAddress || address - snapshot.codeAddress >= snapshot.code.size() * 4u) throw std::runtime_error("AGC driver: registered entry point is outside shader code");
    const auto codeOffset = static_cast<std::size_t>((address - snapshot.codeAddress) / 4u);
    const auto code = std::span(snapshot.code).subspan(codeOffset);
    auto plan = std::make_unique<RegisteredPreparation>();
    plan->codeOffset = codeOffset;
    try {
        plan->decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(code);
    } catch (const ShaderRecompiler::UnsupportedInstructionError& error) {
        if (deferred == nullptr) throw;
        return defer(error.what());
    }
    std::optional<ShaderRecompiler::ShaderComputeStageInfo> compute;
    std::optional<ShaderRecompiler::ShaderPixelStageInfo> pixel;
    std::optional<ShaderRecompiler::ShaderVertexStageInfo> vertex;
    std::optional<ShaderRecompiler::GraphicsCompileContext> graphics;
    std::uint32_t wave;
    const auto resources = RegisterValue(state.shader, resourceRegister);
    auto userCount = (resources >> 1u) & 0x1fu;
    if (stage != Stage::Compute) userCount |= ((resources >> 27u) & 1u) << 5u;
    if (userCount > 32) throw std::runtime_error("AGC driver: static user SGPR count exceeds the register bank");
    if (stage == Stage::Compute) {
        compute = Graphics::DecodeComputeStageInfo(state.shader, snapshot.header);
        if (header.specials == nullptr) {
            if (deferred != nullptr) return defer("its header has no specials to give its wave size");
            return {};
        }
        const auto special = ReadHeaderArray(snapshot, header.specials, 1).front();
        wave = (special.dispatch_modifier & 0x8000u) != 0 ? 32u : 64u;
    } else if (stage == Stage::Fragment) {
        const auto count = RegisterValue(state.context, 0x1b6) & 0x3fu;
        if (registration && count != 0) return nullptr;
        pixel = Graphics::DecodePixelStageInfo(state.context, {});
        wave = pixel->wave32 ? 32u : 64u;
    } else {
        const auto routing = RegisterValue(state.context, 0x2d5);
        wave = (routing & 0x00400000u) != 0 ? 32u : 64u;
        if ((routing & 0x20u) != 0 || stage == Stage::Mesh || (routing & 4u) != 0) {
            if (registration) return nullptr;
            auto stageState = state;
            stageState.context[0x1b6] = 0;
            const auto stages = Graphics::DecodeShaderStages(stageState);
            graphics = ShaderRecompiler::GraphicsCompileContext{0, {}, stages.mesh, stages.tessellation, {}};
            if (stages.mesh) { stage = Stage::Mesh; firstUser = 0; }
            if (stages.tessellation && snapshot.type == 2) stage = Stage::TessellationEvaluation;
        }
        if (firstUser == 0) userCount += 8;
    }
    plan->userData.resize(userCount);
    if (stage != Stage::Compute && stage != Stage::Fragment && snapshot.type != 6) vertex = Graphics::DecodeVertexStageInfo(snapshot.header, snapshot.headerAddress, plan->userData, nullptr, true);
    plan->swappc = {vertex.has_value(), firstUser, userCount};
    if (deferred != nullptr) {
        if (const auto counter = ShaderRecompiler::UnresolvableSwappcTarget(plan->decoded, &plan->swappc)) {
            char reason[96];
            std::snprintf(reason, sizeof(reason), "its s_swappc_b64 at program counter 0x%x calls a computed target", *counter);
            return defer(reason);
        }
    }
    plan->memory = {{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}};
    auto& request = plan->request.emplace(ShaderRecompiler::RecompileRequest{{stage, address, code, snapshot.headerAddress, snapshot.header}, {wave, firstUser, plan->userData, compute, pixel, vertex, plan->memory, RegisteredFloatMode(snapshot)}, stage == Stage::Compute ? device.ComputeTarget(wave) : device.Target(), {0, 0, 0, 128}, graphics});
    if (graphics && graphics->mesh) request.layout.pushConstantSizeBytes = ShaderRecompiler::MeshDrawPushOffsetBytes;
    if (pixel && device.GraphicsPipelineLibraries()) request.layout.pushConstantOffsetBytes = Graphics::FixedPushOffset(Stage::Fragment);
    return plan;
}

std::vector<PreparedShaders::Entry> PrepareRegistered(RegisteredPreparation& plan) {
    auto& request = *plan.request;
    std::vector<PreparedShaders::Entry> entries;
    ShaderRecompiler::ShaderPreparationContext preparation;
    const auto append = [&] {
        PerformanceTimer timing("Shader.PrepareArtifact");
        entries.push_back({plan.codeOffset, PrepareShaderWithDiagnostics(request, &preparation)});
    };
    append();
    if (request.context.compute) {
        request.context.compute->partialThreads = {1, 1, 1};
        append();
    } else if (request.context.pixel) {
        request.layout.pushConstantSizeBytes = 0;
        append();
    }
    return entries;
}

std::vector<PreparedShaders::Entry> PrepareRegistered(const ShaderSnapshot& snapshot, const VulkanDevice& device, const QueueState& state, bool registration, bool* deferred = nullptr) {
    const auto plan = PlanRegistered(snapshot, device, state, registration, deferred);
    return plan != nullptr ? PrepareRegistered(*plan) : std::vector<PreparedShaders::Entry>{};
}

class RegistrationPreparePool {
public:
    static RegistrationPreparePool& Instance() {
        static auto* pool = new RegistrationPreparePool();
        return *pool;
    }

    void Submit(std::function<void()> task) {
        if (threads == 0) {
            task();
            return;
        }
        {
            std::lock_guard lock(mutex);
            tasks.push_back(std::move(task));
        }
        ready.notify_one();
    }

private:
    RegistrationPreparePool() {
        const auto count = std::max(2u, std::thread::hardware_concurrency() / 2u);
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setstacksize(&attributes, 64u << 20u);
        for (unsigned index = 0; index < count; ++index) {
            pthread_t thread;
            if (pthread_create(&thread, &attributes, &RegistrationPreparePool::Main, this) == 0) {
                pthread_detach(thread);
                ++threads;
            }
        }
        pthread_attr_destroy(&attributes);
    }

    static void* Main(void* argument) {
        auto& pool = *static_cast<RegistrationPreparePool*>(argument);
        for (;;) {
            std::function<void()> task;
            {
                std::unique_lock lock(pool.mutex);
                pool.ready.wait(lock, [&] { return !pool.tasks.empty(); });
                task = std::move(pool.tasks.front());
                pool.tasks.pop_front();
            }
            task();
        }
        return nullptr;
    }

    std::mutex mutex;
    std::condition_variable ready;
    std::deque<std::function<void()>> tasks;
    unsigned threads = 0;
};

bool AsyncRegistrationPrepare() {
    static const bool sync = std::getenv("APS5_SYNC_SHADER_PREPARE") != nullptr;
    return !sync;
}

void PrepareRegisteredInBackground(std::shared_ptr<const ShaderSnapshot> snapshot, std::shared_ptr<RegisteredPreparation> plan) {
    RegistrationPreparePool::Instance().Submit([snapshot = std::move(snapshot), plan = std::move(plan)] {
        PerformanceContext timingContext(FrameTiming::Preparation());
        std::vector<PreparedShaders::Entry> entries;
        std::exception_ptr failure;
        try {
            entries = PrepareRegistered(*plan);
        } catch (const std::exception& error) {
            APS5_LOG_ERR("Shader 0x%llx could not be prepared after registration: %s", static_cast<unsigned long long>(snapshot->codeAddress), error.what());
            failure = std::current_exception();
        } catch (...) {
            failure = std::current_exception();
        }
        auto& prepared = *snapshot->prepared;
        {
            std::lock_guard lock(prepared.mutex);
            prepared.entries.insert(prepared.entries.end(), std::make_move_iterator(entries.begin()), std::make_move_iterator(entries.end()));
            if (failure == nullptr && !plan->abiKey.empty()) prepared.registeredAbis.push_back(std::move(plan->abiKey));
            prepared.failure = failure;
            prepared.pending = false;
        }
        prepared.settled.notify_all();
    });
}

}

std::vector<PreparedGraphicsStage> PrepareGraphicsStages(const DrawDecode& decoded, const ShaderRecompiler::SpirvTarget& target) {
    ShaderPreparationTransaction transaction;
    require(decoded.programs.size() == decoded.roles.size(), "graphics ABI program roles are incomplete");
    for (const auto& program : decoded.programs) require(program.snapshot != nullptr, "graphics ABI has no registered shader snapshot");
    std::vector<ShaderRecompiler::ProgramRole> expected;
    if (decoded.state.stages.path == Graphics::ShaderPath::Tessellation) expected = {ShaderRecompiler::ProgramRole::Local, ShaderRecompiler::ProgramRole::Hull, ShaderRecompiler::ProgramRole::Domain};
    else {
        expected.push_back(ShaderRecompiler::ProgramRole::Main);
        if (decoded.state.stages.path == Graphics::ShaderPath::Geometry && !decoded.programs.empty() && decoded.programs.front().snapshot->type == 4) expected.push_back(ShaderRecompiler::ProgramRole::GeometryBack);
    }
    if (!decoded.roles.empty() && decoded.roles.back() == ShaderRecompiler::ProgramRole::Fragment) expected.push_back(ShaderRecompiler::ProgramRole::Fragment);
    require(expected == decoded.roles, "graphics ABI linked programs do not match the stage routing");
    std::vector<ShaderRecompiler::LinkedProgram> linked;
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    for (std::size_t index = 0; index < decoded.programs.size(); ++index) {
        const auto& program = decoded.programs[index];
        require(program.snapshot != nullptr && program.codeOffset < program.snapshot->code.size(), "graphics ABI has an invalid registered entry point");
        const auto code = std::span(program.snapshot->code).subspan(program.codeOffset);
        require(program.binary.code.data() == code.data() && program.binary.code.size() == code.size() && program.binary.codeAddress == program.snapshot->codeAddress + program.codeOffset * sizeof(std::uint32_t), "graphics ABI entry point differs from its snapshot");
        linked.push_back({decoded.roles[index], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
        memory.insert(memory.end(), program.memory.begin(), program.memory.end());
    }
    std::vector<PreparedGraphicsStage> prepared;
    std::uint32_t pushOffset = 0;
    const bool fixedSlots = target.fixedPushSlots;
    const auto capacity = decoded.state.stages.mesh ? ShaderRecompiler::MeshDrawPushOffsetBytes : Graphics::PipelinePushSlotBytes;
    for (std::size_t index = 0; index < decoded.programs.size(); ++index) {
        if (decoded.roles[index] == ShaderRecompiler::ProgramRole::GeometryBack) continue;
        const auto& program = decoded.programs[index];
        const bool fragment = program.binary.stage == ShaderRecompiler::ShaderStage::Fragment;
        const auto wave = fragment ? decoded.state.stages.fragmentWaveSize : decoded.state.stages.vertexWaveSize;
        std::optional<ShaderRecompiler::ShaderVertexStageInfo> vertex;
        if (!fragment) vertex = Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, nullptr, true);
        pushOffset = Graphics::StagePushOffset(pushOffset, program.binary.stage, fixedSlots);
        const auto offset = pushOffset;
        const auto room = (fixedSlots && fragment ? Graphics::PipelinePushSlotBytes : capacity) - pushOffset % Graphics::PipelinePushSlotBytes;
        ShaderRecompiler::RecompileRequest request{program.binary, {wave, program.firstUserSgpr, program.userData, {}, fragment ? std::optional(decoded.pixel) : std::nullopt, vertex, memory, RegisteredFloatMode(*program.snapshot)}, target, {0, 0, offset, room}, ShaderRecompiler::GraphicsCompileContext{program.firstUserSgpr, linked, decoded.state.stages.mesh, decoded.state.stages.tessellation, {}}};
        std::vector<std::uint64_t> key;
        ShaderRecompiler::BuildPreparedShaderKey(request, key);
        std::shared_ptr<const ShaderRecompiler::SourceHandle> handle;
        for (const auto& entry : transaction.Read(*program.snapshot).entries) {
            if (entry.codeOffset == program.codeOffset && ShaderRecompiler::MatchesPreparedShader(request, *entry.handle, key)) {
                handle = entry.handle;
                break;
            }
        }
        if (handle == nullptr) {
            PerformanceTimer timing("Shader.PrepareArtifact");
            handle = PrepareShaderWithDiagnostics(request);
        }
        const auto bytes = handle->artifact->bindings.pushConstantSizeBytes;
        require(bytes <= room, "prepared graphics stages exceed the push constant block");
        prepared.push_back({program.snapshot, {program.codeOffset, handle}});
        pushOffset += bytes;
    }
    transaction.Commit();
    return prepared;
}

void Driver::ResolveGraphicsStagesAbi(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    PerformanceContext timingContext(FrameTiming::Preparation());
    PerformanceTimer timing("Shader.ResolveGraphicsStagesAbi");
    ShaderPreparationTransaction transaction;
    CheckFailure();
    require(!stages.empty(), "graphics ABI has no shader headers");
    QueueState state{};
    std::shared_ptr<const ShaderRegistry> registry;
    std::shared_ptr<const ShaderSnapshot> owner;
    {
        std::lock_guard lock(mutex);
        registry = shaders;
        for (const auto* shader : stages) {
            GuestMemory::CheckRange(shader, sizeof(Shader), alignof(Shader));
            const auto address = reinterpret_cast<std::uintptr_t>(const_cast<const void*>(shader->code));
            require(registry != nullptr && registry->contains(address), "graphics ABI refers to an unregistered shader");
            if (owner == nullptr) owner = registry->at(address);
            const auto& snapshot = *registry->at(address);
            require(SameHeader(snapshot, shader), "graphics ABI refers to a replaced shader header");
            require(snapshot.registeredState != nullptr, "registered shader state is missing");
            const auto& registered = *snapshot.registeredState;
            for (const auto& [offset, value] : registered.shader) state.shader.insert_or_assign(offset, value);
            for (const auto& [offset, value] : registered.context) state.context.insert_or_assign(offset, value);
            for (const auto& [offset, value] : registered.userConfig) state.userConfig.insert_or_assign(offset, value);
        }
    }
    for (const auto reg : context) state.context.insert_or_assign(reg.offset, reg.value);
    for (const auto reg : primitive) state.userConfig.insert_or_assign(reg.offset, reg.value);
    const auto localDevice = device.Load();
    require(localDevice != nullptr, "graphics ABI device is missing");
    struct GraphicsAbiKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, GraphicsAbiKeyStorage>();
    BuildRegisteredAbiKey(state, *localDevice, key);
    const auto primitiveType = state.userConfig.find(0x242u);
    for (const auto& abi : transaction.Read(*owner).graphicsAbis) {
        if (abi.key != key || abi.registry.lock() != registry) continue;
        for (const auto& stage : abi.stages) {
            const auto snapshot = stage.lock();
            require(snapshot != nullptr, "prepared graphics ABI lost a registered stage");
            if (snapshot->type != 1) ResolvePreparedGraphics(*snapshot, {}, primitiveType == state.userConfig.end() ? 0u : primitiveType->second, localDevice->Target());
        }
        transaction.Commit();
        return;
    }
    DrawDecode decoded{};
    decoded.state.stages = Graphics::DecodeShaderStages(state);
    DecodeGraphicsPrograms(decoded, state, *registry, true, false);
    auto prepared = PrepareGraphicsStages(decoded, localDevice->Target());
    for (auto& stage : prepared) {
        const auto& entries = transaction.Read(*stage.snapshot).entries;
        const auto duplicate = std::ranges::any_of(entries, [&](const auto& existing) { return existing.codeOffset == stage.entry.codeOffset && existing.handle->artifact == stage.entry.handle->artifact; });
        if (!duplicate) transaction.Edit(*stage.snapshot).entries.push_back(std::move(stage.entry));
    }
    for (const auto& stage : prepared) {
        if (stage.snapshot->type == 1) continue;
        ResolvePreparedGraphics(*stage.snapshot, {}, primitiveType == state.userConfig.end() ? 0u : primitiveType->second, localDevice->Target());
    }
    PreparedShaderState::GraphicsAbi abi{key, registry, {}};
    for (const auto& stage : prepared) abi.stages.push_back(stage.snapshot);
    auto& graphicsAbis = transaction.Edit(*owner).graphicsAbis;
    std::erase_if(graphicsAbis, [](const auto& entry) { return entry.registry.expired(); });
    graphicsAbis.push_back(std::move(abi));
    transaction.Commit();
}

void Driver::ResolveShaderAbi(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    PerformanceContext timingContext(FrameTiming::Preparation());
    PerformanceTimer timing("Shader.ResolveAbi");
    ShaderPreparationTransaction transaction;
    CheckFailure();
    GuestMemory::CheckRange(shader, sizeof(Shader), alignof(Shader));
    if (shader->type != 0 && shader->type != 1) {
        const std::array<const Shader*, 1> stages{shader};
        ResolveGraphicsStagesAbi(stages, context, primitive);
        transaction.Commit();
        return;
    }
    std::shared_ptr<const ShaderSnapshot> snapshot;
    {
        std::lock_guard lock(mutex);
        const auto address = reinterpret_cast<std::uintptr_t>(const_cast<const void*>(shader->code));
        require(shaders != nullptr && shaders->contains(address), "static ABI refers to an unregistered shader");
        snapshot = shaders->at(address);
        require(SameHeader(*snapshot, shader), "static ABI refers to a replaced shader header");
    }
    require(snapshot->registeredState != nullptr, "registered shader state is missing");
    QueueState state{};
    state.shader = snapshot->registeredState->shader;
    state.context = snapshot->registeredState->context;
    state.userConfig = snapshot->registeredState->userConfig;
    for (const auto reg : context) state.context[reg.offset] = reg.value;
    for (const auto reg : primitive) state.userConfig[reg.offset] = reg.value;
    const auto localDevice = device.Load();
    require(localDevice != nullptr, "shader registration device is missing");
    struct RegisteredAbiKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, RegisteredAbiKeyStorage>();
    BuildRegisteredAbiKey(state, *localDevice, key);
    const auto& registeredAbis = transaction.Read(*snapshot).registeredAbis;
    if (std::ranges::find(registeredAbis, key) != registeredAbis.end()) {
        transaction.Commit();
        return;
    }
    auto entries = PrepareRegistered(*snapshot, *localDevice, state, false);
    auto& prepared = transaction.Edit(*snapshot);
    for (auto& entry : entries) {
        const auto duplicate = std::ranges::any_of(prepared.entries, [&](const auto& existing) { return existing.handle->artifact == entry.handle->artifact; });
        if (!duplicate) prepared.entries.push_back(std::move(entry));
    }
    prepared.registeredAbis.push_back(key);
    transaction.Commit();
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

void Driver::ResolveGraphicsAbi(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    PerformanceContext timingContext(FrameTiming::Preparation());
    PerformanceTimer timing("Shader.ResolveGraphicsAbi");
    CheckFailure();
    if (primitiveType != 0 && primitiveType != 7 && primitiveType != 17) return;
    ShaderPreparationTransaction transaction;
    require(vertex != nullptr && pixel != nullptr, "rectangle ABI requires vertex and fragment shaders");
    std::shared_ptr<const ShaderSnapshot> front;
    std::shared_ptr<const ShaderSnapshot> fragment;
    {
        std::lock_guard lock(mutex);
        GuestMemory::CheckRange(vertex, sizeof(Shader), alignof(Shader));
        GuestMemory::CheckRange(pixel, sizeof(Shader), alignof(Shader));
        const auto lookup = [&](const Shader* shader) {
            const auto address = reinterpret_cast<std::uintptr_t>(const_cast<const void*>(shader->code));
            require(shaders != nullptr && shaders->contains(address), "rectangle ABI refers to an unregistered shader");
            const auto snapshot = shaders->at(address);
            require(SameHeader(*snapshot, shader), "rectangle ABI refers to a replaced shader header");
            return snapshot;
        };
        front = lookup(vertex);
        fragment = lookup(pixel);
    }
    require(front != fragment, "rectangle stages refer to the same shader");
    const auto localDevice = device.Load();
    require(localDevice != nullptr, "shader registration device is missing");
    ResolvePreparedGraphics(*front, fragment, primitiveType, localDevice->Target());
    transaction.Commit();
}

void ResolvePreparedGraphics(const ShaderSnapshot& front, const std::shared_ptr<const ShaderSnapshot>& fragment, std::uint32_t primitiveType, const ShaderRecompiler::SpirvTarget& target) {
    ShaderPreparationTransaction transaction;
    require(fragment.get() != &front, "rectangle stages refer to the same shader");
    std::vector<std::shared_ptr<const ShaderSnapshot>> fragments;
    {
        const auto& current = transaction.Read(front);
        const bool newFragment = fragment && !std::ranges::any_of(current.fragments, [&](const auto& entry) { return entry.lock() == fragment; });
        const bool newRequest = !current.rectangleRequested && (primitiveType == 7 || primitiveType == 17);
        const bool expiredFragment = std::ranges::any_of(current.fragments, [](const auto& entry) { return entry.expired(); });
        if (newFragment || newRequest || expiredFragment) {
            auto& prepared = transaction.Edit(front);
            std::erase_if(prepared.fragments, [](const auto& entry) { return entry.expired(); });
            std::erase_if(prepared.rectangleProgress, [](const auto& entry) { return entry.fragment.expired(); });
            if (newFragment) prepared.fragments.push_back(fragment);
            prepared.rectangleRequested |= newRequest;
        }
        const auto& prepared = transaction.Read(front);
        if (!prepared.rectangleRequested) {
            transaction.Commit();
            return;
        }
        for (const auto& entry : prepared.fragments) {
            if (auto snapshot = entry.lock()) fragments.push_back(std::move(snapshot));
        }
    }
    for (const auto& pixel : fragments) {
        const auto& current = transaction.Read(front);
        const auto& pixelPrepared = transaction.Read(*pixel);
        require(!current.entries.empty() && !pixelPrepared.entries.empty(), "rectangle ABI has missing stage artifacts");
        const auto progress = std::ranges::find_if(current.rectangleProgress, [&](const auto& entry) { return entry.fragment.lock() == pixel; });
        const auto vertexCount = progress == current.rectangleProgress.end() ? 0u : progress->vertexCount;
        const auto fragmentCount = progress == current.rectangleProgress.end() ? 0u : progress->fragmentCount;
        if (vertexCount == current.entries.size() && fragmentCount == pixelPrepared.entries.size()) continue;
        auto& prepared = transaction.Edit(front);
        std::vector<PreparedShaders::Rectangle> rectangles;
        for (std::size_t vertexIndex = 0; vertexIndex < prepared.entries.size(); ++vertexIndex) {
            const auto& vertexEntry = prepared.entries[vertexIndex];
            for (std::size_t fragmentIndex = vertexIndex < vertexCount ? fragmentCount : 0u; fragmentIndex < pixelPrepared.entries.size(); ++fragmentIndex) {
                const auto& fragmentEntry = pixelPrepared.entries[fragmentIndex];
                const auto& vertexArtifact = ShaderRecompiler::GetPreparedArtifact(*vertexEntry.handle);
                const auto& fragmentArtifact = ShaderRecompiler::GetPreparedArtifact(*fragmentEntry.handle);
                const auto exists = [&](const auto& entry) { return entry.vertexId == vertexArtifact.variantId && entry.fragmentId == fragmentArtifact.variantId; };
                if (std::ranges::any_of(prepared.rectangles, exists) || std::ranges::any_of(rectangles, exists)) continue;
                ShaderRecompiler::RecompileResult vertexResult;
                ShaderRecompiler::RecompileResult fragmentResult;
                static_cast<ShaderRecompiler::CompiledShaderArtifact&>(vertexResult) = vertexArtifact;
                static_cast<ShaderRecompiler::CompiledShaderArtifact&>(fragmentResult) = fragmentArtifact;
                rectangles.push_back({vertexArtifact.variantId, fragmentArtifact.variantId, std::make_shared<const ShaderRecompiler::RectListShaders>(ShaderRecompiler::BuildRectListShaders(vertexResult, fragmentResult, target))});
            }
        }
        prepared.rectangles.insert(prepared.rectangles.end(), std::make_move_iterator(rectangles.begin()), std::make_move_iterator(rectangles.end()));
        std::erase_if(prepared.rectangleProgress, [&](const auto& entry) { return entry.fragment.expired() || entry.fragment.lock() == pixel; });
        prepared.rectangleProgress.push_back({pixel, prepared.entries.size(), pixelPrepared.entries.size()});
    }
    transaction.Commit();
}

ShaderSnapshot PrepareNullPixelProgram(const VulkanDevice& device) {
    ShaderSnapshot null{NullPixelProgramAddress(), reinterpret_cast<std::uintptr_t>(&NullPixelShader), NullPixelShader.type, {}, {}};
    null.code.assign(std::begin(NullPixelCode), std::end(NullPixelCode));
    null.header.resize(sizeof(Shader));
    std::memcpy(null.header.data(), &NullPixelShader, sizeof(Shader));
    auto nullRegisteredState = DecodeRegisteredState(null);
    nullRegisteredState.shader.insert_or_assign(0x008u, static_cast<std::uint32_t>(null.codeAddress >> 8u));
    nullRegisteredState.shader.insert_or_assign(0x009u, static_cast<std::uint32_t>(null.codeAddress >> 40u));
    nullRegisteredState.shader.insert_or_assign(0x00bu, 0u);
    nullRegisteredState.context.insert_or_assign(0x1b3u, 0x2u);
    nullRegisteredState.context.insert_or_assign(0x1b4u, 0x2u);
    null.registeredState = std::make_shared<const RegisteredShaderState>(std::move(nullRegisteredState));
    QueueState nullState{};
    nullState.shader = null.registeredState->shader;
    nullState.context = null.registeredState->context;
    nullState.userConfig = null.registeredState->userConfig;
    null.prepared->entries = PrepareRegistered(null, device, nullState, true);
    return null;
}

void Driver::RegisterShader(const Shader* shader) {
    PerformanceContext timingContext(FrameTiming::Preparation());
    PerformanceTimer timing("Shader.Register");
    ShaderPreparationTransaction transaction;
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

    {
        std::lock_guard lock(mutex);
        rethrowFailure();
        if (shaders != nullptr) {
            const auto found = shaders->find(snapshot.codeAddress);
            if (found != shaders->end()) {
                const auto& current = *found->second;
                if (current.headerAddress == snapshot.headerAddress && current.type == snapshot.type && current.code == snapshot.code && current.header == snapshot.header) {
                    transaction.Commit();
                    return;
                }
            }
        }
    }

    std::shared_ptr<VulkanDevice> localDevice = device.Load();
    if (localDevice == nullptr) {
        std::lock_guard gpuLock(GuestMemory::GpuMutex());
        if (device == nullptr) device = std::make_shared<VulkanDevice>();
        localDevice = device;
    }
    snapshot.registeredState = std::make_shared<const RegisteredShaderState>(DecodeRegisteredState(snapshot));
    QueueState registered{};
    registered.shader = snapshot.registeredState->shader;
    registered.context = snapshot.registeredState->context;
    registered.userConfig = snapshot.registeredState->userConfig;
    std::shared_ptr<RegisteredPreparation> preparation;
    if (AsyncRegistrationPrepare()) {
        preparation = PlanRegistered(snapshot, *localDevice, registered, true, &snapshot.prepared->deferred);
        if (preparation != nullptr && (snapshot.type == 0 || snapshot.type == 1)) BuildRegisteredAbiKey(registered, *localDevice, preparation->abiKey);
        snapshot.prepared->pending = preparation != nullptr;
    } else {
        snapshot.prepared->entries = PrepareRegistered(snapshot, *localDevice, registered, true, &snapshot.prepared->deferred);
        if ((snapshot.type == 0 || snapshot.type == 1) && !snapshot.prepared->entries.empty()) {
            std::vector<std::uint64_t> key;
            BuildRegisteredAbiKey(registered, *localDevice, key);
            snapshot.prepared->registeredAbis.push_back(std::move(key));
        }
    }
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
    const auto published = std::make_shared<const ShaderSnapshot>(std::move(snapshot));
    std::lock_guard lock(mutex);
    rethrowFailure();
    PublishRegisteredShader(shaders, published);
    if (shaders->find(NullPixelProgramAddress()) == shaders->end()) {
        PublishRegisteredShader(shaders, std::make_shared<const ShaderSnapshot>(PrepareNullPixelProgram(*localDevice)));
    }
    transaction.Commit();
    if (preparation != nullptr) PrepareRegisteredInBackground(published, std::move(preparation));
}

}

extern "C" AgcDriver::DriverDetail::ShaderPreparationTransaction* AgcDriverBeginShaderPreparation_nid_postfix() {
    return new AgcDriver::DriverDetail::ShaderPreparationTransaction();
}

extern "C" void AgcDriverCommitShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction) {
    transaction->Commit();
}

extern "C" void AgcDriverEndShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction) noexcept {
    delete transaction;
}
