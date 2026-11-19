#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ProfileOutput.hpp"
#include <spirv/unified1/spirv.hpp>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <optional>
#include <chrono>
#include <cstdlib>
#include <map>
#include <mutex>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace AgcDriver::Graphics {

namespace {

std::uint32_t pointerOperandStored(std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
    switch (static_cast<spv::Op>(opcode)) {
    case spv::OpStore:
    case spv::OpCopyMemory:
    case spv::OpCopyMemorySized:
    case spv::OpAtomicStore:
    case spv::OpAtomicFlagClear:
    case spv::OpCooperativeMatrixStoreKHR:
    case spv::OpCooperativeMatrixStoreNV:
    case spv::OpCooperativeMatrixStoreTensorNV:
        return instruction.size() > 1 ? instruction[1] : 0u;
    case spv::OpAtomicExchange:
    case spv::OpAtomicCompareExchange:
    case spv::OpAtomicCompareExchangeWeak:
    case spv::OpAtomicIIncrement:
    case spv::OpAtomicIDecrement:
    case spv::OpAtomicIAdd:
    case spv::OpAtomicISub:
    case spv::OpAtomicSMin:
    case spv::OpAtomicUMin:
    case spv::OpAtomicSMax:
    case spv::OpAtomicUMax:
    case spv::OpAtomicAnd:
    case spv::OpAtomicOr:
    case spv::OpAtomicXor:
    case spv::OpAtomicFlagTestAndSet:
    case spv::OpAtomicFMinEXT:
    case spv::OpAtomicFMaxEXT:
    case spv::OpAtomicFAddEXT:
        return instruction.size() > 3 ? instruction[3] : 0u;
    default:
        return 0u;
    }
}

bool definesNoPointerValue(std::uint32_t opcode) {
    switch (static_cast<spv::Op>(opcode)) {
    case spv::OpTypePointer:
    case spv::OpTypeUntypedPointerKHR:
    case spv::OpTypeForwardPointer:
    case spv::OpName:
    case spv::OpMemberName:
    case spv::OpDecorate:
    case spv::OpMemberDecorate:
    case spv::OpDecorateId:
    case spv::OpDecorateString:
    case spv::OpMemberDecorateString:
        return true;
    default:
        return false;
    }
}

bool referencesNoValue(std::uint32_t opcode) {
    switch (static_cast<spv::Op>(opcode)) {
    case spv::OpCapability:
    case spv::OpExtension:
    case spv::OpExtInstImport:
    case spv::OpMemoryModel:
    case spv::OpEntryPoint:
    case spv::OpExecutionMode:
    case spv::OpExecutionModeId:
    case spv::OpSource:
    case spv::OpSourceExtension:
    case spv::OpString:
    case spv::OpLine:
    case spv::OpNoLine:
    case spv::OpModuleProcessed:
    case spv::OpConstantTrue:
    case spv::OpConstantFalse:
    case spv::OpConstant:
    case spv::OpConstantComposite:
    case spv::OpConstantNull:
    case spv::OpSpecConstantTrue:
    case spv::OpSpecConstantFalse:
    case spv::OpSpecConstant:
    case spv::OpSpecConstantComposite:
    case spv::OpSpecConstantOp:
        return true;
    default:
        return definesNoPointerValue(opcode) || (opcode >= spv::OpTypeVoid && opcode <= spv::OpTypeForwardPointer);
    }
}

template<typename TVisit>
bool forEachInstruction(std::span<const std::uint32_t> words, const TVisit& visit) {
    for (std::size_t at = 5; at < words.size();) {
        const auto count = words[at] >> 16u;
        if (count == 0 || at + count > words.size()) return false;
        visit(words[at] & 0xffffu, words.subspan(at, count));
        at += count;
    }
    return true;
}

bool storesThroughAddressesCached(const ShaderRecompiler::SharedSpirv& spirv) {
    static std::mutex mutex;
    static std::map<std::pair<std::uint64_t, std::uint64_t>, bool> known;
    const auto hash = spirv.ContentHash();
    const std::pair key{hash.low, hash.high};
    {
        std::lock_guard lock(mutex);
        if (const auto found = known.find(key); found != known.end()) return found->second;
    }
    const bool stores = SpirvStoresThroughAddresses(spirv.Words());
    std::lock_guard lock(mutex);
    known.emplace(key, stores);
    return stores;
}

bool faultReportOnlyCached(const ShaderRecompiler::SharedSpirv& spirv, std::uint32_t set, std::uint32_t binding) {
    static std::mutex mutex;
    static std::map<std::tuple<std::uint64_t, std::uint64_t, std::uint32_t, std::uint32_t>, bool> known;
    const auto hash = spirv.ContentHash();
    const std::tuple key{hash.low, hash.high, set, binding};
    {
        std::lock_guard lock(mutex);
        if (const auto found = known.find(key); found != known.end()) return found->second;
    }
    const bool reportOnly = SpirvFaultReportOnly(spirv.Words(), set, binding);
    std::lock_guard lock(mutex);
    known.emplace(key, reportOnly);
    return reportOnly;
}

}

