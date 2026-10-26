#include "prx/libc/include/general/LogMacros.hpp"
#include "ControlFlow/GraphBuilder.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/PipelineLibrary.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Diagnostics.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "CacheKey.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparationScope.hpp"
#include "CompiledVariant.hpp"
#include "Optimization/ResourceMaterializer.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <set>
#include <condition_variable>
#include <cstdio>
#include <cstddef>
#include <cstdint>
#include <span>
#include <cstdlib>
#include <cstring>
#include <deque>
#include <exception>
#include <functional>
#include <iterator>
#include <list>
#include <map>
#include <mutex>
#include <atomic>
#include <pthread.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <process.h>
#endif
#ifdef __GLIBC__
#include <malloc.h>
#endif
#include <set>
#include <sstream>
#include <stdexcept>
#include <thread>
#include "Optimization/ShaderStageInputInfo.hpp"
#include <spirv/unified1/spirv.hpp>
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"

namespace AgcDriver::DriverDetail {

std::shared_ptr<const ShaderRecompiler::SourceHandle> PrepareShaderWithDiagnostics(const ShaderRecompiler::RecompileRequest& request, ShaderRecompiler::ShaderPreparationContext* preparation) {
    std::exception_ptr failure;
    std::string dump;
    try {
        auto handle = ShaderRecompiler::PrepareShader(request, preparation);
        return handle;
    } catch (...) {
        failure = std::current_exception();
        try {
            if (std::getenv("APS5_DUMP_SHADERS") != nullptr) dump = Driver::dumpRequest(request.shader.codeAddress, request, ".prepare");
        } catch (...) {}
    }
    try {
        std::rethrow_exception(failure);
    } catch (const std::exception& error) {
        std::ostringstream where;
        where << "shader 0x" << std::hex << request.shader.codeAddress << " stage " << static_cast<unsigned>(request.shader.stage);
        if (!dump.empty()) where << " (" << dump << ")";
        where << ": ";
        throw std::runtime_error(where.str() + error.what());
    }
}

std::shared_ptr<const ShaderSnapshot> ReadRawShader(std::uint64_t address) {
    const auto drain = [](std::uint64_t first, std::size_t bytes) {
        if (!DrawPipeline::Active()) return;
        auto& pipeline = DrawPipeline::Queue0();
        if (pipeline.Busy() && pipeline.Overlaps(first, bytes)) pipeline.Drain(DrawPipeline::DrainReason::Capture);
    };
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
        drain(address, code.size());
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
        drain(address + previous * sizeof(std::uint32_t), (snapshot.code.size() - previous) * sizeof(std::uint32_t));
        GuestMemory::Read(address + previous * sizeof(std::uint32_t),
            std::as_writable_bytes(std::span(snapshot.code).subspan(previous)), alignof(std::uint32_t));
        try {
            const auto decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(snapshot.code);
            if (const auto tail = ShaderRecompiler::FindUserDataTail(decoded, 0u, 106u)) snapshot.userDataTailRegister = tail->userDataRegister;
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
    throw std::runtime_error("AGC driver: raw shader program has no reachable end within mapped code or the size limit");
}

std::shared_ptr<const ShaderSnapshot> ReadUserDataTailContinuation(std::span<const std::uint32_t> code, std::uint32_t userDataBase, std::span<const std::uint32_t> userData) {
    const auto program = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(code);
    const auto tail = ShaderRecompiler::FindUserDataTail(program, userDataBase, static_cast<std::uint32_t>(userData.size()));
    if (!tail) return nullptr;
    const auto slot = tail->userDataRegister - userDataBase;
    const auto high = tail->constantHigh ? *tail->constantHigh : userData[slot + 1u];
    const auto address = userData[slot] | (static_cast<std::uint64_t>(high) << 32u);
    require(address != 0 && (address & 3u) == 0u, "user-data tail continuation address is null or not dword aligned");
    return ReadRawShader(address);
}

std::shared_ptr<const ShaderSnapshot> ProgramSnapshot(const ShaderRegistry& shaders, std::uint64_t address, std::initializer_list<std::uint8_t> types) {
    if (auto registered = RegisteredProgram(shaders, address, types)) return registered;
    return ReadRawShader(address);
}

namespace {
std::atomic<bool> rawGraphicsDrawn{false};
std::mutex rawGraphicsMutex;
std::map<std::uint64_t, std::uint64_t> rawGraphicsRanges;
}

void NoteRawGraphicsDraw(const ShaderSnapshot& snapshot) {
    if (snapshot.rawGraphicsNoted->load(std::memory_order_acquire)) return;
    auto begin = snapshot.codeAddress;
    auto end = snapshot.codeAddress + snapshot.code.size() * sizeof(std::uint32_t);
    {
        std::lock_guard lock(rawGraphicsMutex);
        auto next = rawGraphicsRanges.upper_bound(begin);
        if (next != rawGraphicsRanges.begin() && std::prev(next)->second >= begin) {
            const auto previous = std::prev(next);
            begin = previous->first;
            end = std::max(end, previous->second);
            rawGraphicsRanges.erase(previous);
        }
        while (next != rawGraphicsRanges.end() && next->first <= end) {
            end = std::max(end, next->second);
            next = rawGraphicsRanges.erase(next);
        }
        rawGraphicsRanges.emplace(begin, end);
    }
    snapshot.rawGraphicsNoted->store(true, std::memory_order_release);
    rawGraphicsDrawn.store(true, std::memory_order_relaxed);
}

bool RawGraphicsDrawn() {
    return rawGraphicsDrawn.load(std::memory_order_relaxed);
}

bool RawGraphicsDrawnIn(std::uint64_t begin, std::uint64_t end) {
    if (begin >= end) return false;
    std::lock_guard lock(rawGraphicsMutex);
    const auto next = rawGraphicsRanges.lower_bound(end);
    return next != rawGraphicsRanges.begin() && std::prev(next)->second > begin;
}

namespace {

void AwaitRegisteredPreparation(PreparedShaders& prepared, std::unique_lock<std::mutex>& lock) {
    if (prepared.pending) PrioritizeRegistrationPreparation(prepared);
    prepared.settled.wait(lock, [&] { return !prepared.pending; });
    if (prepared.failure != nullptr) std::rethrow_exception(prepared.failure);
}

bool CommittedAbi(const ShaderSnapshot& snapshot, std::span<const std::uint64_t> key) {
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    return std::ranges::any_of(snapshot.prepared->registeredAbis, [&](const auto& abi) { return std::ranges::equal(abi, key); });
}

std::optional<std::vector<std::weak_ptr<const ShaderSnapshot>>> CommittedGraphicsAbi(const ShaderSnapshot& owner, std::span<const std::uint64_t> key, const std::shared_ptr<const ShaderRegistry>& registry) {
    std::unique_lock lock(owner.prepared->mutex);
    AwaitRegisteredPreparation(*owner.prepared, lock);
    const auto& abis = owner.prepared->graphicsAbis;
    const auto found = std::ranges::find_if(abis, [&](const auto& abi) { return std::ranges::equal(abi.key, key) && abi.registry.lock() == registry; });
    if (found == abis.end()) return std::nullopt;
    return found->stages;
}

bool CommittedLink(const ShaderSnapshot& front, const std::shared_ptr<const ShaderSnapshot>& fragment, std::uint32_t primitiveType) {
    if (primitiveType == 7 || primitiveType == 17) return false;
    std::unique_lock lock(front.prepared->mutex);
    AwaitRegisteredPreparation(*front.prepared, lock);
    const auto& fragments = front.prepared->fragments;
    if (front.prepared->rectangleRequested || std::ranges::any_of(fragments, [](const auto& entry) { return entry.expired(); })) return false;
    return fragment == nullptr || std::ranges::any_of(fragments, [&](const auto& entry) { return entry.lock() == fragment; });
}

}

bool ProgramUsesWorkgroup(const ShaderSnapshot& snapshot, std::size_t codeOffset) {
    constexpr std::uint8_t Unknown = 0, Unused = 1, Used = 2;
    if (codeOffset == 0) {
        const auto cached = snapshot.workgroupUse->load(std::memory_order_acquire);
        if (cached != Unknown) return cached == Used;
    }
    const auto decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(std::span(snapshot.code).subspan(codeOffset));
    const bool used = std::any_of(decoded.instructions.begin(), decoded.instructions.end(), [](const auto& instruction) {
        return instruction.family == ShaderRecompiler::RdnaInstructionFamily::DS || instruction.op == ShaderRecompiler::RdnaOpcode::SBarrier;
    });
    if (codeOffset == 0) snapshot.workgroupUse->store(used ? Used : Unused, std::memory_order_release);
    return used;
}

namespace {

bool readOnlyMemoryOp(ShaderRecompiler::RdnaOpcode op) {
    using Op = ShaderRecompiler::RdnaOpcode;
    switch (op) {
        case Op::SLoadDword:
        case Op::SLoadDwordx2:
        case Op::SLoadDwordx4:
        case Op::SLoadDwordx8:
        case Op::SLoadDwordx16:
        case Op::BufferLoadFormatX:
        case Op::BufferLoadFormatXy:
        case Op::BufferLoadFormatXyz:
        case Op::BufferLoadFormatXyzw:
        case Op::BufferLoadDword:
        case Op::BufferLoadDwordx2:
        case Op::BufferLoadDwordx4:
        case Op::ImageSample:
        case Op::ImageSampleLz:
        case Op::ImageSampleBCl:
        case Op::ImageSampleBClO:
        case Op::ImageSampleBO:
        case Op::ImageSampleC:
        case Op::ImageSampleCB:
        case Op::ImageSampleCBCl:
        case Op::ImageSampleCBClO:
        case Op::ImageSampleCBO:
        case Op::ImageSampleCCl:
        case Op::ImageSampleCClO:
        case Op::ImageSampleCD:
        case Op::ImageSampleCDCl:
        case Op::ImageSampleCDClO:
        case Op::ImageSampleCDO:
        case Op::ImageSampleCL:
        case Op::ImageSampleCLzO:
        case Op::ImageSampleCLO:
        case Op::ImageSampleCO:
        case Op::ImageSampleCl:
        case Op::ImageSampleClO:
        case Op::ImageSampleD:
        case Op::ImageSampleDCl:
        case Op::ImageSampleDO:
        case Op::ImageSampleLzO:
        case Op::ImageSampleO:
        case Op::ImageSampleCd:
        case Op::ImageSampleCdCl:
        case Op::ImageSampleCCd:
        case Op::ImageSampleCCdCl:
        case Op::ImageSampleCdO:
        case Op::ImageSampleCdClO:
        case Op::ImageSampleCCdO:
        case Op::ImageSampleCCdClO:
        case Op::ImageSampleDG16:
        case Op::ImageSampleDClG16:
        case Op::ImageSampleCDG16:
        case Op::ImageSampleCDClG16:
        case Op::ImageSampleDOG16:
        case Op::ImageSampleDClOG16:
        case Op::ImageSampleCDOG16:
        case Op::ImageSampleCDClOG16:
        case Op::ImageSampleCdG16:
        case Op::ImageSampleCdClG16:
        case Op::ImageSampleCCdG16:
        case Op::ImageSampleCCdClG16:
        case Op::ImageSampleCdOG16:
        case Op::ImageSampleCdClOG16:
        case Op::ImageSampleCCdOG16:
        case Op::ImageSampleCCdClOG16:
        case Op::ImageLoad:
        case Op::ImageGetResinfo:
        case Op::SBufferLoadDword:
        case Op::SBufferLoadDwordx2:
        case Op::SBufferLoadDwordx4:
        case Op::SBufferLoadDwordx8:
        case Op::SBufferLoadDwordx16:
        case Op::SScratchLoadDword:
        case Op::SScratchLoadDwordx2:
        case Op::SScratchLoadDwordx4:
        case Op::BufferLoadUbyte:
        case Op::BufferLoadSbyte:
        case Op::BufferLoadUshort:
        case Op::BufferLoadSshort:
        case Op::BufferLoadDwordx3:
        case Op::BufferLoadUbyteD16:
        case Op::BufferLoadUbyteD16Hi:
        case Op::BufferLoadSbyteD16:
        case Op::BufferLoadSbyteD16Hi:
        case Op::BufferLoadShortD16:
        case Op::BufferLoadShortD16Hi:
        case Op::TbufferLoadFormatX:
        case Op::TbufferLoadFormatXy:
        case Op::TbufferLoadFormatXyz:
        case Op::TbufferLoadFormatXyzw:
        case Op::TbufferLoadFormatD16X:
        case Op::TbufferLoadFormatD16Xy:
        case Op::TbufferLoadFormatD16Xyz:
        case Op::TbufferLoadFormatD16Xyzw:
        case Op::BufferLoadFormatD16X:
        case Op::BufferLoadFormatD16Xy:
        case Op::BufferLoadFormatD16Xyz:
        case Op::BufferLoadFormatD16Xyzw:
        case Op::BufferLoadFormatD16HiX:
        case Op::FlatLoadUbyte:
        case Op::FlatLoadSbyte:
        case Op::FlatLoadUshort:
        case Op::FlatLoadSshort:
        case Op::FlatLoadDword:
        case Op::FlatLoadDwordx2:
        case Op::FlatLoadDwordx3:
        case Op::FlatLoadDwordx4:
        case Op::GlobalLoadDwordAddtid:
        case Op::FlatLoadUbyteD16:
        case Op::FlatLoadUbyteD16Hi:
        case Op::FlatLoadSbyteD16:
        case Op::FlatLoadSbyteD16Hi:
        case Op::FlatLoadShortD16:
        case Op::FlatLoadShortD16Hi:
        case Op::ImageGetLod:
        case Op::ImageLoadMip:
        case Op::ImageLoadPck:
        case Op::ImageLoadPckSgn:
        case Op::ImageLoadMipPck:
        case Op::ImageLoadMipPckSgn:
        case Op::ImageLoadBy2:
        case Op::ImageLoadBy4:
        case Op::ImageLoadMipBy2:
        case Op::ImageLoadMipBy4:
        case Op::ImageLoadPck2:
        case Op::ImageLoadPck4:
        case Op::ImageLoadMipPck2:
        case Op::ImageLoadMipPck4:
        case Op::ImageMsaaLoad:
        case Op::ImageGather4Lz:
        case Op::ImageGather4C:
        case Op::ImageGather4CLz:
        case Op::ImageGather4LzO:
        case Op::ImageGather4CO:
        case Op::ImageGather4CLzO:
        case Op::ImageGather4h:
        case Op::ImageGather4hPck:
        case Op::ImageGather4:
        case Op::ImageGather4B:
        case Op::ImageGather4BCl:
        case Op::ImageGather4BClO:
        case Op::ImageGather4BO:
        case Op::ImageGather4Cl:
        case Op::ImageGather4ClO:
        case Op::ImageGather4CB:
        case Op::ImageGather4CBCl:
        case Op::ImageGather4CBClO:
        case Op::ImageGather4CBO:
        case Op::ImageGather4CCl:
        case Op::ImageGather4CClO:
        case Op::ImageGather4CL:
        case Op::ImageGather4CLO:
        case Op::ImageGather4L:
        case Op::ImageGather4LO:
        case Op::ImageGather4O:
        case Op::ImageBvhIntersectRay:
        case Op::SGl1Inv:
        case Op::SDcacheInv:
        case Op::SDcacheWb:
        case Op::SMemtime:
        case Op::SMemrealtime:
        case Op::SDcacheDiscard:
        case Op::SDcacheDiscardX2:
        case Op::BufferGl0Inv:
        case Op::BufferGl1Inv:
        return true;
    default:
        return false;
    }
}

bool decodedWritesMemory(std::span<const std::uint32_t> code) {
    using Family = ShaderRecompiler::RdnaInstructionFamily;
    using Op = ShaderRecompiler::RdnaOpcode;
    try {
        const auto decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(code);
        return std::any_of(decoded.instructions.begin(), decoded.instructions.end(), [](const auto& instruction) {
            switch (instruction.family) {
            case Family::MUBUF: case Family::MTBUF: case Family::FLAT: case Family::MIMG: case Family::SMEM:
                return !readOnlyMemoryOp(instruction.op);
            case Family::DS:
                return instruction.gds || instruction.op == Op::DsGwsInit || instruction.op == Op::DsGwsBarrier || instruction.op == Op::DsGwsSemaV || instruction.op == Op::DsGwsSemaP || instruction.op == Op::DsGwsSemaBr || instruction.op == Op::DsGwsSemaReleaseAll || instruction.op == Op::DsOrderedCount || instruction.op == Op::DsAppend || instruction.op == Op::DsConsume;
            case Family::Unknown:
                return true;
            default:
                return instruction.op == Op::SSetpcB64 || instruction.op == Op::SSwappcB64 || instruction.op == Op::SRfeB64;
            }
        });
    } catch (const std::exception&) {
        return true;
    }
}

}

bool ProgramWritesMemory(const ShaderSnapshot& snapshot, std::size_t codeOffset) {
    constexpr std::uint8_t Unknown = 0, Reads = 1, Writes = 2;
    if (codeOffset == 0) {
        const auto cached = snapshot.memoryWrites->load(std::memory_order_acquire);
        if (cached != Unknown) return cached == Writes;
    }
    if (codeOffset > snapshot.code.size()) return true;
    const bool writes = decodedWritesMemory(std::span(snapshot.code).subspan(codeOffset));
    if (codeOffset == 0) snapshot.memoryWrites->store(writes ? Writes : Reads, std::memory_order_release);
    return writes;
}

std::uint32_t PositionExportLayout(const ShaderSnapshot* snapshot, std::size_t codeOffset, std::span<const std::uint32_t> code, std::uint32_t paClVsOutCntl, const ShaderSnapshot* continuation) {
    constexpr std::uint8_t Unknown = 0, Unused = 1, Used = 2;
    const auto bits = paClVsOutCntl & Graphics::PositionExportLayoutBits;
    if (bits == 0) return 0;
    if (continuation != nullptr && PositionExportLayout(continuation, 0, continuation->code, bits) != 0) return bits;
    const bool cached = snapshot != nullptr && codeOffset == 0;
    if (cached) {
        const auto use = snapshot->positionVectorUse->load(std::memory_order_acquire);
        if (use != Unknown) return use == Used ? bits : 0;
    }
    bool used = true;
    try {
        const auto decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(code);
        used = std::any_of(decoded.instructions.begin(), decoded.instructions.end(), [](const auto& instruction) {
            return instruction.family == ShaderRecompiler::RdnaInstructionFamily::EXP && instruction.exportTarget >= 0x0du && instruction.exportTarget <= 0x0fu && instruction.exportEnableMask != 0;
        });
    } catch (const std::invalid_argument& error) {
        if (std::strstr(error.what(), "opcode is not implemented") == nullptr) throw;
    }
    if (cached) snapshot->positionVectorUse->store(used ? Used : Unused, std::memory_order_release);
    return used ? bits : 0;
}


bool VertexWorkgroupRequired(const QueueState& queue, const ShaderRegistry& registry) {
    Graphics::NoteRegisterRead(Graphics::RegisterBank::Context, 0x2d5);
    const auto routing = queue.context.find(0x2d5);
    if (routing == queue.context.end() || (routing->second & 0x02002024u) != 0x2000u) return false;
    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, 0xc8);
    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, 0xc9);
    const auto low = queue.shader.find(0xc8);
    const auto high = queue.shader.find(0xc9);
    if (low == queue.shader.end() || high == queue.shader.end()) return false;
    require((high->second & ~0xffu) == 0, "reserved graphics program address bits are set");
    const auto address = (static_cast<std::uint64_t>(low->second) << 8u) | (static_cast<std::uint64_t>(high->second) << 40u);
    const auto snapshot = ProgramSnapshot(registry, address, {2});
    require((address - snapshot->codeAddress) % sizeof(std::uint32_t) == 0, "graphics entry point is not dword aligned");
    const auto offset = (address - snapshot->codeAddress) / sizeof(std::uint32_t);
    if (!snapshot->header.empty()) return snapshot->type == 2 && ProgramUsesWorkgroup(*snapshot, offset);
    if (!snapshot->userDataTailRegister) return ProgramUsesWorkgroup(*snapshot, offset);
    Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, 0x8b);
    const auto resources = queue.shader.find(0x8b);
    const auto rsrc2 = resources == queue.shader.end() ? 0u : resources->second;
    const auto count = ((rsrc2 >> 1u) & 0x1fu) | (((rsrc2 >> 27u) & 1u) << 5u);
    require(count <= 32, "graphics user SGPR count exceeds the register bank");
    std::vector<std::uint32_t> userData;
    for (std::uint32_t index = 0; index < count; ++index) {
        Graphics::NoteRegisterRead(Graphics::RegisterBank::Shader, 0x8c + index);
        const auto found = queue.shader.find(0x8c + index);
        userData.push_back(found == queue.shader.end() ? 0u : found->second);
    }
    const auto continuation = ReadUserDataTailContinuation(std::span(snapshot->code).subspan(offset), 8u, userData);
    return ProgramUsesWorkgroup(*snapshot, offset) || (continuation != nullptr && ProgramUsesWorkgroup(*continuation, 0u));
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
    std::unique_lock<std::mutex> lock{writers, std::defer_lock};
    std::map<PreparedShaders*, Change> changes;
    bool failed = false;

    void Hold() {
        if (!lock.owns_lock()) lock.lock();
    }
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
    root->Hold();
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
    root->Hold();
    auto& changes = root->changes;
    const auto found = changes.find(snapshot.prepared.get());
    if (found != changes.end()) return found->second.prepared;
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    return changes.emplace(snapshot.prepared.get(), State::Change{snapshot.prepared, *snapshot.prepared, false, snapshot.prepared->entries.size(), snapshot.prepared->registeredAbis.size()}).first->second.prepared;
}

