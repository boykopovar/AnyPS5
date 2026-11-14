#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "Optimization/ResourceProgram.hpp"
#include "BdaAbi.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <new>
#include <numeric>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace AgcDriver::DriverDetail {

struct DriverTestAccess {
    static std::shared_ptr<DispatchVariant> Insert(const std::shared_ptr<const ShaderSnapshot>& shader, const ShaderRecompiler::RecompileRequest& request, bool disabled = false, std::vector<std::weak_ptr<const ShaderRecompiler::SrtGuestWords>>* dependencies = nullptr) {
        auto& driver = Driver::Get();
        auto memory = std::make_shared<ShaderMemory>(request.context.memory);
        std::shared_ptr<const PreparedShaderState::UserDataLink> linking;
        auto invocation = InvocationFor(*shader, 0, request, &linking);
        const auto forget = GuestMemory::ForgetSerial();
        const auto capture = CaptureInvocation(*memory, *shader, 0, request, invocation, &linking);
        if (dependencies != nullptr && linking != nullptr) dependencies->assign(linking->dependencies.begin(), linking->dependencies.end());
        const auto captured = memory->Regions();
        const auto result = invocation.Materialize(*capture);
        std::shared_ptr<DispatchVariant> attached;
        auto lap = std::chrono::steady_clock::now();
        auto phaseLap = lap;
        std::array<double, DriverPhaseCount> phases{};
        DispatchPhaseTiming timing{false, lap, phases, phaseLap};
        driver.insertDispatch(shader->codeAddress, 14u, disabled, false, shader, forget, request.context.memory, memory, captured, capture, result, {}, false, attached, timing, linking);
        return attached;
    }

    static void Evict() {
        auto& driver = Driver::Get();
        std::lock_guard lock(driver.dispatchCacheMutex);
        driver.eraseDispatchEntry(driver.dispatchCache.find(14u));
    }

    static std::shared_ptr<DispatchVariant> Lookup(const ShaderSnapshot& shader, const ShaderRecompiler::RecompileRequest& request, std::vector<ShaderRecompiler::MemoryRegion>* retained = nullptr, std::vector<std::uint32_t>* liveWords = nullptr, std::shared_ptr<const ShaderRecompiler::RecompileResult>* patched = nullptr) {
        auto& driver = Driver::Get();
        Submission submission{};
        std::shared_ptr<const ShaderRecompiler::RecompileResult> result;
        std::shared_ptr<DispatchVariant> kept;
        std::shared_ptr<DispatchEntry> missed;
        std::vector<ShaderRecompiler::MemoryRegion> captured;
        std::vector<std::uint32_t> words;
        bool data = false, cached = false, validated = false, differing = false;
        auto lap = std::chrono::steady_clock::now();
        auto phaseLap = lap;
        std::array<double, DriverPhaseCount> phases{};
        DispatchPhaseTiming timing{false, lap, phases, phaseLap};
        driver.lookupDispatch(shader.codeAddress, submission, 14u, false, false, false, request.context.memory, timing, phases, result, kept, captured, words, data, cached, validated, missed, differing);
        if (retained != nullptr) *retained = captured;
        if (liveWords != nullptr) {
            if (!data) throw std::runtime_error("the verifier test did not get a data hit");
            *liveWords = words;
        }
        if (patched != nullptr) *patched = result;
        return cached ? kept : nullptr;
    }

    static void Verify(const ShaderSnapshot& shader, const ShaderRecompiler::RecompileRequest& request, const DispatchVariant& variant, std::span<const std::uint32_t> liveWords, const ShaderRecompiler::RecompileResult& patched) {
        auto& driver = Driver::Get();
        if (!driver.verifyDataHits()) throw std::runtime_error("data-hit verification is disabled");
        const auto before = driver.DispatchCacheCounters().dataVerified;
        driver.verifyDataHit(shader, 0, request, request.context.memory, shader.codeAddress, variant, liveWords, patched);
        if (driver.DispatchCacheCounters().dataVerified != before + 1u) throw std::runtime_error("the data hit was not verified");
    }

    static bool Final(std::span<const ShaderRecompiler::MemoryRegion> captured, bool pipelined, const PreparedShaderState::UserDataLink* linking) {
        std::vector<Graphics::GuestMemorySnapshot> snapshots;
        if (!pipelined) for (const auto& region : captured) snapshots.push_back({region.guestAddress, region.bytes});
        const bool current = !Driver::capturedMemoryChanged(captured, snapshots, linking);
        if (current && !pipelined && std::any_of(snapshots.begin(), snapshots.end(), [](const auto& snapshot) { return !snapshot.compared; })) throw std::runtime_error("a final proof did not mark its snapshot current");
        return current;
    }