bool ShaderStoresThroughAddresses(const ShaderRecompiler::SharedSpirv& spirv) {
    return storesThroughAddressesCached(spirv);
}

bool SpirvStoresThroughAddresses(std::span<const std::uint32_t> words) {
    if (words.size() < 5 || words[0] != spv::MagicNumber) return true;
    std::unordered_set<std::uint32_t> pointerTypes;
    const bool readable = forEachInstruction(words, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        if ((opcode == spv::OpTypePointer || opcode == spv::OpTypeUntypedPointerKHR) && instruction.size() > 2 && instruction[2] == spv::StorageClassPhysicalStorageBuffer) pointerTypes.insert(instruction[1]);
    });
    if (!readable) return true;
    if (pointerTypes.empty()) return false;
    std::unordered_set<std::uint32_t> pointers;
    forEachInstruction(words, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        if (instruction.size() > 2 && !definesNoPointerValue(opcode) && pointerTypes.contains(instruction[1])) pointers.insert(instruction[2]);
    });
    bool stores = false;
    forEachInstruction(words, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        const auto pointer = pointerOperandStored(opcode, instruction);
        const bool extendedWithPointer = opcode == spv::OpExtInst && std::any_of(instruction.begin() + static_cast<std::ptrdiff_t>(std::min<std::size_t>(5u, instruction.size())), instruction.end(), [&](std::uint32_t operand) { return pointers.contains(operand); });
        stores = stores || extendedWithPointer || (pointer != 0 && pointers.contains(pointer));
    });
    return stores;
}