bool ShaderPreparationTransaction::Tracks(const ShaderSnapshot& snapshot) const {
    return root->changes.contains(snapshot.prepared.get());
}

void ShaderPreparationTransaction::Hold() {
    require(!committed, "shader preparation transaction is already committed");
    root->Hold();
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
            destination.deferred = destination.deferred || change.prepared.deferred;
        }
    }
    committed = true;
}

bool FusedHullHeader(const ShaderSnapshot& snapshot, Shader used, const Shader& registered) {
    if (used.type != 3u || registered.type != 7u) return false;
    const auto* bound = used.sh_registers;
    used.type = registered.type;
    used.sh_registers = registered.sh_registers;
    if (std::memcmp(&used, &registered, offsetof(Shader, num_sh_registers) + sizeof(Shader::num_sh_registers)) != 0) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(registered.sh_registers);
    const std::size_t bytes = registered.num_sh_registers * sizeof(ShaderRegister);
    if (bytes == 0 || bound == nullptr || address < snapshot.headerAddress || address - snapshot.headerAddress > snapshot.header.size() || snapshot.header.size() - (address - snapshot.headerAddress) < bytes) return false;
    GuestMemory::CheckRange(bound, bytes, alignof(ShaderRegister));
    for (std::size_t index = 0; index < registered.num_sh_registers; ++index) {
        ShaderRegister expected;
        std::memcpy(&expected, snapshot.header.data() + (address - snapshot.headerAddress) + index * sizeof(ShaderRegister), sizeof(ShaderRegister));
        const auto reg = bound[index];
        if (reg.offset != expected.offset) return false;
        const bool fused = reg.offset == 0x100u || reg.offset == 0x10au || reg.offset == 0x10bu || reg.offset == 0x148u || reg.offset == 0x149u;
        if (!fused && reg.value != expected.value) return false;
    }
    return true;
}

static Shader GuestShaderFields(const Shader* shader) {
    GuestMemory::CheckRange(shader, sizeof(Shader), 1);
    Shader fields;
    std::memcpy(&fields, static_cast<const void*>(shader), sizeof(Shader));
    return fields;
}

static bool RelocatedRegisters(const ShaderSnapshot& snapshot, Shader used, const Shader& registered) {
    const auto* bound = used.sh_registers;
    used.sh_registers = registered.sh_registers;
    if (std::memcmp(&used, &registered, offsetof(Shader, num_sh_registers) + sizeof(Shader::num_sh_registers)) != 0) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(registered.sh_registers);
    const std::size_t bytes = registered.num_sh_registers * sizeof(ShaderRegister);
    if (bytes == 0 || bound == nullptr || address < snapshot.headerAddress || address - snapshot.headerAddress > snapshot.header.size() || snapshot.header.size() - (address - snapshot.headerAddress) < bytes) return false;
    GuestMemory::CheckRange(bound, bytes, alignof(ShaderRegister));
    return std::memcmp(static_cast<const void*>(bound), snapshot.header.data() + (address - snapshot.headerAddress), bytes) == 0;
}