    static bool Adopt(std::span<const ShaderRecompiler::MemoryRegion> captured, const ShaderSnapshot& shader, const PreparedShaderState::UserDataLink* linking) {
        return Driver::capturedCurrent(captured, shader, linking);
    }
};

}

namespace {

using namespace AgcDriver::DriverDetail;

alignas(256) constexpr std::array<std::uint32_t, 19> DirectCaller{
    0x7e020280u, 0xbe940383u, 0xbe8e0300u, 0xbe8f0301u, 0xbe8e210eu, 0xbe8e0302u, 0xbe8f0303u, 0xbe8e210eu,
    0x80948114u, 0xbf078014u, 0xbf85fff7u, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u,
    0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 20> TableCaller{
    0x7e020280u, 0xf4080400u, 0xfa000000u, 0x8f148402u, 0xbf8cc07fu, 0xf4280208u, 0x28000000u, 0xbf8cc07fu,
    0xbe8e0308u, 0xbe8f0309u, 0xbf068103u, 0xbe8e210eu, 0x34040082u, 0xe0701000u, 0x80010102u, 0xbf810000u,
    0xbf9f0000u, 0x30306c73u, 0x00000040u, 0x00000008u};
alignas(256) constexpr std::array<std::uint32_t, 21> NestedCaller{
    0x34040082u, 0x7e0202ffu, 0x00000055u, 0xf4080200u, 0xfa000000u, 0xbf8cc07fu, 0xf4281204u, 0xfa000040u, 0xbf8cc07fu, 0xbe880381u, 0x8f6a8408u,
    0xf4280224u, 0xd4000000u, 0xbf8cc07fu, 0xbe8e0308u, 0xbe8f0309u, 0xbe8e210eu, 0xe0701000u, 0x80010102u, 0xbf810000u, 0xbf9f0000u};
struct Inputs {
    alignas(4096) std::array<std::uint32_t, 1024> first{};
    alignas(4096) std::array<std::uint32_t, 1024> second{};
    alignas(16) std::array<std::uint64_t, 4> table{};
    alignas(16) std::array<std::uint64_t, 4> otherTable{};
    alignas(16) std::array<std::uint32_t, 4> descriptor{};
    alignas(16) std::array<std::uint32_t, 20> outer{};
    alignas(16) std::array<std::uint32_t, 4> outerDescriptor{};
    alignas(256) std::array<std::uint32_t, 64> output{};
    std::uint32_t data = 7u;
};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::span<const std::shared_ptr<const ShaderRecompiler::SrtGuestWords>> countedDependencies;
std::size_t comparedBytes = 0;
std::size_t dependencyBytes = 0;
std::size_t dependencyCompares = 0;

void CountCompare(std::uint64_t address, std::size_t bytes) {
    comparedBytes += bytes;
    bool dependency = false;
    for (const auto& block : countedDependencies) {
        for (const auto& [begin, count] : block->runs) {
            const auto end = begin + count * sizeof(std::uint32_t);
            const auto low = std::max(address, begin);
            const auto high = std::min(address + bytes, end);
            if (low < high) {
                dependencyBytes += high - low;
                dependency = true;
            }
        }
    }
    if (dependency) ++dependencyCompares;
}

void Run(const std::string& mode, std::string_view watch) {
    const bool watched = !watch.empty();
    auto ordinary = std::make_unique<Inputs>();
    constexpr std::size_t ArenaBytes = 65536u;
    std::unique_ptr<Inputs, void (*)(Inputs*)> arena(nullptr, [](Inputs* pointer) {
        if (pointer != nullptr) GuestArena::GuestArenaRelease_nid_postfix(pointer, ArenaBytes);
    });
    if (watched) {
        auto* pointer = GuestArena::GuestArenaAllocate_nid_postfix(ArenaBytes, ArenaBytes);
        Require(pointer != nullptr, "cannot allocate watched linking inputs");
        GuestArena::GuestArenaCommit_nid_postfix(pointer, ArenaBytes, 0x04u, ArenaBytes);
#ifndef _WIN32
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(pointer, ArenaBytes);
#endif
        arena.reset(new (pointer) Inputs{});
    }
    const bool allWatched = watch == "--watched";
    auto& First = allWatched || watch == "--watch-first" ? arena->first : ordinary->first;
    auto& Second = allWatched ? arena->second : ordinary->second;
    auto& Table = allWatched || watch == "--watch-table" ? arena->table : ordinary->table;
    auto& OtherTable = allWatched || watch == "--watch-table" ? arena->otherTable : ordinary->otherTable;
    auto& Descriptor = allWatched || watch == "--watch-descriptor" ? arena->descriptor : ordinary->descriptor;
    auto& Outer = allWatched || watch == "--watch-descriptor" ? arena->outer : ordinary->outer;
    auto& OuterDescriptor = allWatched || watch == "--watch-descriptor" ? arena->outerDescriptor : ordinary->outerDescriptor;
    auto& Output = allWatched ? arena->output : ordinary->output;
    auto& Data = allWatched ? arena->data : ordinary->data;
    First[0] = 0x4a020285u;
    First[1] = 0xbe80200eu;
    Second[0] = 0x4a0202c0u;
    Second[1] = 0xbe80200eu;
    const auto first = reinterpret_cast<std::uintptr_t>(First.data());
    const auto second = reinterpret_cast<std::uintptr_t>(Second.data());
    const bool verify = mode.starts_with("data-hit-verify");
    const bool table = verify ? mode == "data-hit-verify-table" : mode != "direct" && mode != "eviction" && mode != "disabled" && mode != "final-direct";
    Table = {first, 0u, 0u, 0u};
    OtherTable = {second, 0u, 0u, 0u};
    const auto tableAddress = reinterpret_cast<std::uintptr_t>(Table.data());
    Descriptor = {static_cast<std::uint32_t>(tableAddress), static_cast<std::uint32_t>(tableAddress >> 32u) | (16u << 16u), 2u, 0x31016facu};
    const bool nested = mode == "nested";
    const auto outerAddress = reinterpret_cast<std::uintptr_t>(Outer.data());
    std::copy(Descriptor.begin(), Descriptor.end(), Outer.begin() + 16u);
    OuterDescriptor = {static_cast<std::uint32_t>(outerAddress), static_cast<std::uint32_t>(outerAddress >> 32u) & 0xffffu, static_cast<std::uint32_t>(sizeof(Outer)), 0x31016facu};
    const auto pointer = nested ? reinterpret_cast<std::uintptr_t>(OuterDescriptor.data()) : table ? reinterpret_cast<std::uintptr_t>(Descriptor.data()) : first;
    const auto output = reinterpret_cast<std::uintptr_t>(Output.data());
    const auto data = reinterpret_cast<std::uintptr_t>(&Data);
    const std::array<std::uint32_t, 10> users{static_cast<std::uint32_t>(pointer), static_cast<std::uint32_t>(pointer >> 32u), table ? 0u : static_cast<std::uint32_t>(second), nested ? 0u : table ? 1u : static_cast<std::uint32_t>(second >> 32u),
        static_cast<std::uint32_t>(output), static_cast<std::uint32_t>(output >> 32u), sizeof(Output), 0x31016facu, static_cast<std::uint32_t>(data), static_cast<std::uint32_t>(data >> 32u)};
    auto code = nested ? std::span<const std::uint32_t>(NestedCaller) : table ? std::span<const std::uint32_t>(TableCaller) : std::span<const std::uint32_t>(DirectCaller);
    std::vector<std::uint32_t> dataCode;
    if (verify) {
        dataCode = {0xf4000204u, 0xfa000000u, 0xbf8cc07fu, 0x7e020208u};
        dataCode.insert(dataCode.end(), code.begin() + 1u, code.end());
        code = dataCode;
    }
    auto shader = std::make_shared<ShaderSnapshot>(ShaderSnapshot{reinterpret_cast<std::uintptr_t>(code.data()), 0, 0, {code.begin(), code.end()}, {}});
    const std::array<ShaderRecompiler::MemoryRegion, 1> initial{{{shader->codeAddress, std::as_bytes(std::span(shader->code))}}};
    const ShaderRecompiler::ShaderComputeStageInfo compute{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    ShaderRecompiler::RecompileRequest request{{ShaderRecompiler::ShaderStage::Compute, shader->codeAddress, shader->code, 0, {}}, {32u, 0u, std::span(users).first(verify ? 10u : 8u), compute, std::nullopt, std::nullopt, initial}, {}, {0u, 0u, 0u, 128u}};
    static constexpr std::array<std::uint32_t, 6> capabilities{spv::CapabilityShader, spv::CapabilityImageGatherExtended, spv::CapabilityInt64, spv::CapabilityPhysicalStorageBufferAddresses, spv::CapabilityStorageBuffer8BitAccess, spv::CapabilityClipDistance};
    static constexpr std::array<std::string_view, 2> extensions{"SPV_KHR_physical_storage_buffer", "SPV_KHR_8bit_storage"};
    request.target.bdaAbiVersion = ShaderRecompiler::BdaAbi::Version;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.vulkanVersion = 0x00401000u;
    request.target.spirvVersion = 0x00010300u;
    request.target.subgroupSize = 32u;
    if (mode == "disabled") {
        std::vector<std::weak_ptr<const ShaderRecompiler::SrtGuestWords>> dependencies;
        Require(DriverTestAccess::Insert(shader, request, true, &dependencies) == nullptr, "disabled dispatch caching retained a variant");
        Require(dependencies.size() == 2u, "uncached direct linking did not capture both callees");
        const auto footprint = Driver::Get().DispatchCacheSize();
        Require(footprint.entries == 0u && footprint.variantBytes == 0u, "disabled dispatch caching retained byte accounting");
        for (const auto& dependency : dependencies) Require(dependency.expired(), "uncached dependency snapshot survived outside the dispatch-cache budget");
        return;
    }
    auto original = DriverTestAccess::Insert(shader, request);
    Require(original != nullptr, "the initial prepared dispatch was not cached");
    Require(DriverTestAccess::Lookup(*shader, request) == original, "unchanged linking inputs missed the dispatch cache");
    if (nested) Require(original->linking != nullptr && original->linking->Overlaps(outerAddress + 64u, 16u), "the nested descriptor is not a linking dependency");
    if (verify) {
        Require(!original->dataPositions.empty() && original->linking != nullptr && !original->linking->dependencies.empty(), "the verifier fixture has no data words or external callees");
        Data = 11u;
        AgcDriver::GuestMemory::BumpCollectEpoch();
        std::vector<std::uint32_t> liveWords;
        std::shared_ptr<const ShaderRecompiler::RecompileResult> patched;
        Require(DriverTestAccess::Lookup(*shader, request, nullptr, &liveWords, &patched) == original, "a data-only SRT write missed the linked dispatch");
        DriverTestAccess::Verify(*shader, request, *original, liveWords, *patched);
        if (mode == "data-hit-verify-stale-data" || mode == "data-hit-verify-stale-code") {
            if (mode == "data-hit-verify-stale-data") Data = 19u;
            else First[0] = 0x4a020281u;
            AgcDriver::GuestMemory::BumpCollectEpoch();
            std::signal(SIGABRT, [](int) { std::_Exit(0); });
            DriverTestAccess::Verify(*shader, request, *original, liveWords, *patched);
            throw std::runtime_error("the data-hit verifier accepted a stale hit");
        }
        std::puts("linked data-only SRT hit verified");
        return;
    }
    if (mode.starts_with("final-")) {
        for (std::size_t path = 0; path < 3; ++path) {
            AgcDriver::GuestMemory::BumpCollectEpoch();
            std::vector<ShaderRecompiler::MemoryRegion> captured;
            Require(DriverTestAccess::Lookup(*shader, request, &captured) == original, "the final-change test did not get a hit");
            if (mode == "final-row") Table[2] = second;
            else if (mode == "final-descriptor") Descriptor[0] += 16u;
            else First[0] = 0x4a020281u;
            if (path == 0) Require(!DriverTestAccess::Final(captured, false, original->linking.get()), "the final snapshot check reused changed linking inputs");
            else if (path == 1) Require(!DriverTestAccess::Final(captured, true, original->linking.get()), "the pipelined final check reused changed linking inputs");
            else Require(!DriverTestAccess::Adopt(captured, *shader, original->linking.get()), "ahead adoption reused changed linking inputs");
            First[0] = 0x4a020285u;
            Table[2] = 0u;
            Descriptor[0] = static_cast<std::uint32_t>(tableAddress);
        }
        return;
    }
    if (mode == "unused") {
        for (std::uint64_t value = 1; value <= 16; ++value) {
            Table[1] = value;
            Table[3] = value;
            AgcDriver::GuestMemory::BumpCollectEpoch();
            std::vector<ShaderRecompiler::MemoryRegion> captured;
            Require(DriverTestAccess::Lookup(*shader, request, &captured) == original, "an unused table field rejected an unchanged link");
            Require(DriverTestAccess::Final(captured, false, original->linking.get()) && DriverTestAccess::Final(captured, true, original->linking.get()) && DriverTestAccess::Adopt(captured, *shader, original->linking.get()), "an unused table field rejected a final or adoption check");
        }
        Table[2] = second;
        AgcDriver::GuestMemory::BumpCollectEpoch();
        Require(DriverTestAccess::Lookup(*shader, request) == nullptr, "a formerly zero target slot reused the stale link");
        std::puts("16 unused-field writes hit; zero-to-target write missed");
        return;
    }
    if (mode == "hit-cost" || mode == "recover") {
        Require(original->linking != nullptr, "the cost test has no linking dependencies");
        countedDependencies = original->linking->dependencies;
        AgcDriver::GuestMemory::SetCompareMappedHook(&CountCompare);
        struct ResetHook {
            ~ResetHook() { AgcDriver::GuestMemory::SetCompareMappedHook(nullptr); countedDependencies = {}; }
        } reset;
        const auto check = [&](std::size_t expectedBytes, std::size_t expectedCompares, const char* phase) {
            comparedBytes = dependencyBytes = dependencyCompares = 0;
            std::vector<ShaderRecompiler::MemoryRegion> captured;
            Require(DriverTestAccess::Lookup(*shader, request, &captured) == original, "the cost test missed an unchanged link");
            const auto lookupBytes = dependencyBytes;
            Require(DriverTestAccess::Final(captured, false, original->linking.get()), "the final snapshot check rejected unchanged inputs");
            const auto finalBytes = dependencyBytes - lookupBytes;
            Require(DriverTestAccess::Final(captured, true, original->linking.get()), "the pipelined final check rejected unchanged inputs");
            const auto pipelineBytes = dependencyBytes - lookupBytes - finalBytes;
            Require(DriverTestAccess::Adopt(captured, *shader, original->linking.get()), "ahead adoption rejected unchanged inputs");
            std::printf("%s: %zu dependency bytes, %zu dependency comparisons, %zu total bytes; lookup %zu, final %zu, pipeline %zu, adopt %zu; %zu captured ranges\n", phase, dependencyBytes, dependencyCompares, comparedBytes, lookupBytes, finalBytes, pipelineBytes, dependencyBytes - lookupBytes - finalBytes - pipelineBytes, captured.size());
            Require(dependencyBytes == expectedBytes && dependencyCompares == expectedCompares, "dispatch final/adoption checks compared proved dependencies or failed to recover their proof");
        };
        if (mode == "recover") {
            First[16] = 1u;
            AgcDriver::GuestMemory::BumpCollectEpoch();
            check(8u, 1u, "dirty neighbour");
        }
        for (std::size_t hit = 0; hit < 10; ++hit) {
            AgcDriver::GuestMemory::BumpCollectEpoch();
            check(0u, 0u, "unchanged hit");
        }
        return;
    }
    if (mode == "eviction") {
        const auto footprint = Driver::Get().DispatchCacheSize();
        Require(footprint.entries == 1u && footprint.variantBytes >= 16u, "cached callee snapshots were not accounted");
        Require(original->linking != nullptr && original->linking->dependencies.size() == 2u, "cached direct linking did not capture both callees");
        std::vector<std::weak_ptr<const ShaderRecompiler::SrtGuestWords>> dependencies(original->linking->dependencies.begin(), original->linking->dependencies.end());
        const auto linked = original->linking->linked;
        DriverTestAccess::Evict();
        original.reset();
        const auto evicted = Driver::Get().DispatchCacheSize();
        Require(evicted.entries == 0u && evicted.variantBytes == 0u, "eviction did not remove dispatch-cache accounting");
        for (const auto& dependency : dependencies) Require(dependency.expired(), "evicted dependency snapshot survived outside the dispatch-cache budget");
        std::shared_ptr<const PreparedShaderState::UserDataLink> reused;
        InvocationFor(*shader, 0, request, &reused);
        Require(reused != nullptr && reused->linked == linked, "eviction discarded the reusable direct-call link");
        std::printf("dispatch-cache bytes: %llu -> %llu; released %zu dependency snapshots\n", static_cast<unsigned long long>(footprint.variantBytes), static_cast<unsigned long long>(evicted.variantBytes), dependencies.size());
        return;
    }
    if (mode == "row") Table[2] = second;
    else if (mode == "descriptor") {
        const auto address = reinterpret_cast<std::uintptr_t>(OtherTable.data());
        Descriptor[0] = static_cast<std::uint32_t>(address);
        Descriptor[1] = static_cast<std::uint32_t>(address >> 32u) | (16u << 16u);
    } else if (nested) {
        const auto address = reinterpret_cast<std::uintptr_t>(OtherTable.data());
        Outer[16] = static_cast<std::uint32_t>(address);
        Outer[17] = static_cast<std::uint32_t>(address >> 32u) | (16u << 16u);
    } else First[0] = 0x4a020281u;
    AgcDriver::GuestMemory::BumpCollectEpoch();
    Require(DriverTestAccess::Lookup(*shader, request) == nullptr, "changed linking input reused the stale prepared dispatch");
    const auto changed = DriverTestAccess::Insert(shader, request);
    Require(changed != nullptr && changed != original, "changed linking input did not retain a new dispatch variant");
    Require(changed->compiled->variantId != original->compiled->variantId, "changed linking input retained the old linked-code identity");
    Require(DriverTestAccess::Lookup(*shader, request) == changed, "the rebuilt link was not reusable");
    Require(changed->linking != nullptr && !changed->linking->dependencies.empty(), "the rebuilt variant lost its linking dependencies");
    const auto shared = changed->shared;
    for (const auto& dependency : changed->linking->dependencies) {
        changed->shared.clear();
        std::copy_if(shared.begin(), shared.end(), std::back_inserter(changed->shared), [&](const auto& entry) { return entry.block.lock() == dependency; });
        const auto sharedWords = std::accumulate(changed->shared.begin(), changed->shared.end(), std::size_t{0}, [](std::size_t total, const auto& entry) { return total + entry.count; });
        std::vector<bool> covered(dependency->words.size(), false);
        for (const auto& entry : changed->shared) {
            Require(entry.offset <= covered.size() && entry.count <= covered.size() - entry.offset, "a dependency proof extends outside its snapshot");
            std::fill(covered.begin() + entry.offset, covered.begin() + entry.offset + entry.count, true);
        }
        Require(std::ranges::all_of(covered, [](bool word) { return word; }), "a linking dependency has no complete shared proof entry");
        const auto arenaAddress = reinterpret_cast<std::uintptr_t>(arena.get());
        const bool tracked = watched && dependency->low >= arenaAddress && dependency->high <= arenaAddress + ArenaBytes;
        const auto vouched = AgcDriver::ShaderMemory::Vouches().words;
        Require(DriverTestAccess::Lookup(*shader, request) == changed, "isolating a dependency proof prevented dispatch reuse");
        const auto words = AgcDriver::ShaderMemory::Vouches().words - vouched;
        Require(words == (tracked ? sharedWords : 0u), "a linking dependency did not use exactly its own watched proof or untracked fallback");
        std::printf("dependency 0x%llx: %zu retained words, %zu shared words, %llu vouched, %s\n", static_cast<unsigned long long>(dependency->low), dependency->words.size(), sharedWords, static_cast<unsigned long long>(words), tracked ? "watched" : "untracked");
    }
    changed->shared = shared;
    First[0] = 0x4a020285u;
    Table[2] = 0u;
    Descriptor[0] = static_cast<std::uint32_t>(tableAddress);
    Descriptor[1] = static_cast<std::uint32_t>(tableAddress >> 32u) | (16u << 16u);
    std::copy(Descriptor.begin(), Descriptor.end(), Outer.begin() + 16u);
    AgcDriver::GuestMemory::BumpCollectEpoch();
    const auto restored = DriverTestAccess::Lookup(*shader, request);
    if (std::getenv("APS5_DISPATCH_STAMP_VALIDATE") == nullptr) Require(restored == original, "restored linking inputs did not reuse the earlier variant");
    else Require(restored == nullptr, "stamp validation reused the only retained variant after its linking inputs changed");
}

}

int main(int argc, char** argv) {
    try {
        Require(argc == 2 || argc == 3, "expected a linking dependency case");
        Run(argv[1], argc == 3 ? std::string_view(argv[2]) : std::string_view{});
        LibcRunShutdown_nid_postfix();
        std::puts("linked-code dependency tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); } catch (...) {}
        return 1;
    }
}