bool SpirvFaultReportOnly(std::span<const std::uint32_t> words, std::uint32_t set, std::uint32_t binding) {
    constexpr std::uint32_t reportWords = sizeof(ShaderRecompiler::BdaAbi::Fault) / sizeof(std::uint32_t);
    if (words.size() < 5 || words[0] != spv::MagicNumber) return false;
    std::unordered_map<std::uint32_t, std::uint32_t> sets;
    std::unordered_map<std::uint32_t, std::uint32_t> bindings;
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    std::unordered_map<std::uint32_t, std::pair<std::uint32_t, std::uint32_t>> selects;
    const bool readable = forEachInstruction(words, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        if (opcode == spv::OpDecorate && instruction.size() == 4 && instruction[2] == spv::DecorationDescriptorSet) sets[instruction[1]] = instruction[3];
        if (opcode == spv::OpDecorate && instruction.size() == 4 && instruction[2] == spv::DecorationBinding) bindings[instruction[1]] = instruction[3];
        if (opcode == spv::OpConstant && instruction.size() == 4) constants[instruction[2]] = instruction[3];
        if (opcode == spv::OpSelect && instruction.size() == 6) selects[instruction[2]] = {instruction[4], instruction[5]};
    });
    if (!readable) return false;
    std::unordered_set<std::uint32_t> buffers;
    for (const auto& [id, number] : bindings) {
        if (const auto found = sets.find(id); number == binding && found != sets.end() && found->second == set) buffers.insert(id);
    }
    const auto constantBelow = [&](std::uint32_t id, std::uint32_t limit) {
        const auto below = [&](std::uint32_t value) {
            const auto found = constants.find(value);
            return found != constants.end() && found->second < limit;
        };
        const auto selected = selects.find(id);
        return below(id) || (selected != selects.end() && below(selected->second.first) && below(selected->second.second));
    };
    const auto chainsBuffer = [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        return (opcode == spv::OpAccessChain || opcode == spv::OpInBoundsAccessChain) && instruction.size() > 3 && buffers.contains(instruction[3]);
    };
    bool reportOnly = !buffers.empty();
    std::unordered_set<std::uint32_t> reportPointers;
    forEachInstruction(words, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        if (!chainsBuffer(opcode, instruction)) return;
        reportOnly = reportOnly && instruction.size() == 6 && constantBelow(instruction[4], 1) && constantBelow(instruction[5], reportWords);
        reportPointers.insert(instruction[2]);
    });
    forEachInstruction(words, [&](std::uint32_t opcode, std::span<const std::uint32_t> instruction) {
        if (!reportOnly || referencesNoValue(opcode) || chainsBuffer(opcode, instruction) || (opcode == spv::OpVariable && instruction.size() > 2 && buffers.contains(instruction[2]))) return;
        const auto accessed = opcode == spv::OpLoad || opcode == spv::OpAtomicLoad ? (instruction.size() > 3 ? instruction[3] : 0u) : pointerOperandStored(opcode, instruction);
        const auto references = std::count_if(instruction.begin() + 1, instruction.end(), [&](std::uint32_t word) { return buffers.contains(word) || reportPointers.contains(word); });
        reportOnly = references == (reportPointers.contains(accessed) ? 1 : 0);
    });
    return reportOnly;
}

void ShaderResources::prepareAddressBindings(std::span<const CompiledShader> shaders, std::span<const GuestMemorySnapshot> snapshots) {
    static const bool loopGuarded = std::getenv("APS5_LOOP_GUARD") != nullptr;
    for (const auto& shader : shaders) {
        Require(shader.program != nullptr, "missing compiled shader");
        std::uint32_t tables = 0;
        std::uint32_t faults = 0;
        for (const auto& binding : shader.program->bindings) {
            if (binding.role != ShaderRecompiler::DescriptorRole::BdaPagetable && binding.role != ShaderRecompiler::DescriptorRole::FaultBuffer) continue;
            Require(binding.kind == ShaderRecompiler::DescriptorKind::StorageBuffer && binding.count == 1 && binding.guestDescriptor.empty() && !binding.readOnly, "invalid BDA descriptor contract");
            if (binding.role == ShaderRecompiler::DescriptorRole::BdaPagetable) {
                ++tables;
            } else {
                ++faults;
                faultReportOnly = faultReportOnly && faultReportOnlyCached(shader.program->spirv, binding.descriptorSet, binding.binding);
            }
        }
        const bool faultOnly = tables == 0 && faults == 1 && shader.program->bdaAbiVersion == ShaderRecompiler::BdaAbi::Version;
        Require((tables == faults || faultOnly) && tables <= 1 && faults <= 1, "invalid BDA table and fault descriptors");
        Require(shader.program->bdaAbiVersion == (faults == 0 ? 0u : ShaderRecompiler::BdaAbi::Version), "incompatible BDA ABI version");
        // Rect-list validation needs a fault buffer, but never accesses guest addresses.
        usesBda = usesBda || tables != 0;
        if (tables != 0 && storesThroughAddressesCached(shader.program->spirv)) {
            bdaReadOnly = false;
            if (shader.program->storeTables.fallback == ShaderRecompiler::StoreTableFallback::None) storeWindows.push_back(shader.program->storeTables);
            else if (!storeWindowMissed.has_value()) storeWindowMissed = shader.program->storeTables;
        }
        usesFaultBuffer = usesFaultBuffer || faults != 0;
        const bool tableFaultsOnly = std::any_of(shader.program->bindings.begin(), shader.program->bindings.end(), [](const ShaderRecompiler::DescriptorBinding& binding) { return binding.role == ShaderRecompiler::DescriptorRole::ImageTableMap; });
        faultChecks = faultChecks || tables != 0 || (faults != 0 && (!tableFaultsOnly || shader.program->imageTableFaults != 0 || shader.program->poisonedSrtReads != 0 || shader.program->windowLaneGuards != 0 || shader.program->descriptorFaults != 0 || loopGuarded));
    }
    if (usesBda) {
        Require(context.bufferDeviceAddress, "buffer device address is not enabled");
        static const bool profile = std::getenv("APS5_PROFILE_DRAW") != nullptr;
        guestMemory.AcquireRegistered();
        const auto snapshotsStart = profile ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point{};
        for (const auto& snapshot : snapshots) guestMemory.AddSnapshot(snapshot);
        if (profile) GuestBufferMemory::CountAddressBuild(std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - snapshotsStart).count());
    }
}