// A title can relocate a registered header, and the arrays it points at, into memory of its own and resolve the ABI
// with that copy. Every array the copy refers to must still hold the registered bytes: the registered header then
// decodes to the values the copy describes.
static bool ReferredBytesMatch(const ShaderSnapshot& snapshot, const void* pointer, const void* registeredPointer, std::size_t bytes) {
    if (bytes == 0 || pointer == registeredPointer) return true;
    if (pointer == nullptr || registeredPointer == nullptr) return false;
    const auto address = reinterpret_cast<std::uintptr_t>(registeredPointer);
    if (address < snapshot.headerAddress) return false;
    const auto offset = address - snapshot.headerAddress;
    if (offset > snapshot.header.size() || snapshot.header.size() - offset < bytes) return false;
    if (!GuestMemory::Accessible(pointer, bytes)) return false;
    return std::memcmp(static_cast<const void*>(pointer), snapshot.header.data() + offset, bytes) == 0;
}

static bool RelocatedCopy(const ShaderSnapshot& snapshot, Shader used, const Shader& registered) {
    Shader masked = used;
    masked.user_data = registered.user_data;
    masked.cx_registers = registered.cx_registers;
    masked.sh_registers = registered.sh_registers;
    masked.specials = registered.specials;
    masked.input_semantics = registered.input_semantics;
    masked.output_semantics = registered.output_semantics;
    if (std::memcmp(&masked, &registered, offsetof(Shader, num_sh_registers) + sizeof(Shader::num_sh_registers)) != 0) return false;
    return ReferredBytesMatch(snapshot, used.sh_registers, registered.sh_registers, used.num_sh_registers * sizeof(ShaderRegister))
        && ReferredBytesMatch(snapshot, used.cx_registers, registered.cx_registers, used.num_cx_registers * sizeof(ShaderRegister))
        && ReferredBytesMatch(snapshot, used.specials, registered.specials, sizeof(ShaderSpecialRegs))
        && ReferredBytesMatch(snapshot, used.input_semantics, registered.input_semantics, used.num_input_semantics * sizeof(ShaderSemantic))
        && ReferredBytesMatch(snapshot, used.output_semantics, registered.output_semantics, used.num_output_semantics * sizeof(ShaderSemantic));
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
    return std::memcmp(&used, &registered, offsetof(Shader, num_sh_registers) + sizeof(Shader::num_sh_registers)) == 0 || RelocatedRegisters(snapshot, used, registered) || RelocatedCopy(snapshot, used, registered) || FusedHullHeader(snapshot, used, registered);
}

std::size_t ShaderRegistry::chunkFor(std::uint64_t address) const {
    const auto above = std::upper_bound(chunks.begin(), chunks.end(), address, [](std::uint64_t key, const std::shared_ptr<Chunk>& chunk) { return key < chunk->begin()->first; });
    return above == chunks.begin() ? 0u : static_cast<std::size_t>(above - chunks.begin()) - 1u;
}

ShaderRegistry::Chunk& ShaderRegistry::writable(std::size_t index) {
    auto& chunk = chunks[index];
    if (chunk.use_count() != 1) chunk = std::make_shared<Chunk>(*chunk);
    return *chunk;
}

ShaderRegistry::const_iterator ShaderRegistry::find(std::uint64_t address) const {
    if (chunks.empty()) return end();
    const auto index = chunkFor(address);
    const auto found = chunks[index]->find(address);
    if (found == chunks[index]->end()) return end();
    return const_iterator(chunks.cbegin() + static_cast<std::ptrdiff_t>(index), chunks.cend(), found);
}

ShaderRegistry::const_iterator ShaderRegistry::upper_bound(std::uint64_t address) const {
    if (chunks.empty()) return end();
    const auto index = chunkFor(address);
    const auto found = chunks[index]->upper_bound(address);
    if (found != chunks[index]->end()) return const_iterator(chunks.cbegin() + static_cast<std::ptrdiff_t>(index), chunks.cend(), found);
    if (index + 1u == chunks.size()) return end();
    return const_iterator(chunks.cbegin() + static_cast<std::ptrdiff_t>(index + 1u), chunks.cend(), chunks[index + 1u]->cbegin());
}

const ShaderRegistry::mapped_type& ShaderRegistry::at(std::uint64_t address) const {
    const auto found = find(address);
    if (found == end()) throw std::out_of_range("shader registry has no entry for the code address");
    return found->second;
}

ShaderRegistry::mapped_type& ShaderRegistry::operator[](std::uint64_t address) {
    if (chunks.empty()) chunks.push_back(std::make_shared<Chunk>());
    const auto index = chunks.front()->empty() ? 0u : chunkFor(address);
    auto& chunk = writable(index);
    const auto [entry, inserted] = chunk.try_emplace(address);
    auto& headers = entry->second;
    if (!inserted) return headers;
    ++count;
    if (chunk.size() > ChunkEntries) {
        auto upper = std::make_shared<Chunk>();
        for (auto moved = std::next(chunk.begin(), static_cast<std::ptrdiff_t>(chunk.size() / 2u)); moved != chunk.end();) upper->insert(upper->end(), chunk.extract(moved++));
        chunks.insert(chunks.begin() + static_cast<std::ptrdiff_t>(index + 1u), std::move(upper));
    }
    return headers;
}

std::pair<ShaderRegistry::const_iterator, bool> ShaderRegistry::emplace(std::uint64_t address, mapped_type headers) {
    if (const auto found = find(address); found != end()) return {found, false};
    (*this)[address] = std::move(headers);
    return {find(address), true};
}

std::size_t ShaderRegistry::erase(std::uint64_t address) {
    if (!contains(address)) return 0;
    const auto index = chunkFor(address);
    auto& chunk = writable(index);
    chunk.erase(address);
    --count;
    if (chunk.empty()) chunks.erase(chunks.begin() + static_cast<std::ptrdiff_t>(index));
    return 1;
}

std::shared_ptr<const ShaderSnapshot> RegisteredHeader(const ShaderRegistry& registry, const Shader* shader) {
    const auto found = registry.find(reinterpret_cast<std::uintptr_t>(const_cast<const void*>(GuestShaderFields(shader).code)));
    if (found == registry.end()) return {};
    for (const auto& snapshot : found->second) {
        if (snapshot->headerAddress == reinterpret_cast<std::uintptr_t>(shader)) return snapshot;
    }
    for (const auto& snapshot : found->second) {
        if (SameHeader(*snapshot, shader)) return snapshot;
    }
    return {};
}

std::shared_ptr<const ShaderSnapshot> RegisteredProgram(const ShaderRegistry& registry, std::uint64_t address, std::initializer_list<std::uint8_t> types) {
    auto found = registry.upper_bound(address);
    if (found == registry.begin()) return {};
    --found;
    std::shared_ptr<const ShaderSnapshot> result;
    bool registered = false;
    for (const auto& snapshot : found->second) {
        if (address - snapshot->codeAddress >= snapshot->code.size() * sizeof(std::uint32_t)) continue;
        registered = true;
        if (std::ranges::find(types, snapshot->type) == types.end()) continue;
        require(result == nullptr || result->type == snapshot->type, "registered program has ambiguous shader binary types");
        result = snapshot;
    }
    require(!registered || result != nullptr, "registered program refers to an incompatible shader binary type");
    return result;
}

void PublishRegisteredShader(std::shared_ptr<ShaderRegistry>& registry, const std::shared_ptr<const ShaderSnapshot>& snapshot) {
    ShaderPreparationTransaction transaction;
    transaction.Hold();
    auto published = snapshot;
    if (registry != nullptr) {
        const auto found = registry->find(snapshot->codeAddress);
        if (found != registry->end()) {
            for (const auto& current : found->second) {
                if (current->headerAddress == snapshot->headerAddress && current->type == snapshot->type && current->code == snapshot->code && current->header == snapshot->header) {
                    if (current == found->second.back()) {
                        transaction.Commit();
                        return;
                    }
                    published = current;
                    break;
                }
            }
        }
    }
    auto next = registry != nullptr ? std::make_shared<ShaderRegistry>(*registry) : std::make_shared<ShaderRegistry>();
    auto& headers = (*next)[snapshot->codeAddress];
    std::erase_if(headers, [&](const auto& current) { return current->headerAddress == snapshot->headerAddress || current->code != snapshot->code; });
    headers.push_back(published);
    registry = std::move(next);
    transaction.Commit();
}

namespace {

bool LazyShaderPreparation() {
    static const bool lazy = std::getenv("APS5_LAZY_SHADER_PREPARE") != nullptr;
    return lazy;
}

using PreparedAtUse = std::optional<std::vector<const ShaderRecompiler::SourceHandle*>>;

PreparedAtUse PrepareDeferredAbis(const ShaderSnapshot& snapshot, std::unique_lock<std::mutex>& lock);

struct DeferredAbiUseCounters {
    std::atomic<std::uint64_t> firstUses{0};
    std::atomic<std::uint64_t> found{0};
    std::atomic<std::uint64_t> own{0};
    std::atomic<std::uint64_t> entries{0};
    std::atomic<std::uint64_t> matchedEntries{0};
    std::atomic<std::int64_t> lastReport{0};
};

DeferredAbiUseCounters& DeferredAbiCounters() {
    static DeferredAbiUseCounters counters;
    return counters;
}

void ReportDeferredAbiUses() {
    static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    if (!profile) return;
    auto& counters = DeferredAbiCounters();
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous = counters.lastReport.load(std::memory_order_relaxed);
    if ((previous != 0 && now - previous < 60000) || !counters.lastReport.compare_exchange_strong(previous, now, std::memory_order_relaxed)) return;
    const auto entries = counters.entries.load(std::memory_order_relaxed);
    const auto matched = counters.matchedEntries.load(std::memory_order_relaxed);
    AgcDriver::ProfilePrint_nid_no_patch("[shaders] deferred ABIs: %llu prepared at first use, %llu found their artifact there, %llu prepared their own; %llu of the %llu entries prepared from them never matched a use\n", static_cast<unsigned long long>(counters.firstUses.load(std::memory_order_relaxed)), static_cast<unsigned long long>(counters.found.load(std::memory_order_relaxed)), static_cast<unsigned long long>(counters.own.load(std::memory_order_relaxed)), static_cast<unsigned long long>(entries - std::min(entries, matched)), static_cast<unsigned long long>(entries));
}

void NoteMatchedEntry(PreparedShaders::Entry& entry, const PreparedAtUse& preparedAtUse) {
    auto& counters = DeferredAbiCounters();
    bool changed = false;
    if (entry.fromDeferredAbi && !entry.matched) {
        entry.matched = true;
        counters.matchedEntries.fetch_add(1, std::memory_order_relaxed);
        changed = true;
    }
    if (preparedAtUse) {
        counters.firstUses.fetch_add(1, std::memory_order_relaxed);
        if (std::ranges::find(*preparedAtUse, entry.handle.get()) != preparedAtUse->end()) counters.found.fetch_add(1, std::memory_order_relaxed);
        changed = true;
    }
    if (changed) ReportDeferredAbiUses();
}

void NoteOwnPreparation(const PreparedAtUse& preparedAtUse) {
    if (!preparedAtUse) return;
    auto& counters = DeferredAbiCounters();
    counters.firstUses.fetch_add(1, std::memory_order_relaxed);
    counters.own.fetch_add(1, std::memory_order_relaxed);
    ReportDeferredAbiUses();
}

void ReportPreparedAtUse(const ShaderSnapshot& snapshot, const ShaderRecompiler::RecompileRequest& request) {
    if (request.context.convertedSamples) {
        APS5_LOG_OUT_DEBUG("Shader 0x%llx samples a converted image; preparing its converted-sample variant at use", static_cast<unsigned long long>(request.shader.codeAddress));
        return;
    }
    if (snapshot.codeAddress == NullPixelProgramAddress() && request.shader.stage == ShaderRecompiler::ShaderStage::Fragment && request.context.waveSize == 32u) {
        APS5_LOG_ERR("The null pixel program has no wave%u artifact for this draw; preparing it at draw", request.context.waveSize);
        return;
    }
    if (request.context.vertex && (request.context.vertex->paClVsOutCntl & (1u << 18u)) != 0) {
        APS5_LOG_OUT_DEBUG("Shader 0x%llx is drawn into a layered target; preparing its layered variant at draw", static_cast<unsigned long long>(snapshot.codeAddress));
        return;
    }
    if (!snapshot.header.empty()) {
        if (!snapshot.prepared->deferred) APS5_LOG_ERR("Shader 0x%llx stage %u has no artifact prepared at registration for the state of this draw or dispatch; preparing it at use", static_cast<unsigned long long>(request.shader.codeAddress), static_cast<std::uint32_t>(request.shader.stage));
        return;
    }
    if (request.shader.stage == ShaderRecompiler::ShaderStage::Compute) {
        if (snapshot.type != 0) throw std::runtime_error("AGC driver: unregistered program is not a compute shader");
        APS5_LOG_ERR("Compute shader 0x%llx was not registered; preparing its artifact at dispatch", static_cast<unsigned long long>(snapshot.codeAddress));
        return;
    }
    APS5_LOG_ERR("Graphics shader 0x%llx was not registered; preparing its artifact at first use", static_cast<unsigned long long>(snapshot.codeAddress));
}

}

std::shared_ptr<const ShaderRecompiler::SourceHandle> SourceHandleFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request) {
    require(codeOffset < snapshot.code.size(), "prepared shader code offset is outside the snapshot");
    require(std::ranges::equal(request.shader.code, std::span(snapshot.code).subspan(codeOffset)), "prepared shader request does not refer to registered code");
    struct PreparedKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, PreparedKeyStorage>();
    ShaderRecompiler::BuildPreparedShaderKey(request, key);
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    PreparedAtUse preparedAtUse;
    if (!snapshot.prepared->deferredAbis.empty() || snapshot.prepared->deferredPreparing) preparedAtUse = PrepareDeferredAbis(snapshot, lock);
    for (auto& entry : snapshot.prepared->entries) {
        if (entry.codeOffset == codeOffset && ShaderRecompiler::MatchesPreparedShader(request, *entry.handle, key)) {
            NoteMatchedEntry(entry, preparedAtUse);
            return entry.handle;
        }
    }
    NoteOwnPreparation(preparedAtUse);
    ReportPreparedAtUse(snapshot, request);
    auto handle = PrepareShaderWithDiagnostics(request);
    snapshot.prepared->entries.push_back({codeOffset, handle});
    return handle;
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

namespace {

std::vector<std::uint32_t> LinkGraphicsUserDataTail(const ShaderRecompiler::RecompileRequest& request, const ShaderSnapshot& snapshot) {
    using Stage = ShaderRecompiler::ShaderStage;
    if ((request.shader.stage != Stage::Vertex && request.shader.stage != Stage::Fragment) || !request.shader.header.empty() || !snapshot.userDataTailRegister) return {};
    const auto program = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(request.shader.code);
    const auto tail = ShaderRecompiler::FindUserDataTail(program, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size()));
    if (!tail) return {};
    const auto continuation = ReadUserDataTailContinuation(request.shader.code, request.context.userDataBaseRegister, request.context.userData);
    return ShaderRecompiler::LinkUserDataTail(program, *tail, ShaderRecompiler::RdnaInstructionDecoder{}.Decode(continuation->code));
}

std::string hexText(std::uint64_t value) {
    char text[24];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(value));
    return text;
}

std::vector<std::uint32_t> ReadUserDataCallee(std::uint64_t address, std::uint32_t linkRegister) {
    constexpr std::uint64_t page = 4096u;
    constexpr std::size_t limitWords = 16384u;
    if (address % 4u != 0u) throw std::runtime_error("its callee address " + hexText(address) + " is not dword aligned");
    std::vector<std::uint32_t> words;
    while (words.size() < limitWords) {
        const auto next = address + words.size() * 4u;
        const auto bytes = static_cast<std::size_t>(page - next % page);
        if (!GuestMemory::Accessible(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(next)), bytes)) throw std::runtime_error("its callee at " + hexText(address) + " is not readable at " + hexText(next));
        const auto old = words.size();
        words.resize(old + bytes / 4u);
        GuestMemory::Read(next, std::as_writable_bytes(std::span(words).subspan(old)));
        if (const auto length = ShaderRecompiler::UserDataCalleeLength(words, linkRegister)) {
            words.resize(*length);
            return words;
        }
    }
    throw std::runtime_error("its callee at " + hexText(address) + " does not return within " + std::to_string(limitWords * 4u) + " bytes");
}

std::vector<std::uint8_t> ReadGuestBytes(std::uint64_t address, std::uint64_t bytes, const char* what) {
    if (address > UINTPTR_MAX - bytes || !GuestMemory::Accessible(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(address)), static_cast<std::size_t>(bytes))) {
        throw std::runtime_error(std::string("its ") + what + " at " + hexText(address) + " + " + hexText(bytes) + " is not readable");
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(bytes));
    GuestMemory::Read(address, std::as_writable_bytes(std::span(data)));
    return data;
}

std::vector<ShaderRecompiler::TableCallee> ReadTableCallees(const ShaderRecompiler::RecompileRequest& request, const ShaderRecompiler::TableCall& call) {
    constexpr std::uint64_t limitBytes = 64u << 20u;
    constexpr std::size_t limitCallees = 64u;
    const auto slot = call.userDataRegister - request.context.userDataBaseRegister;
    const auto pointer = call.constantPointer ? *call.constantPointer : static_cast<std::uint64_t>(request.context.userData[slot]) | static_cast<std::uint64_t>(request.context.userData[slot + 1u]) << 32u;
    const auto words = ReadGuestBytes(pointer + call.descriptorOffset, 16u, "call table descriptor");
    ShaderRecompiler::ShaderBufferResource descriptor{};
    std::memcpy(descriptor.fields.data(), words.data(), words.size());
    const auto bytes = descriptor.GetSize();
    if (bytes > limitBytes) throw std::runtime_error("its call table of " + std::to_string(bytes) + " bytes is larger than the " + std::to_string(limitBytes) + " bytes that are searched for callees");
    const auto table = bytes == 0u ? std::vector<std::uint8_t>{} : ReadGuestBytes(descriptor.Base48(), bytes, "call table");
    std::set<std::uint64_t> targets;
    for (std::uint64_t offset = call.entryOffset; offset + 8u <= bytes; offset += call.entryStride) {
        std::uint64_t target = 0;
        std::memcpy(&target, table.data() + offset, sizeof(target));
        if (target != 0u) targets.insert(target);
        if (call.entryStride == 0u) break;
    }
    if (targets.size() > limitCallees) throw std::runtime_error("its call table holds " + std::to_string(targets.size()) + " distinct targets; at most " + std::to_string(limitCallees) + " are linked");
    std::vector<ShaderRecompiler::TableCallee> callees;
    for (const auto target : targets) callees.push_back({target, ReadUserDataCallee(target, call.linkRegister)});
    return callees;
}

ShaderRecompiler::LinkedCalls LinkUserDataCallees(const ShaderRecompiler::RecompileRequest& request) {
    if (request.shader.stage != ShaderRecompiler::ShaderStage::Compute) return {};
    const auto code = request.shader.code;
    if (std::none_of(code.begin(), code.end(), [](std::uint32_t word) { return (word & 0xff80ff00u) == 0xbe802100u; })) return {};
    const auto program = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(code);
    const ShaderRecompiler::SwappcInfo swappc{false, request.context.userDataBaseRegister, static_cast<std::uint32_t>(request.context.userData.size()), request.context.waveSize};
    const auto calls = ShaderRecompiler::FindUserDataCalls(program, swappc);
    const auto tableCalls = ShaderRecompiler::FindTableCalls(program, swappc);
    if (calls.empty() && tableCalls.empty()) return {};
    std::vector<std::vector<std::uint32_t>> callees;
    for (const auto& call : calls) {
        const auto slot = call.userDataRegister - swappc.userDataBaseRegister;
        const auto address = static_cast<std::uint64_t>(request.context.userData[slot]) | static_cast<std::uint64_t>(request.context.userData[slot + 1u]) << 32u;
        try {
            callees.push_back(ReadUserDataCallee(address, call.linkRegister));
        } catch (const std::exception& error) {
            throw std::runtime_error("AGC driver: compute shader " + hexText(request.shader.codeAddress) + " calls user data s[" + std::to_string(call.userDataRegister) + ":" +
                std::to_string(call.userDataRegister + 1u) + "] at program counter " + hexText(call.programCounter) + ", but " + error.what());
        }
    }
    std::vector<std::vector<ShaderRecompiler::TableCallee>> tableCallees;
    for (const auto& call : tableCalls) {
        try {
            tableCallees.push_back(ReadTableCallees(request, call));
        } catch (const std::exception& error) {
            throw std::runtime_error("AGC driver: compute shader " + hexText(request.shader.codeAddress) + " calls a target loaded from a table at program counter " + hexText(call.programCounter) + ", but " + error.what());
        }
    }
    return ShaderRecompiler::LinkCalls(program, calls, callees, tableCalls, tableCallees);
}

}

ShaderRecompiler::PreparedShaderInvocation InvocationFor(const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request) {
    require(codeOffset < snapshot.code.size(), "prepared shader code offset is outside the snapshot");
    const auto code = std::span(snapshot.code).subspan(codeOffset);
    require(request.shader.code.data() == code.data() && request.shader.code.size() == code.size(), "prepared invocation does not refer to registered code");
    auto linked = LinkUserDataCallees(request);
    if (linked.code.empty()) linked.code = LinkGraphicsUserDataTail(request, snapshot);
    auto source = request;
    if (!linked.code.empty()) {
        source.shader.code = linked.code;
        source.context.linkedCallFallbacks = linked.fallbacks;
    }
    std::unique_lock lock(snapshot.prepared->mutex);
    AwaitRegisteredPreparation(*snapshot.prepared, lock);
    if (std::ranges::find(snapshot.prepared->convertedSamples, codeOffset) != snapshot.prepared->convertedSamples.end()) source.context.convertedSamples = true;
    auto invocationRequest = source;
    struct PreparedKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, PreparedKeyStorage>();
    ShaderRecompiler::BuildPreparedShaderKey(source, key);
    PreparedAtUse preparedAtUse;
    if (!snapshot.prepared->deferredAbis.empty() || snapshot.prepared->deferredPreparing) preparedAtUse = PrepareDeferredAbis(snapshot, lock);
    for (auto& entry : snapshot.prepared->entries) {
        if (entry.codeOffset != codeOffset) continue;
        invocationRequest.shader.code = ShaderRecompiler::GetPreparedCode(*entry.handle);
        if (!linked.code.empty() && !std::ranges::equal(invocationRequest.shader.code, linked.code)) continue;
        if (auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(invocationRequest, entry.handle, key)) {
            NoteMatchedEntry(entry, preparedAtUse);
            return std::move(*invocation);
        }
    }
    NoteOwnPreparation(preparedAtUse);
    ReportPreparedAtUse(snapshot, source);
    auto handle = PrepareShaderWithDiagnostics(source);
    invocationRequest = source;
    invocationRequest.shader.code = ShaderRecompiler::GetPreparedCode(*handle);
    auto invocation = ShaderRecompiler::PreparedShaderInvocation::TryCreate(invocationRequest, handle, key);
    if (!invocation.has_value()) throw std::runtime_error("AGC driver: an artifact prepared at use does not match its invocation");
    snapshot.prepared->entries.push_back({codeOffset, std::move(handle)});
    return std::move(*invocation);
}

std::shared_ptr<const ShaderRecompiler::ResourceCapture> CaptureInvocation(ShaderMemory& memory, const ShaderSnapshot& snapshot, std::size_t codeOffset, const ShaderRecompiler::RecompileRequest& request, ShaderRecompiler::PreparedShaderInvocation& invocation) {
    try {
        return memory.Capture(invocation);
    } catch (const ShaderRecompiler::ConvertedSampleRequired&) {
        if (invocation.Request().context.convertedSamples) throw;
    }
    {
        std::lock_guard lock(snapshot.prepared->mutex);
        auto& offsets = snapshot.prepared->convertedSamples;
        if (std::ranges::find(offsets, codeOffset) == offsets.end()) offsets.push_back(codeOffset);
    }
    invocation = InvocationFor(snapshot, codeOffset, request);
    return memory.Capture(invocation);
}
namespace {

std::optional<ShaderRecompiler::ShaderFloatMode> DecodeFloatMode(const Registers& registers, std::uint8_t type) {
    std::uint32_t rsrc1;
    std::uint32_t fp16OverflowBit;
    switch (type) {
    case 0: rsrc1 = 0x212; fp16OverflowBit = 26; break;
    case 1: rsrc1 = 0x00a; fp16OverflowBit = 29; break;
    case 2: case 4: case 6: rsrc1 = 0x08a; fp16OverflowBit = 31; break;
    case 3: case 5: case 7: rsrc1 = 0x10a; fp16OverflowBit = 30; break;
    default: return std::nullopt;
    }
    const auto found = registers.find(rsrc1);
    if (found == registers.end()) return std::nullopt;
    const auto value = found->second;
    return ShaderRecompiler::ShaderFloatMode{(value >> 12u) & 0xffu, ((value >> 21u) & 1u) != 0u, ((value >> 23u) & 1u) != 0u, ((value >> fp16OverflowBit) & 1u) != 0u};
}

}