VkDescriptorBufferInfo ShaderResources::descriptor(Allocation& allocation) {
    if (allocation.guest) return wholeDwordRange(guestMemory.Descriptor(allocation.address, allocation.size, allocation.adjustment));
    if (allocation.role == ShaderRecompiler::DescriptorRole::GuestBuffers) {
        const auto placeholder = allocation.oversized ? context.oversizedBuffer : context.emptyBuffer;
        Require(placeholder != VK_NULL_HANDLE, "no placeholder buffer for a null V#");
        return {placeholder, 0, allocation.size};
    }
    if (allocation.role == ShaderRecompiler::DescriptorRole::BdaPagetable || allocation.role == ShaderRecompiler::DescriptorRole::FaultBuffer) {
        Require(bda != nullptr, "BDA descriptors have no memory owner");
        return allocation.role == ShaderRecompiler::DescriptorRole::BdaPagetable ? bda->Table() : bda->Fault();
    }
    Require(allocation.buffer != nullptr, "shader data has no buffer owner");
    return {allocation.buffer->Handle(), 0, allocation.size};
}

VkDescriptorBufferInfo ShaderResources::wholeDwordRange(VkDescriptorBufferInfo info) const {
    if (context.exactRobustLoads && info.range >= 4u) info.range &= ~VkDeviceSize{3};
    return info;
}