std::optional<ShaderRecompiler::ShaderFloatMode> RegisteredFloatMode(const ShaderSnapshot& snapshot) {
    if (snapshot.registeredState == nullptr) return std::nullopt;
    return DecodeFloatMode(snapshot.registeredState->shader, snapshot.type);
}

std::optional<ShaderRecompiler::ShaderFloatMode> DispatchFloatMode(const ShaderSnapshot& snapshot, const Registers& live) {
    if (snapshot.registeredState != nullptr) return RegisteredFloatMode(snapshot);
    return DecodeFloatMode(live, snapshot.type);
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

RegisteredShaderState DecodeRegisteredState(const ShaderSnapshot& snapshot, RegisteredShaderState state = {{}, InitialContextRegisters(), {{0x24a, 0}, {0x24b, 0}}}) {
    const auto header = ReadHeader(snapshot);
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
        registers->ForEach([&](std::uint32_t offset, std::uint32_t value) { key.push_back((static_cast<std::uint64_t>(offset) << 32u) | value); });
    }
}

struct RegisteredPreparation {
    std::size_t codeOffset = 0;
    bool registration = false;
    ShaderRecompiler::SwappcInfo swappc{};
    std::vector<std::uint32_t> userData;
    std::array<ShaderRecompiler::MemoryRegion, 2> memory{};
    std::optional<ShaderRecompiler::RecompileRequest> request;
    std::vector<std::uint64_t> abiKey;
    std::shared_ptr<VulkanDevice> prebuildDevice;
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
    case 3: case 5: return {};
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
    ShaderRecompiler::RdnaProgram decoded;
    plan->registration = registration;
    try {
        decoded = ShaderRecompiler::RdnaInstructionDecoder{}.Decode(code);
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
        const auto& capabilities = device.Target().supportedCapabilities;
        if (pixel->stencilRefExportEnable && std::ranges::find(capabilities, static_cast<std::uint32_t>(spv::CapabilityStencilExportEXT)) == capabilities.end()) return nullptr;
        wave = pixel->wave32 ? 32u : 64u;
    } else {
        const auto routing = RegisterValue(state.context, 0x2d5);
        wave = (routing & 0x00400000u) != 0 ? 32u : 64u;
        const bool unroutedBarrier = routing == 0 && std::ranges::any_of(decoded.instructions, [](const auto& instruction) {
            return instruction.op == ShaderRecompiler::RdnaOpcode::SBarrier;
        });
        if (registration && stage == Stage::Vertex && (((routing & 0x02002024u) == 0x2000u && ProgramUsesWorkgroup(snapshot, codeOffset)) || unroutedBarrier)) {
            if (unroutedBarrier && deferred != nullptr) return defer("its s_barrier depends on draw routing");
            return nullptr;
        }
        if ((routing & 0x20u) != 0 || stage == Stage::Mesh || (routing & 4u) != 0 || ((routing >> 3u) & 3u) == 1u) {
            if (registration) return {};
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
    if (stage != Stage::Compute && stage != Stage::Fragment && snapshot.type != 6) {
        vertex = Graphics::DecodeVertexStageInfo(snapshot.header, snapshot.headerAddress, plan->userData, nullptr, true, 8u - firstUser);
        const auto outputs = state.context.find(0x207u);
        if (vertex && outputs != state.context.end()) vertex->paClVsOutCntl = PositionExportLayout(&snapshot, codeOffset, std::span(snapshot.code).subspan(codeOffset), outputs->second);
    }
    plan->swappc = {vertex.has_value(), firstUser, userCount};
    if (deferred != nullptr) {
        if (const auto counter = ShaderRecompiler::UnresolvableSwappcTarget(decoded, &plan->swappc)) {
            char reason[96];
            std::snprintf(reason, sizeof(reason), "its s_swappc_b64 at program counter 0x%x calls a computed target", *counter);
            return defer(reason);
        }
    }
    plan->memory = {{{snapshot.codeAddress, std::as_bytes(std::span(snapshot.code))}, {snapshot.headerAddress, snapshot.header}}};
    auto& request = plan->request.emplace(ShaderRecompiler::RecompileRequest{{stage, address, code, snapshot.headerAddress, snapshot.header}, {wave, firstUser, plan->userData, compute, pixel, vertex, plan->memory, RegisteredFloatMode(snapshot)}, stage == Stage::Compute ? device.ComputeTarget(wave) : device.Target(), {0, 0, 0, 128}, graphics});
    if (graphics && graphics->mesh) request.layout.pushConstantSizeBytes = ShaderRecompiler::MeshDrawPushOffsetBytes;
    if (graphics && graphics->tessellation) request.layout.pushConstantSizeBytes = ShaderRecompiler::TessellationLevelPushOffsetBytes;
    if (pixel && device.GraphicsPipelineLibraries()) request.layout.pushConstantOffsetBytes = Graphics::FixedPushOffset(Stage::Fragment);
    return plan;
}

bool LibraryPrebuildDisabled() {
    static const bool disabled = [] {
        const auto* value = std::getenv("APS5_NO_LIBRARY_PREBUILD");
        return value != nullptr && std::strcmp(value, "1") == 0;
    }();
    return disabled;
}

void PrebuildStageLibrary(const std::shared_ptr<VulkanDevice>& device, const Graphics::ShaderStages& stages, ShaderRecompiler::ShaderStage stage, std::shared_ptr<const ShaderRecompiler::RecompileResult> result, std::uint32_t pushOffset) {
    if (LibraryPrebuildDisabled() || result == nullptr || (stage != ShaderRecompiler::ShaderStage::Vertex && stage != ShaderRecompiler::ShaderStage::Fragment)) return;
    SubmitLibraryPrebuild([device, stages, stage, result = std::move(result), pushOffset] {
        const std::array<Graphics::CompiledShader, 1> shaders{{{stage, result.get(), pushOffset}}};
        try {
            static_cast<void>(device->PrebuildGraphicsLibraries(stages, shaders));
        } catch (const std::exception& error) {
            APS5_LOG_OUT("shader library prebuild skipped: %s", error.what());
        }
    });
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
    if (plan.prebuildDevice != nullptr && request.target.registrationModules && request.shader.stage == ShaderRecompiler::ShaderStage::Fragment && !LibraryPrebuildDisabled()) {
        try {
            PrebuildStageLibrary(plan.prebuildDevice, {Graphics::ShaderPath::Vertex, 0u, 32u, request.context.waveSize, {}, {}}, request.shader.stage, ShaderRecompiler::RegistrationResult(request, *entries.front().handle), request.layout.pushConstantOffsetBytes);
        } catch (const std::exception& error) {
            APS5_LOG_OUT("shader library prebuild skipped: %s", error.what());
        }
    }
    if (request.context.compute) {
        request.context.compute->partialThreads = {1, 1, 1};
        append();
    } else if (request.context.pixel) {
        request.layout.pushConstantSizeBytes = 0;
        append();
    }
    if (plan.registration) {
        for (const auto& entry : entries) {
            if (entry.handle != nullptr) ShaderRecompiler::ReleaseUndrawnResourcePlan(*entry.handle);
        }
    }
    return entries;
}

std::vector<PreparedShaders::Entry> PrepareRegistered(const ShaderSnapshot& snapshot, const VulkanDevice& device, const QueueState& state, bool registration, bool* deferred = nullptr) {
    const auto plan = PlanRegistered(snapshot, device, state, registration, deferred);
    return plan != nullptr ? PrepareRegistered(*plan) : std::vector<PreparedShaders::Entry>{};
}

PreparedAtUse PrepareDeferredAbis(const ShaderSnapshot& snapshot, std::unique_lock<std::mutex>& lock) {
    auto& prepared = *snapshot.prepared;
    if (prepared.deferredPreparing) {
        prepared.settled.wait(lock, [&] { return !prepared.deferredPreparing; });
        return std::nullopt;
    }
    if (prepared.deferredAbis.empty()) return std::nullopt;
    std::vector<PreparedShaders::DeferredAbi> deferred;
    deferred.swap(prepared.deferredAbis);
    prepared.deferredPreparing = true;
    struct Preparing {
        PreparedShaders& prepared;
        std::unique_lock<std::mutex>& lock;
        ~Preparing() {
            if (!lock.owns_lock()) lock.lock();
            prepared.deferredPreparing = false;
            prepared.settled.notify_all();
        }
    } preparing{prepared, lock};
    lock.unlock();
    std::vector<PreparedShaders::Entry> entries;
    for (const auto& abi : deferred) {
        const auto device = abi.device.lock();
        if (device == nullptr || snapshot.registeredState == nullptr) continue;
        QueueState state{};
        state.shader = snapshot.registeredState->shader;
        state.context = snapshot.registeredState->context;
        state.userConfig = snapshot.registeredState->userConfig;
        for (const auto& [offset, value] : abi.context) state.context[offset] = value;
        for (const auto& [offset, value] : abi.primitive) state.userConfig[offset] = value;
        try {
            const auto plan = PlanRegistered(snapshot, *device, state, false);
            if (plan == nullptr) continue;
            plan->prebuildDevice = device;
            auto made = PrepareRegistered(*plan);
            std::ranges::move(made, std::back_inserter(entries));
        } catch (const std::exception& error) {
            APS5_LOG_ERR("Shader 0x%llx could not be prepared at its first use for its resolved ABI (%s); preparing it for the use instead", static_cast<unsigned long long>(snapshot.codeAddress), error.what());
            continue;
        }
    }
    lock.lock();
    std::vector<const ShaderRecompiler::SourceHandle*> added;
    for (auto& entry : entries) {
        const auto duplicate = std::ranges::any_of(prepared.entries, [&](const auto& existing) { return existing.codeOffset == entry.codeOffset && existing.handle->artifact == entry.handle->artifact; });
        if (duplicate) continue;
        entry.fromDeferredAbi = true;
        added.push_back(entry.handle.get());
        prepared.entries.push_back(std::move(entry));
    }
    DeferredAbiCounters().entries.fetch_add(added.size(), std::memory_order_relaxed);
    return added;
}

void DeferPipelineStage(PreparedShaderState& prepared, const ShaderSnapshot& snapshot, const char* reason) {
    if (!prepared.deferred) APS5_LOG_ERR("Shader 0x%llx could not be prepared when the title created its pipeline (%s); preparing it when it is used", static_cast<unsigned long long>(snapshot.codeAddress), reason);
    prepared.deferred = true;
}

std::optional<std::string> FailedRegistration(const ShaderSnapshot& snapshot) {
    std::unique_lock lock(snapshot.prepared->mutex);
    try {
        AwaitRegisteredPreparation(*snapshot.prepared, lock);
    } catch (const std::exception& error) {
        return error.what();
    }
    return std::nullopt;
}

bool DeferFailedRegistration(const ShaderSnapshot& snapshot) {
    const auto failure = FailedRegistration(snapshot);
    if (!failure) return false;
    std::lock_guard lock(snapshot.prepared->mutex);
    DeferPipelineStage(*snapshot.prepared, snapshot, failure->c_str());
    return true;
}

#ifdef __GLIBC__
void trimFreedMemory() {
    static std::atomic<std::int64_t> last{0};
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count();
    auto previous = last.load(std::memory_order_relaxed);
    if (now - previous < 1000 || !last.compare_exchange_strong(previous, now, std::memory_order_relaxed)) return;
    malloc_trim(0);
}
#endif

class RegistrationPreparePool {
public:
    static RegistrationPreparePool& Instance() {
        static auto* pool = [] {
            std::lock_guard lifecycle(Lifecycle());
            auto* created = new RegistrationPreparePool(Closing() ? 0u : std::max(2u, std::thread::hardware_concurrency() / 2u));
            PreparationSlot().store(created);
            return created;
        }();
        return *pool;
    }

    static RegistrationPreparePool* Existing() {
        return PreparationSlot().load();
    }

    static RegistrationPreparePool& Prebuild() {
        static auto* pool = [] {
            const char* text = std::getenv("APS5_PREBUILD_THREADS");
            std::lock_guard lifecycle(Lifecycle());
            auto* created = new RegistrationPreparePool(Closing() ? 0u : text != nullptr ? std::max(1u, static_cast<unsigned>(std::strtoul(text, nullptr, 10))) : 1u);
            PrebuildSlot().store(created);
            return created;
        }();
        return *pool;
    }

    static RegistrationPreparePool* ExistingPrebuild() {
        return PrebuildSlot().load();
    }

    static unsigned LiveWorkers() {
        return liveWorkers.load(std::memory_order_acquire);
    }

    static void CloseAll() {
        std::array<RegistrationPreparePool*, 2> pools{};
        {
            std::lock_guard lifecycle(Lifecycle());
            Closing() = true;
            pools = {Existing(), ExistingPrebuild()};
        }
        if (pools[0] != nullptr) pools[0]->Close(false);
        if (pools[1] != nullptr) pools[1]->Close(true);
    }

private:
    static std::mutex& Lifecycle() {
        static std::mutex lifecycle;
        return lifecycle;
    }

    static bool& Closing() {
        static bool closing = false;
        return closing;
    }

    static std::atomic<RegistrationPreparePool*>& PreparationSlot() {
        static std::atomic<RegistrationPreparePool*> slot{nullptr};
        return slot;
    }

    static std::atomic<RegistrationPreparePool*>& PrebuildSlot() {
        static std::atomic<RegistrationPreparePool*> slot{nullptr};
        return slot;
    }

public:

    void Submit(std::function<void()> task, const PreparedShaders* prepared = nullptr) {
        {
            std::unique_lock lock(mutex);
            if (threads != 0 && !closed) {
                const auto sequence = ++submitted;
                if (prepared != nullptr && prepared->demanded.load()) tasks.push_front({std::move(task), prepared, sequence});
                else tasks.push_back({std::move(task), prepared, sequence});
                unfinished.insert(sequence);
                lock.unlock();
                ready.notify_one();
                return;
            }
        }
        task();
    }

    void Prioritize(const PreparedShaders& prepared) {
        std::lock_guard lock(mutex);
        const auto found = std::ranges::find_if(tasks, [&](const auto& task) { return task.prepared == &prepared; });
        if (found == tasks.end()) return;
        if (std::getenv("APS5_TRACE_PREPARATION_WAIT") != nullptr) std::fprintf(stderr, "[shader-prepare] demanded queued task at position %zu of %zu\n", static_cast<std::size_t>(std::distance(tasks.begin(), found)), tasks.size());
        std::rotate(tasks.begin(), found, std::next(found));
    }

    void AwaitSubmitted() {
        std::unique_lock lock(mutex);
        const auto ticket = submitted;
        idle.wait(lock, [&] { return unfinished.empty() || *unfinished.begin() > ticket; });
    }

    void Close(bool dropQueued) {
        std::vector<Worker> joined;
        std::deque<Task> dropped;
        {
            std::unique_lock lock(mutex);
            closed = true;
            if (dropQueued) {
                dropped.swap(tasks);
                for (const auto& task : dropped) unfinished.erase(task.sequence);
                idle.notify_all();
            }
            idle.wait(lock, [&] { return tasks.empty() && running == 0; });
            joined.swap(workers);
        }
        ready.notify_all();
        for (const auto& worker : joined) Join(worker);
    }

private:
    static constexpr std::size_t StackBytes = 64u << 20u;

#ifdef _WIN32
    struct Worker {
        HANDLE handle;
        unsigned id;
    };

    static void Join(const Worker& worker) {
        if (worker.id != GetCurrentThreadId()) WaitForSingleObject(worker.handle, INFINITE);
        CloseHandle(worker.handle);
    }
#else
    using Worker = pthread_t;

    static void Join(Worker worker) {
        if (!pthread_equal(worker, pthread_self())) pthread_join(worker, nullptr);
    }
#endif

    explicit RegistrationPreparePool(unsigned count) {
#ifdef _WIN32
        for (unsigned index = 0; index < count; ++index) {
            unsigned id = 0;
            const auto thread = _beginthreadex(nullptr, static_cast<unsigned>(StackBytes), &RegistrationPreparePool::WindowsMain, this, STACK_SIZE_PARAM_IS_A_RESERVATION, &id);
            if (thread != 0) {
                workers.push_back({reinterpret_cast<HANDLE>(thread), id});
                ++threads;
                liveWorkers.fetch_add(1, std::memory_order_acq_rel);
            }
        }
#else
        pthread_attr_t attributes;
        pthread_attr_init(&attributes);
        pthread_attr_setstacksize(&attributes, StackBytes);
        for (unsigned index = 0; index < count; ++index) {
            pthread_t thread;
            if (pthread_create(&thread, &attributes, &RegistrationPreparePool::Main, this) == 0) {
                workers.push_back(thread);
                ++threads;
                liveWorkers.fetch_add(1, std::memory_order_acq_rel);
            }
        }
        pthread_attr_destroy(&attributes);
#endif
    }

#ifdef _WIN32
    static unsigned __stdcall WindowsMain(void* argument) {
        Main(argument);
        return 0;
    }
#endif

    static void* Main(void* argument) {
        auto& pool = *static_cast<RegistrationPreparePool*>(argument);
        if (PerformanceTrace::Configured()) PerformanceTrace::BindThread("shader prepare", TraceThread::DefaultCapacity / 8);
        for (;;) {
            Task task;
            {
                std::unique_lock lock(pool.mutex);
                pool.ready.wait(lock, [&] { return !pool.tasks.empty() || pool.closed; });
                if (pool.tasks.empty()) {
                    liveWorkers.fetch_sub(1, std::memory_order_acq_rel);
                    return nullptr;
                }
                task = std::move(pool.tasks.front());
                pool.tasks.pop_front();
                ++pool.running;
            }
            const auto sequence = task.sequence;
            task.function();
            task = {};
            bool drained = false;
            {
                std::lock_guard lock(pool.mutex);
                --pool.running;
                pool.unfinished.erase(sequence);
                drained = pool.tasks.empty() && pool.running == 0;
                pool.idle.notify_all();
            }
#ifdef __GLIBC__
            if (drained) trimFreedMemory();
#endif
        }
        return nullptr;
    }

    struct Task {
        std::function<void()> function;
        const PreparedShaders* prepared = nullptr;
        std::uint64_t sequence = 0;
    };

    std::mutex mutex;
    std::condition_variable ready;
    std::condition_variable idle;
    std::deque<Task> tasks;
    std::vector<Worker> workers;
    unsigned threads = 0;
    inline static std::atomic<unsigned> liveWorkers{0};
    unsigned running = 0;
    std::uint64_t submitted = 0;
    std::set<std::uint64_t> unfinished;
    bool closed = false;
};

bool AsyncRegistrationPrepare() {
    static const bool sync = std::getenv("APS5_SYNC_SHADER_PREPARE") != nullptr;
    return !sync;
}

void PrepareRegisteredInBackground(std::shared_ptr<const ShaderSnapshot> snapshot, std::shared_ptr<RegisteredPreparation> plan) {
    const auto* preparedKey = snapshot->prepared.get();
    SubmitRegistrationPreparation([snapshot = std::move(snapshot), plan = std::move(plan)] {
        PerformanceContext timingContext(FrameTiming::Preparation());
        std::vector<PreparedShaders::Entry> entries;
        std::exception_ptr failure;
        try {
            entries = PrepareRegistered(*plan);
        } catch (const std::exception& error) {
            APS5_LOG_ERR("Shader 0x%llx snapshot=0x%llx could not be prepared after registration: %s", static_cast<unsigned long long>(snapshot->codeAddress), static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(snapshot.get())), error.what());
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
    }, preparedKey);
}

}

void SubmitRegistrationPreparation(std::function<void()> task, const PreparedShaders* prepared) {
    RegistrationPreparePool::Instance().Submit(std::move(task), prepared);
}

void PrioritizeRegistrationPreparation(PreparedShaders& prepared) {
    prepared.demanded.store(true);
    if (auto* pool = RegistrationPreparePool::Existing()) pool->Prioritize(prepared);
}

void AwaitRegistrationPreparation() {
    if (auto* pool = RegistrationPreparePool::Existing()) pool->AwaitSubmitted();
}

void SubmitLibraryPrebuild(std::function<void()> task) {
    RegistrationPreparePool::Prebuild().Submit(std::move(task));
}

void AwaitLibraryPrebuilds() {
    if (auto* pool = RegistrationPreparePool::ExistingPrebuild()) pool->AwaitSubmitted();
}

void CloseRegistrationPreparation() {
    RegistrationPreparePool::CloseAll();
}

unsigned RegistrationPreparationThreads() {
    return RegistrationPreparePool::LiveWorkers();
}

std::vector<PreparedGraphicsStage> PrepareGraphicsStages(const DrawDecode& decoded, const ShaderRecompiler::SpirvTarget& target, bool deferUnprepared) {
    ShaderPreparationTransaction transaction;
    require(decoded.programs.size() == decoded.roles.size(), "graphics ABI program roles are incomplete");
    for (const auto& program : decoded.programs) require(program.snapshot != nullptr, "graphics ABI has no registered shader snapshot");
    std::vector<ShaderRecompiler::ProgramRole> expected;
    if (decoded.state.stages.path == Graphics::ShaderPath::Tessellation) {
        if (!decoded.roles.empty() && decoded.roles.front() == ShaderRecompiler::ProgramRole::Local) expected.push_back(ShaderRecompiler::ProgramRole::Local);
        expected.insert(expected.end(), {ShaderRecompiler::ProgramRole::Hull, ShaderRecompiler::ProgramRole::Domain});
    } else {
        expected.push_back(ShaderRecompiler::ProgramRole::Main);
        if (decoded.state.stages.path == Graphics::ShaderPath::Geometry && !decoded.programs.empty() && decoded.programs.front().snapshot->type == 4) expected.push_back(ShaderRecompiler::ProgramRole::GeometryBack);
    }
    if (!decoded.roles.empty() && decoded.roles.back() == ShaderRecompiler::ProgramRole::Fragment) expected.push_back(ShaderRecompiler::ProgramRole::Fragment);
    require(expected == decoded.roles, "graphics ABI linked programs do not match the stage routing");
    std::vector<ShaderRecompiler::LinkedProgram> linked;
    std::vector<ShaderRecompiler::MemoryRegion> memory;
    AppendProgramMemory(memory, decoded.programs);
    for (std::size_t index = 0; index < decoded.programs.size(); ++index) {
        const auto& program = decoded.programs[index];
        require(program.snapshot != nullptr && program.codeOffset < program.snapshot->code.size(), "graphics ABI has an invalid registered entry point");
        const auto code = std::span(program.snapshot->code).subspan(program.codeOffset);
        require(program.binary.code.data() == code.data() && program.binary.code.size() == code.size() && program.binary.codeAddress == program.snapshot->codeAddress + program.codeOffset * sizeof(std::uint32_t), "graphics ABI entry point differs from its snapshot");
        linked.push_back({decoded.roles[index], program.binary, program.userDataBase, program.firstUserSgpr, program.userData});
    }
    std::vector<PreparedGraphicsStage> prepared;
    std::uint32_t pushOffset = 0;
    const bool fixedSlots = target.fixedPushSlots;
    const auto capacity = decoded.state.stages.mesh ? ShaderRecompiler::MeshDrawPushOffsetBytes : decoded.state.stages.tessellation ? ShaderRecompiler::TessellationLevelPushOffsetBytes : decoded.state.depthRemap.mode != 0u ? ShaderRecompiler::DepthRemapPushOffsetBytes : Graphics::PipelinePushSlotBytes;
    for (std::size_t index = 0; index < decoded.programs.size(); ++index) {
        if (decoded.roles[index] == ShaderRecompiler::ProgramRole::GeometryBack) continue;
        const auto& program = decoded.programs[index];
        const bool fragment = program.binary.stage == ShaderRecompiler::ShaderStage::Fragment;
        const auto wave = fragment ? decoded.state.stages.fragmentWaveSize : decoded.state.stages.vertexWaveSize;
        pushOffset = Graphics::StagePushOffset(pushOffset, program.binary.stage, fixedSlots);
        const auto offset = pushOffset;
        const auto room = (fixedSlots && fragment ? Graphics::PipelinePushSlotBytes : capacity) - pushOffset % Graphics::PipelinePushSlotBytes;
        if (deferUnprepared && DeferFailedRegistration(*program.snapshot)) {
            prepared.push_back({program.snapshot, {program.codeOffset, nullptr}, program.binary.stage, offset, nullptr});
            continue;
        }
        std::optional<ShaderRecompiler::ShaderVertexStageInfo> vertex;
        try {
            if (!fragment) vertex = Graphics::DecodeVertexStageInfo(program.binary.header, program.binary.headerAddress, program.userData, nullptr, true, 8u - program.firstUserSgpr);
        } catch (const std::exception& error) {
            if (!deferUnprepared) throw;
            DeferPipelineStage(transaction.Edit(*program.snapshot), *program.snapshot, error.what());
            prepared.push_back({program.snapshot, {program.codeOffset, nullptr}, program.binary.stage, offset, nullptr});
            continue;
        }
        if (vertex) {
            vertex->paClVsOutCntl = PositionExportLayout(program.snapshot.get(), program.codeOffset, program.binary.code, decoded.state.paClVsOutCntl, program.tailContinuation.get()) | decoded.state.layerExports;
            Graphics::ApplyPointSize(decoded.state, *vertex);
        }
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
            try {
                handle = PrepareShaderWithDiagnostics(request);
            } catch (const std::exception& error) {
                if (!deferUnprepared) throw;
                DeferPipelineStage(transaction.Edit(*program.snapshot), *program.snapshot, error.what());
                prepared.push_back({program.snapshot, {program.codeOffset, nullptr}, program.binary.stage, offset, nullptr});
                continue;
            }
        }
        const auto bytes = handle->artifact->bindings.pushConstantSizeBytes;
        require(bytes <= room, "prepared graphics stages exceed the push constant block");
        std::shared_ptr<const ShaderRecompiler::RecompileResult> registration;
        if (target.registrationModules && !LibraryPrebuildDisabled()) {
            try {
                registration = ShaderRecompiler::RegistrationResult(request, *handle);
            } catch (const std::exception& error) {
                APS5_LOG_OUT("shader library prebuild skipped: %s", error.what());
            }
        }
        prepared.push_back({program.snapshot, {program.codeOffset, handle}, program.binary.stage, offset, std::move(registration)});
        pushOffset += bytes;
    }
    transaction.Commit();
    return prepared;
}