namespace {

enum class StoreWindowOutcome : std::size_t { Bounded, Unmatched, TooMany, NotBuffer, Unevaluated, Disabled, PendingTable, Unmapped, InvalidType, Swizzled, UnboundedIndex, UnboundedOffset, TooWide, SelfWritten, Count };

constexpr std::array<const char*, static_cast<std::size_t>(StoreWindowOutcome::Count)> StoreWindowOutcomeNames{"bounded", "unmatched", "too many", "not buffer", "unevaluated", "disabled", "pending table", "unmapped", "invalid type", "swizzled", "unbounded index", "unbounded offset", "too wide", "self-written"};

struct StoreWindowTotals {
    std::array<std::uint64_t, static_cast<std::size_t>(StoreWindowOutcome::Count)> outcomes{};
    std::uint64_t noted = 0;
    std::uint64_t whole = 0;
    std::uint64_t wantedKeys = 0;
    std::uint32_t missPc = 0;
    ShaderRecompiler::StoreTableMiss miss = ShaderRecompiler::StoreTableMiss::None;
    bool missed = false;
};

struct StoreWindowStats {
    std::mutex lock;
    std::map<std::uint64_t, StoreWindowTotals> programs;
    std::chrono::steady_clock::time_point last = std::chrono::steady_clock::now();
};

StoreWindowStats& storeWindowStats() {
    static auto* stats = new StoreWindowStats;
    return *stats;
}

bool storeWindowsProfiled() {
    static const bool profiled = std::getenv("APS5_PROFILE_DRAW") != nullptr;
    return profiled;
}

StoreWindowOutcome outcomeOf(ShaderRecompiler::StoreTableFallback fallback) {
    switch (fallback) {
        case ShaderRecompiler::StoreTableFallback::None: return StoreWindowOutcome::Bounded;
        case ShaderRecompiler::StoreTableFallback::TooMany: return StoreWindowOutcome::TooMany;
        case ShaderRecompiler::StoreTableFallback::NotBuffer: return StoreWindowOutcome::NotBuffer;
        case ShaderRecompiler::StoreTableFallback::Unevaluated: return StoreWindowOutcome::Unevaluated;
        case ShaderRecompiler::StoreTableFallback::Unmatched: break;
    }
    return StoreWindowOutcome::Unmatched;
}

std::uint64_t rangeBytes(std::span<const std::pair<std::uint64_t, std::uint64_t>> ranges) {
    std::uint64_t bytes = 0;
    for (const auto& [begin, end] : ranges) bytes += end > begin ? end - begin : 0u;
    return bytes;
}

void reportStoreWindows(std::uint64_t program, StoreWindowOutcome outcome, std::uint64_t noted, std::uint64_t whole, const ShaderRecompiler::StoreTablesSnapshot* missed) {
    auto& stats = storeWindowStats();
    const std::lock_guard guard(stats.lock);
    auto& totals = stats.programs[program];
    ++totals.outcomes[static_cast<std::size_t>(outcome)];
    totals.noted += noted;
    totals.whole += whole;
    if (missed != nullptr) {
        totals.missed = true;
        totals.miss = missed->miss;
        totals.missPc = missed->missPc;
        totals.wantedKeys = std::max(totals.wantedKeys, missed->wantedKeys);
    }
    const auto now = std::chrono::steady_clock::now();
    if (now - stats.last < std::chrono::seconds(10)) return;
    stats.last = now;
    std::vector<std::pair<std::uint64_t, StoreWindowTotals>> rows(stats.programs.begin(), stats.programs.end());
    stats.programs.clear();
    std::ranges::sort(rows, [](const auto& left, const auto& right) { return left.second.whole > right.second.whole; });
    std::string text;
    for (std::size_t row = 0; row < rows.size() && row < 16u; ++row) {
        const auto& [address, entry] = rows[row];
        char head[32];
        std::snprintf(head, sizeof(head), " [0x%llx", static_cast<unsigned long long>(address));
        text += head;
        for (std::size_t kind = 0; kind < entry.outcomes.size(); ++kind) {
            if (entry.outcomes[kind] == 0u) continue;
            text += " " + std::string(StoreWindowOutcomeNames[kind]) + " " + std::to_string(entry.outcomes[kind]);
        }
        if (entry.missed) {
            char miss[128];
            if (entry.wantedKeys != 0u) std::snprintf(miss, sizeof(miss), " (pc 0x%x, %llu keys)", entry.missPc, static_cast<unsigned long long>(entry.wantedKeys));
            else if (entry.miss == ShaderRecompiler::StoreTableMiss::None) std::snprintf(miss, sizeof(miss), " (pc 0x%x)", entry.missPc);
            else std::snprintf(miss, sizeof(miss), " (pc 0x%x %s)", entry.missPc, ShaderRecompiler::StoreTableMissName(entry.miss));
            text += miss;
        }
        char bytes[96];
        std::snprintf(bytes, sizeof(bytes), "; noted %.1f of %.1f MiB]", static_cast<double>(entry.noted) / 1048576.0, static_cast<double>(entry.whole) / 1048576.0);
        text += bytes;
    }
    AgcDriver::ProfilePrint_nid_no_patch("[store-windows] (10 s) reach notes by program (outcome uses; reach noted of whole MiB):%s\n", text.c_str());
}

std::optional<std::uint64_t> addBounded(std::uint64_t left, std::uint64_t right) {
    if (left > std::numeric_limits<std::uint64_t>::max() - right) return std::nullopt;
    return left + right;
}

std::optional<std::uint64_t> mulBounded(std::uint64_t left, std::uint64_t right) {
    if (left != 0u && right > std::numeric_limits<std::uint64_t>::max() / left) return std::nullopt;
    return left * right;
}

constexpr std::uint64_t MaxStoreExtent = std::uint64_t{1} << 40u;

std::optional<std::pair<std::uint64_t, std::uint64_t>> storeExtent(const std::array<std::uint32_t, 4>& words, const ShaderRecompiler::StoreTableSnapshot& table, std::optional<StoreWindowOutcome>& unbounded) {
    unbounded.reset();
    const std::uint64_t base = (words[0] | (static_cast<std::uint64_t>(words[1]) << 32u)) & 0xffffffffffffull;
    const std::uint32_t stride = (words[1] >> 16u) & 0x3fffu;
    const std::uint32_t records = words[2];
    const bool swizzle = ((words[1] >> 31u) & 1u) != 0u && stride != 0u;
    const bool addTid = ((words[3] >> 23u) & 1u) != 0u;
    const std::uint32_t type = (words[3] >> 30u) & 3u;
    const std::uint32_t mode = (words[3] >> 28u) & 3u;
    if (base == 0u || records == 0u) return std::nullopt;
    if (type != 0u) {
        if (!table.direct && !table.scalar) return std::nullopt;
        unbounded = StoreWindowOutcome::InvalidType;
        return std::nullopt;
    }
    const auto known = [](std::uint32_t value) { return value == ShaderRecompiler::StoreTableUnbounded ? std::nullopt : std::optional<std::uint64_t>(value); };
    const auto index = known(table.maxIndex).has_value() ? std::optional<std::uint64_t>(*known(table.maxIndex) + (addTid ? 64u : 0u)) : std::nullopt;
    const auto offset = known(table.maxOffset);
    const auto soffset = known(table.maxSoffset);
    std::optional<std::uint64_t> end;
    if (swizzle) {
        const auto indexShift = ((words[3] >> 21u) & 3u) + 3u;
        const auto lastIndex = mode == 2u ? index : std::optional<std::uint64_t>(records - 1u);
        const auto lastOffset = mode == 0u || mode == 3u ? std::optional<std::uint64_t>(stride - 1u) : offset;
        if (lastIndex.has_value() && lastOffset.has_value() && soffset.has_value()) {
            const auto rows = mulBounded(*lastIndex >> indexShift, stride);
            const auto column = rows.has_value() ? addBounded(*rows, *lastOffset) : std::nullopt;
            const auto scaled = column.has_value() ? mulBounded(*column, std::uint64_t{1} << indexShift) : std::nullopt;
            end = scaled.has_value() ? addBounded(*scaled, (std::uint64_t{4} << indexShift) + 16u + *soffset) : std::nullopt;
        }
        if (!end.has_value()) {
            unbounded = StoreWindowOutcome::Swizzled;
            return std::nullopt;
        }
    } else if (mode == 0u) {
        if (stride == 0u) return std::nullopt;
        end = soffset.has_value() ? addBounded(static_cast<std::uint64_t>(records) * stride, 16u + *soffset) : std::nullopt;
    } else if (mode == 1u) {
        end = offset.has_value() && soffset.has_value() ? addBounded(static_cast<std::uint64_t>(records - 1u) * stride, *offset + *soffset) : std::nullopt;
    } else if (mode == 2u) {
        const auto scaled = stride == 0u ? std::optional<std::uint64_t>(0u) : index.has_value() ? mulBounded(*index, stride) : std::nullopt;
        end = scaled.has_value() && offset.has_value() && soffset.has_value() ? addBounded(*scaled, *offset + *soffset) : std::nullopt;
    } else {
        const auto scaled = stride == 0u ? std::optional<std::uint64_t>(0u) : index.has_value() ? mulBounded(*index, stride) : std::nullopt;
        end = scaled.has_value() ? addBounded(*scaled, records) : std::nullopt;
    }
    if (!end.has_value()) {
        unbounded = (mode == 2u || mode == 3u) && stride != 0u && !index.has_value() ? StoreWindowOutcome::UnboundedIndex : StoreWindowOutcome::UnboundedOffset;
        return std::nullopt;
    }
    if (*end > MaxStoreExtent) {
        unbounded = StoreWindowOutcome::TooWide;
        return std::nullopt;
    }
    return std::pair{base, base + *end};
}

}

void ShaderResources::narrowToStoreWindows(Recorder& recorder, std::vector<std::pair<std::uint64_t, std::uint64_t>>& reachable) const {
    static const bool disabled = std::getenv("APS5_NO_STORE_WINDOWS") != nullptr;
    const bool profiled = storeWindowsProfiled();
    const auto program = storeWindowMissed.has_value() ? storeWindowMissed->shader : storeWindows.empty() ? std::uint64_t{0} : storeWindows.front().shader;
    const auto whole = profiled ? rangeBytes(reachable) : 0u;
    const auto keep = [&](StoreWindowOutcome outcome) {
        if (profiled) reportStoreWindows(program, outcome, whole, whole, storeWindowMissed.has_value() ? &*storeWindowMissed : nullptr);
    };
    if (disabled) return keep(StoreWindowOutcome::Disabled);
    if (storeWindowMissed.has_value()) return keep(outcomeOf(storeWindowMissed->fallback));
    if (storeWindows.empty()) return keep(StoreWindowOutcome::Unmatched);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> windows;
    std::vector<std::pair<std::uint64_t, std::uint64_t>> tableRanges;
    std::vector<std::byte> bytes;
    for (const auto& shader : storeWindows) {
        for (const auto& table : shader.tables) {
            std::optional<StoreWindowOutcome> unbounded;
            if (table.direct) {
                if (const auto extent = storeExtent(table.descriptor, table, unbounded)) windows.push_back(*extent);
                if (unbounded.has_value()) return keep(*unbounded);
                continue;
            }
            if (table.keys == 0u) continue;
            const auto first = table.base + table.offset;
            const auto span = static_cast<std::uint64_t>(table.keys - 1u) * table.stride + 4u * sizeof(std::uint32_t);
            if (first < table.base || first + span < first) return keep(StoreWindowOutcome::TooWide);
            if (recorder.PendingWriteOverlaps(first, static_cast<std::size_t>(span))) return keep(StoreWindowOutcome::PendingTable);
            bytes.resize(static_cast<std::size_t>(span));
            const auto readable = table.address ? span : table.size > table.offset ? std::min<std::uint64_t>(span, table.size - table.offset) : 0u;
            std::fill(bytes.begin(), bytes.end(), std::byte{0});
            if (readable != 0u && GuestMemory::CopyMapped(first, std::span<std::byte>(bytes.data(), static_cast<std::size_t>(readable))) != GuestMemory::Compare::Equal) return keep(StoreWindowOutcome::Unmapped);
            tableRanges.emplace_back(first, first + readable);
            const auto record = [&](std::uint32_t key) {
                std::array<std::uint32_t, 4> words{};
                std::memcpy(words.data(), bytes.data() + static_cast<std::size_t>(key) * table.stride, sizeof(words));
                if (const auto extent = storeExtent(words, table, unbounded)) windows.push_back(*extent);
                return !unbounded.has_value();
            };
            if (table.keyList.empty()) {
                for (std::uint32_t key = 0; key < table.keys; ++key) {
                    if (!record(key)) return keep(*unbounded);
                }
            } else {
                for (const auto key : table.keyList) {
                    if (!record(key)) return keep(*unbounded);
                }
            }
        }
    }
    const auto overlaps = [](const auto& left, const auto& right) { return left.first < right.second && right.first < left.second; };
    for (const auto& window : windows) {
        if (std::ranges::any_of(tableRanges, [&](const auto& table) { return overlaps(window, table); })) return keep(StoreWindowOutcome::SelfWritten);
    }
    std::ranges::sort(windows);
    std::vector<std::pair<std::uint64_t, std::uint64_t>> narrowed;
    for (const auto& region : reachable) {
        for (const auto& window : windows) {
            const auto begin = std::max(region.first, window.first);
            const auto end = std::min(region.second, window.second);
            if (begin < end) narrowed.emplace_back(begin, end);
        }
    }
    reachable = std::move(narrowed);
    if (profiled) reportStoreWindows(program, StoreWindowOutcome::Bounded, rangeBytes(reachable), whole, nullptr);
}

}