namespace {

void BindTitleThread() {
    if (PerformanceTrace::Configured()) PerformanceTrace::BindThread("title", TraceThread::DefaultCapacity / 8);
}

}

void Driver::ResolveGraphicsStagesAbi(std::span<const Shader* const> stages, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    BindTitleThread();
    PerformanceContext timingContext(FrameTiming::Preparation());
    PerformanceTimer timing("Shader.ResolveGraphicsStagesAbi");
    ShaderPreparationTransaction transaction;
    CheckFailure();
    require(!stages.empty(), "graphics ABI has no shader headers");
    QueueState state{};
    std::shared_ptr<const ShaderRegistry> registry;
    std::shared_ptr<const ShaderSnapshot> owner;
    std::vector<std::shared_ptr<const ShaderSnapshot>> selectedStages;
    {
        std::lock_guard lock(mutex);
        registry = shaders;
        for (const auto* shader : stages) {
            const auto fields = GuestShaderFields(shader);
            const auto address = reinterpret_cast<std::uintptr_t>(const_cast<const void*>(fields.code));
            require(registry != nullptr && registry->contains(address), "graphics ABI refers to an unregistered shader");
            const auto selected = RegisteredHeader(*registry, shader);
            require(selected != nullptr, "graphics ABI refers to a replaced shader header");
            selectedStages.push_back(selected);
            const auto& snapshot = *selected;
            require(snapshot.registeredState != nullptr, "registered shader state is missing");
            const auto registered = DecodeRegisteredState(snapshot, {});
            registered.shader.ForEach([&](std::uint32_t offset, std::uint32_t value) { state.shader.insert_or_assign(offset, value); });
            registered.context.ForEach([&](std::uint32_t offset, std::uint32_t value) { state.context.insert_or_assign(offset, value); });
            registered.userConfig.ForEach([&](std::uint32_t offset, std::uint32_t value) { state.userConfig.insert_or_assign(offset, value); });
            if (fields.type != snapshot.type) {
                const std::size_t count = fields.num_sh_registers;
                GuestMemory::CheckRange(fields.sh_registers, count * sizeof(ShaderRegister), alignof(ShaderRegister));
                for (std::size_t index = 0; index < count; ++index) state.shader.insert_or_assign(fields.sh_registers[index].offset, fields.sh_registers[index].value);
            }
        }
    }
    for (const auto reg : context) state.context.insert_or_assign(reg.offset, reg.value);
    for (const auto reg : primitive) state.userConfig.insert_or_assign(reg.offset, reg.value);
    const auto localDevice = device.Load();
    require(localDevice != nullptr, "graphics ABI device is missing");
    for (const auto& selected : selectedStages) {
        if (!DeferFailedRegistration(*selected) && owner == nullptr) owner = selected;
    }
    if (owner == nullptr) {
        transaction.Commit();
        return;
    }
    struct GraphicsAbiKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, GraphicsAbiKeyStorage>();
    BuildRegisteredAbiKey(state, *localDevice, key);
    const auto primitiveType = state.userConfig.find(0x242u);
    const auto resolveStages = [&](const std::vector<std::weak_ptr<const ShaderSnapshot>>& stages) {
        for (const auto& stage : stages) {
            const auto snapshot = stage.lock();
            require(snapshot != nullptr, "prepared graphics ABI lost a registered stage");
            if (snapshot->type != 1) ResolvePreparedGraphics(*snapshot, {}, primitiveType == state.userConfig.end() ? 0u : primitiveType->second, localDevice->Target(), true);
        }
        transaction.Commit();
    };
    if (!transaction.Tracks(*owner)) {
        if (const auto committed = CommittedGraphicsAbi(*owner, key, registry)) {
            resolveStages(*committed);
            return;
        }
    }
    for (const auto& abi : transaction.Read(*owner).graphicsAbis) {
        if (abi.key != key || abi.registry.lock() != registry) continue;
        resolveStages(abi.stages);
        return;
    }
    DrawDecode decoded{};
    try {
        decoded.state.stages = Graphics::DecodeShaderStages(state, VertexWorkgroupRequired(state, *registry));
        DecodeGraphicsPrograms(decoded, state, *registry, true, false);
    } catch (const std::exception& error) {
        for (const auto& selected : selectedStages) DeferPipelineStage(transaction.Edit(*selected), *selected, error.what());
        transaction.Commit();
        return;
    }
    auto prepared = PrepareGraphicsStages(decoded, localDevice->Target(), true);
    for (auto& stage : prepared) {
        if (stage.entry.handle == nullptr) continue;
        const auto& entries = transaction.Read(*stage.snapshot).entries;
        const auto duplicate = std::ranges::any_of(entries, [&](const auto& existing) { return existing.codeOffset == stage.entry.codeOffset && existing.handle->artifact == stage.entry.handle->artifact; });
        if (!duplicate) transaction.Edit(*stage.snapshot).entries.push_back(std::move(stage.entry));
    }
    for (const auto& stage : prepared) {
        if (stage.snapshot->type == 1) continue;
        ResolvePreparedGraphics(*stage.snapshot, {}, primitiveType == state.userConfig.end() ? 0u : primitiveType->second, localDevice->Target(), true);
    }
    if (decoded.state.stages.path == Graphics::ShaderPath::Vertex) {
        for (const auto& stage : prepared) {
            if (stage.registration != nullptr) PrebuildStageLibrary(localDevice, decoded.state.stages, stage.stage, stage.registration, stage.pushOffset);
        }
    }
    PreparedShaderState::GraphicsAbi abi{key, registry, {}};
    for (const auto& stage : prepared) abi.stages.push_back(stage.snapshot);
    auto& graphicsAbis = transaction.Edit(*owner).graphicsAbis;
    std::erase_if(graphicsAbis, [](const auto& entry) { return entry.registry.expired(); });
    graphicsAbis.push_back(std::move(abi));
    transaction.Commit();
}

std::size_t Driver::PreparedEntries(const Shader* shader) {
    std::shared_ptr<const ShaderSnapshot> snapshot;
    {
        std::lock_guard lock(mutex);
        if (shaders == nullptr) return 0;
        snapshot = RegisteredHeader(*shaders, shader);
    }
    if (snapshot == nullptr) return 0;
    std::unique_lock lock(snapshot->prepared->mutex);
    AwaitRegisteredPreparation(*snapshot->prepared, lock);
    return snapshot->prepared->entries.size();
}

Driver::DeferredAbiUseCounts Driver::DeferredAbiUses() {
    auto& counters = DeferredAbiCounters();
    const auto entries = counters.entries.load(std::memory_order_relaxed);
    const auto matched = counters.matchedEntries.load(std::memory_order_relaxed);
    return {counters.firstUses.load(std::memory_order_relaxed), counters.found.load(std::memory_order_relaxed), counters.own.load(std::memory_order_relaxed), entries, entries - std::min(entries, matched)};
}

std::vector<std::array<std::uint64_t, 2>> Driver::PreparedEntryHashes(const Shader* shader) {
    std::shared_ptr<const ShaderSnapshot> snapshot;
    {
        std::lock_guard lock(mutex);
        if (shaders == nullptr) return {};
        snapshot = RegisteredHeader(*shaders, shader);
    }
    if (snapshot == nullptr) return {};
    std::unique_lock lock(snapshot->prepared->mutex);
    AwaitRegisteredPreparation(*snapshot->prepared, lock);
    std::vector<std::array<std::uint64_t, 2>> hashes;
    for (const auto& entry : snapshot->prepared->entries) {
        if (entry.handle == nullptr || entry.handle->artifact == nullptr) continue;
        const auto hash = entry.handle->artifact->artifact.spirv.ContentHash();
        std::uint64_t key = 0xcbf29ce484222325ull;
        for (const auto word : entry.handle->staticKey) key = (key ^ word) * 0x100000001b3ull;
        hashes.push_back({hash.low, hash.high ^ key});
    }
    std::ranges::sort(hashes);
    return hashes;
}

std::array<std::uint64_t, 4> Driver::PrebuiltLibraries() {
    if (auto* pool = RegistrationPreparePool::ExistingPrebuild()) pool->AwaitSubmitted();
    const auto localDevice = device.Load();
    if (localDevice == nullptr) return {};
    return Graphics::PipelineLibraryCountersOf(localDevice->Device()).built;
}

void Driver::ResolveShaderAbi(const Shader* shader, std::span<const ShaderRegister> context, std::span<const ShaderRegister> primitive) {
    BindTitleThread();
    PerformanceContext timingContext(FrameTiming::Preparation());
    PerformanceTimer timing("Shader.ResolveAbi");
    ShaderPreparationTransaction transaction;
    CheckFailure();
    const auto fields = GuestShaderFields(shader);
    if (fields.type != 0 && fields.type != 1) {
        const std::array<const Shader*, 1> stages{shader};
        ResolveGraphicsStagesAbi(stages, context, primitive);
        transaction.Commit();
        return;
    }
    std::shared_ptr<const ShaderSnapshot> snapshot;
    {
        std::lock_guard lock(mutex);
        const auto address = reinterpret_cast<std::uintptr_t>(const_cast<const void*>(fields.code));
        require(shaders != nullptr && shaders->contains(address), "static ABI refers to an unregistered shader");
        snapshot = RegisteredHeader(*shaders, shader);
        require(snapshot != nullptr, "static ABI refers to a replaced shader header");
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
    if (DeferFailedRegistration(*snapshot)) {
        transaction.Commit();
        return;
    }
    struct RegisteredAbiKeyStorage {};
    auto& key = HostThreadLocal<std::vector<std::uint64_t>, RegisteredAbiKeyStorage>();
    BuildRegisteredAbiKey(state, *localDevice, key);
    if (LazyShaderPreparation()) {
        const auto& resolved = transaction.Read(*snapshot).registeredAbis;
        if (std::ranges::find(resolved, key) != resolved.end()) {
            transaction.Commit();
            return;
        }
        std::optional<std::string> invalid;
        try {
            static_cast<void>(PlanRegistered(*snapshot, *localDevice, state, false));
        } catch (const std::exception& error) {
            invalid = error.what();
        }
        auto& prepared = transaction.Edit(*snapshot);
        if (invalid) DeferPipelineStage(prepared, *snapshot, invalid->c_str());
        prepared.registeredAbis.push_back(key);
        prepared.deferred = true;
        transaction.Commit();
        if (invalid) return;
        PreparedShaders::DeferredAbi deferred{key, {}, {}, localDevice};
        for (const auto reg : context) deferred.context.emplace_back(reg.offset, reg.value);
        for (const auto reg : primitive) deferred.primitive.emplace_back(reg.offset, reg.value);
        std::lock_guard lock(snapshot->prepared->mutex);
        snapshot->prepared->deferredAbis.push_back(std::move(deferred));
        return;
    }
    std::optional<std::string> deferral;
    const auto prepare = [&] {
        try {
            const auto plan = PlanRegistered(*snapshot, *localDevice, state, false);
            if (plan == nullptr) return std::vector<PreparedShaders::Entry>{};
            plan->prebuildDevice = localDevice;
            return PrepareRegistered(*plan);
        } catch (const std::exception& error) {
            deferral = error.what();
            return std::vector<PreparedShaders::Entry>{};
        }
    };
    const bool committedAbi = CommittedAbi(*snapshot, key);
    if (committedAbi && !transaction.Tracks(*snapshot)) {
        transaction.Commit();
        return;
    }
    auto entries = committedAbi ? std::vector<PreparedShaders::Entry>{} : prepare();
    const auto& registeredAbis = transaction.Read(*snapshot).registeredAbis;
    if (std::ranges::find(registeredAbis, key) != registeredAbis.end()) {
        transaction.Commit();
        return;
    }
    if (committedAbi) entries = prepare();
    auto& prepared = transaction.Edit(*snapshot);
    for (auto& entry : entries) {
        const auto duplicate = std::ranges::any_of(prepared.entries, [&](const auto& existing) { return existing.handle->artifact == entry.handle->artifact; });
        if (!duplicate) prepared.entries.push_back(std::move(entry));
    }
    if (deferral) DeferPipelineStage(prepared, *snapshot, deferral->c_str());
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

alignas(256) static const std::uint32_t DepthCopyMultisamplePixelCode[64] = {0x360604ffu, 0x0000ffffu, 0x2c080490u, 0x7e0a0208u, 0xf0000130u, 0x00000603u, 0xbf8c3f70u, 0xf8001801u, 0x00000006u, 0xbf810000u};
alignas(256) static const std::uint32_t DepthCopySinglePixelCode[64] = {0x360604ffu, 0x0000ffffu, 0x2c080490u, 0xf0000108u, 0x00000603u, 0xbf8c3f70u, 0xf8001801u, 0x00000006u, 0xbf810000u};
static const Shader DepthCopyMultisamplePixelShader = [] {
    Shader shader{};
    shader.file_header = 0x34333231u;
    shader.version = 0x18u;
    shader.code = DepthCopyMultisamplePixelCode;
    shader.header_size = sizeof(Shader);
    shader.shader_size = sizeof(DepthCopyMultisamplePixelCode);
    shader.type = 1;
    return shader;
}();
static const Shader DepthCopySinglePixelShader = [] {
    Shader shader{};
    shader.file_header = 0x34333231u;
    shader.version = 0x18u;
    shader.code = DepthCopySinglePixelCode;
    shader.header_size = sizeof(Shader);
    shader.shader_size = sizeof(DepthCopySinglePixelCode);
    shader.type = 1;
    return shader;
}();

std::uint64_t DepthCopyPixelProgramAddress(bool multisampled) {
    return reinterpret_cast<std::uintptr_t>(multisampled ? DepthCopyMultisamplePixelCode : DepthCopySinglePixelCode);
}

std::uint32_t DepthCopyPixelUserWords(bool multisampled) {
    return multisampled ? 9u : 8u;
}

void Driver::ResolveGraphicsAbi(const Shader* vertex, const Shader* pixel, std::uint32_t primitiveType) {
    BindTitleThread();
    PerformanceContext timingContext(FrameTiming::Preparation());
    PerformanceTimer timing("Shader.ResolveGraphicsAbi");
    CheckFailure();
    if ((primitiveType != 0 && primitiveType != 7 && primitiveType != 17) || pixel == nullptr) return;
    ShaderPreparationTransaction transaction;
    require(vertex != nullptr && pixel != nullptr, "rectangle ABI requires vertex and fragment shaders");
    std::shared_ptr<const ShaderSnapshot> front;
    std::shared_ptr<const ShaderSnapshot> fragment;
    {
        std::lock_guard lock(mutex);
        const auto lookup = [&](const Shader* shader) {
            const auto address = reinterpret_cast<std::uintptr_t>(const_cast<const void*>(GuestShaderFields(shader).code));
            require(shaders != nullptr && shaders->contains(address), "rectangle ABI refers to an unregistered shader");
            const auto snapshot = RegisteredHeader(*shaders, shader);
            require(snapshot != nullptr, "rectangle ABI refers to a replaced shader header");
            return snapshot;
        };
        front = lookup(vertex);
        fragment = lookup(pixel);
    }
    require(front != fragment, "rectangle stages refer to the same shader");
    const auto localDevice = device.Load();
    require(localDevice != nullptr, "shader registration device is missing");
    ResolvePreparedGraphics(*front, fragment, primitiveType, localDevice->Target(), true);
    transaction.Commit();
}

void Driver::SettleShaderPreparations(std::span<const Shader* const> stages) {
    std::vector<std::shared_ptr<const ShaderSnapshot>> snapshots;
    {
        std::lock_guard lock(mutex);
        if (shaders == nullptr) return;
        for (const auto* shader : stages) {
            if (shader == nullptr) continue;
            if (auto snapshot = RegisteredHeader(*shaders, shader)) snapshots.push_back(std::move(snapshot));
        }
    }
    for (const auto& snapshot : snapshots) static_cast<void>(FailedRegistration(*snapshot));
}

void ResolvePreparedGraphics(const ShaderSnapshot& front, const std::shared_ptr<const ShaderSnapshot>& fragment, std::uint32_t primitiveType, const ShaderRecompiler::SpirvTarget& target, bool deferUnprepared) {
    ShaderPreparationTransaction transaction;
    require(fragment.get() != &front, "rectangle stages refer to the same shader");
    if (deferUnprepared) {
        const bool frontFailed = DeferFailedRegistration(front);
        if ((fragment != nullptr && DeferFailedRegistration(*fragment)) || frontFailed) {
            transaction.Commit();
            return;
        }
    }
    if (!transaction.Tracks(front) && CommittedLink(front, fragment, primitiveType)) {
        transaction.Commit();
        return;
    }
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
        if ((current.entries.empty() && current.deferred) || (pixelPrepared.entries.empty() && pixelPrepared.deferred)) continue;
        if (current.entries.empty() || pixelPrepared.entries.empty()) {
            require(deferUnprepared || pixel != fragment, "rectangle ABI has missing stage artifacts");
            continue;
        }
        const auto progress = std::ranges::find_if(current.rectangleProgress, [&](const auto& entry) { return entry.fragment.lock() == pixel; });
        const auto vertexCount = progress == current.rectangleProgress.end() ? 0u : progress->vertexCount;
        const auto fragmentCount = progress == current.rectangleProgress.end() ? 0u : progress->fragmentCount;
        if (vertexCount == current.entries.size() && fragmentCount == pixelPrepared.entries.size()) continue;
        auto& prepared = transaction.Edit(front);
        std::vector<PreparedShaders::Rectangle> rectangles;
        try {
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
        } catch (const std::exception& error) {
            if (!deferUnprepared && pixel == fragment) throw;
            if (deferUnprepared) DeferPipelineStage(transaction.Edit(*pixel), *pixel, error.what());
            continue;
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

ShaderSnapshot PrepareDepthCopyPixelProgram(bool multisampled) {
    const auto& shader = multisampled ? DepthCopyMultisamplePixelShader : DepthCopySinglePixelShader;
    const auto* code = multisampled ? DepthCopyMultisamplePixelCode : DepthCopySinglePixelCode;
    ShaderSnapshot copy{DepthCopyPixelProgramAddress(multisampled), reinterpret_cast<std::uintptr_t>(&shader), shader.type, {}, {}};
    copy.code.assign(code, code + 64);
    copy.header.resize(sizeof(Shader));
    std::memcpy(copy.header.data(), &shader, sizeof(Shader));
    auto state = DecodeRegisteredState(copy);
    state.shader.insert_or_assign(0x008u, static_cast<std::uint32_t>(copy.codeAddress >> 8u));
    state.shader.insert_or_assign(0x009u, static_cast<std::uint32_t>(copy.codeAddress >> 40u));
    state.shader.insert_or_assign(0x00bu, DepthCopyPixelUserWords(multisampled) << 1u);
    state.context.insert_or_assign(0x1b3u, 0x8002u);
    state.context.insert_or_assign(0x1b4u, 0x8002u);
    state.context.insert_or_assign(0x1c5u, 1u);
    copy.registeredState = std::make_shared<const RegisteredShaderState>(std::move(state));
    copy.prepared->deferred = true;
    return copy;
}

std::shared_ptr<const ShaderRegistry> Driver::RegisteredShaders() {
    std::lock_guard lock(mutex);
    return shaders;
}

void Driver::RegisterShader(const Shader* shader) {
    BindTitleThread();
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
    require(!RawGraphicsDrawnIn(reinterpret_cast<std::uintptr_t>(code), reinterpret_cast<std::uintptr_t>(code) + fields.shader_size), "a shader was registered over a graphics program that was drawn without registration");
    ShaderSnapshot snapshot{reinterpret_cast<std::uintptr_t>(code), reinterpret_cast<std::uintptr_t>(shader), fields.type, {}, {}};
    snapshot.code.resize(fields.shader_size / sizeof(std::uint32_t));
    std::memcpy(snapshot.code.data(), code, fields.shader_size);
    snapshot.header.resize(fields.header_size);
    std::memcpy(snapshot.header.data(), static_cast<const void*>(shader), fields.header_size);

    transaction.Hold();
    {
        std::lock_guard lock(mutex);
        rethrowFailure();
        if (shaders != nullptr) {
            const auto found = shaders->find(snapshot.codeAddress);
            if (found != shaders->end()) {
                for (const auto& current : found->second) {
                    if (current->headerAddress == snapshot.headerAddress && current->type == snapshot.type && current->code == snapshot.code && current->header == snapshot.header) {
                        const auto identical = current;
                        PublishRegisteredShader(shaders, identical);
                        transaction.Commit();
                        return;
                    }
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
    {
        std::lock_guard lock(mutex);
        if (shaders != nullptr) {
            if (const auto found = shaders->find(snapshot.codeAddress); found != shaders->end()) {
                for (const auto& current : found->second) {
                    if (current->registeredState == nullptr || current->type != snapshot.type || current->code != snapshot.code || *current->registeredState != *snapshot.registeredState) continue;
                    snapshot.registeredState = current->registeredState;
                    break;
                }
            }
        }
    }
    QueueState registered{};
    registered.shader = snapshot.registeredState->shader;
    registered.context = snapshot.registeredState->context;
    registered.userConfig = snapshot.registeredState->userConfig;
    std::shared_ptr<RegisteredPreparation> preparation;
    if (LazyShaderPreparation()) {
        static_cast<void>(PlanRegistered(snapshot, *localDevice, registered, true, &snapshot.prepared->deferred));
        snapshot.prepared->deferred = true;
    } else if (AsyncRegistrationPrepare()) {
        preparation = PlanRegistered(snapshot, *localDevice, registered, true, &snapshot.prepared->deferred);
        if (preparation != nullptr) preparation->prebuildDevice = localDevice;
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
    for (const bool multisampled : {false, true}) {
        if (shaders->find(DepthCopyPixelProgramAddress(multisampled)) == shaders->end()) PublishRegisteredShader(shaders, std::make_shared<const ShaderSnapshot>(PrepareDepthCopyPixelProgram(multisampled)));
    }
    transaction.Commit();
    if (preparation != nullptr) PrepareRegisteredInBackground(published, std::move(preparation));
}

}

extern "C" AgcDriver::DriverDetail::ShaderPreparationTransaction* AgcDriverBeginShaderPreparation_nid_postfix(const Shader* const* shaders, std::size_t count) {
    AgcDriver::DriverDetail::Driver::Get().SettleShaderPreparations({shaders, count});
    return new AgcDriver::DriverDetail::ShaderPreparationTransaction();
}

extern "C" void AgcDriverCommitShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction) {
    transaction->Commit();
}

extern "C" void AgcDriverEndShaderPreparation_nid_postfix(AgcDriver::DriverDetail::ShaderPreparationTransaction* transaction) noexcept {
    delete transaction;
}
