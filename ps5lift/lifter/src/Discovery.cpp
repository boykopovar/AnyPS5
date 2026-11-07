#include <lifter/Discovery.hpp>
#include <lifter/Hex.hpp>

#include <llvm/Support/Base64.h>
#include <llvm/Support/SHA1.h>

#include <algorithm>
#include <array>
#include <deque>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace Ps5Lift {

namespace {

class UndecodableCode : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

bool EndsFlow(const Instruction& instruction) {
    switch (instruction.Info.mnemonic) {
    case ZYDIS_MNEMONIC_RET:
    case ZYDIS_MNEMONIC_JMP:
    case ZYDIS_MNEMONIC_UD2:
    case ZYDIS_MNEMONIC_HLT:
    case ZYDIS_MNEMONIC_INT3:
        return true;
    default:
        return false;
    }
}

using Predecessors = std::map<std::uint64_t, std::set<std::uint64_t>>;

Predecessors PredecessorsOf(const DecodedFunction& function) {
    Predecessors predecessors;
    for (const auto& [address, instruction] : function.Instructions) {
        const auto mnemonic = instruction.Info.mnemonic;
        if (!EndsFlow(instruction)) predecessors[instruction.Next()].insert(address);
        if (const auto target = DirectTarget(instruction); target && (mnemonic == ZYDIS_MNEMONIC_JMP || IsConditionalJump(mnemonic))) predecessors[*target].insert(address);
    }
    for (const auto& [jump, targets] : function.JumpTables) {
        for (const auto target : targets) predecessors[target].insert(jump);
    }
    return predecessors;
}

const Instruction* UniquePredecessor(const DecodedFunction& function, const Predecessors& predecessors, const std::uint64_t address) {
    const auto found = predecessors.find(address);
    if (address == function.Entry || found == predecessors.end() || found->second.size() != 1) return nullptr;
    return &function.Instructions.at(*found->second.begin());
}

ZydisRegister Enclosing(const ZydisRegister reg) {
    return ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg);
}

bool ClobberedByCall(const ZydisRegister reg) {
    switch (Enclosing(reg)) {
    case ZYDIS_REGISTER_RBX:
    case ZYDIS_REGISTER_RBP:
    case ZYDIS_REGISTER_RSP:
    case ZYDIS_REGISTER_R12:
    case ZYDIS_REGISTER_R13:
    case ZYDIS_REGISTER_R14:
    case ZYDIS_REGISTER_R15:
        return false;
    default:
        return true;
    }
}

bool WritesRegister(const Instruction& instruction, const ZydisRegister reg) {
    for (std::uint8_t i = 0; i < instruction.Info.operand_count; ++i) {
        const auto& operand = instruction.Operands[i];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER && (operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) && Enclosing(operand.reg.value) == Enclosing(reg)) return true;
    }
    return false;
}

std::optional<std::uint64_t> BranchBound(const DecodedFunction& function, const Instruction& compare) {
    const auto branch = function.Instructions.find(compare.Next());
    if (branch == function.Instructions.end()) return std::nullopt;
    const auto width = compare.Operands[0].size;
    const auto value = width >= 64 ? compare.Operands[1].imm.value.u : compare.Operands[1].imm.value.u & ((std::uint64_t{1} << width) - 1);
    if (branch->second.Info.mnemonic == ZYDIS_MNEMONIC_JNBE) return value + 1;
    if (branch->second.Info.mnemonic == ZYDIS_MNEMONIC_JNB) return value;
    return std::nullopt;
}

bool CopiesRegister(const Instruction& instruction, const ZydisRegister to, const ZydisRegister from) {
    const auto& left = instruction.Operands[0];
    const auto& right = instruction.Operands[1];
    return instruction.Info.mnemonic == ZYDIS_MNEMONIC_MOV && left.type == ZYDIS_OPERAND_TYPE_REGISTER && left.size >= 32 && right.type == ZYDIS_OPERAND_TYPE_REGISTER &&
           Enclosing(left.reg.value) == Enclosing(to) && Enclosing(right.reg.value) == Enclosing(from);
}

std::optional<ZydisDecodedOperand> ZeroExtendedFrom(const Instruction& instruction, const ZydisRegister to) {
    const auto& left = instruction.Operands[0];
    const auto& right = instruction.Operands[1];
    if (instruction.Info.mnemonic != ZYDIS_MNEMONIC_MOVZX || left.type != ZYDIS_OPERAND_TYPE_REGISTER || left.size < 32 || Enclosing(left.reg.value) != Enclosing(to) ||
        right.type != ZYDIS_OPERAND_TYPE_REGISTER)
        return std::nullopt;
    return right;
}

std::optional<std::vector<const Instruction*>> ReachingDefinitions(const DecodedFunction& function, const Predecessors& predecessors, const std::uint64_t address,
                                                                   const ZydisRegister reg) {
    std::vector<const Instruction*> definitions;
    std::set<std::uint64_t> visited{address};
    std::vector<std::uint64_t> pending{address};
    while (!pending.empty()) {
        const auto current = pending.back();
        pending.pop_back();
        const auto found = predecessors.find(current);
        if (current == function.Entry || found == predecessors.end()) return std::nullopt;
        for (const auto predecessor : found->second) {
            if (!visited.insert(predecessor).second) continue;
            const auto& instruction = function.Instructions.at(predecessor);
            if (WritesRegister(instruction, reg)) {
                definitions.push_back(&instruction);
            } else if (instruction.Info.mnemonic == ZYDIS_MNEMONIC_CALL && ClobberedByCall(reg)) {
                return std::nullopt;
            } else {
                pending.push_back(predecessor);
            }
        }
    }
    return definitions;
}

const Instruction* SingleDefinition(const DecodedFunction& function, const Predecessors& predecessors, const std::uint64_t address, const ZydisRegister reg) {
    const auto definitions = ReachingDefinitions(function, predecessors, address, reg);
    return definitions && definitions->size() == 1 ? definitions->front() : nullptr;
}

std::optional<std::uint64_t> RipRelativeAddress(const Instruction& instruction) {
    const auto& source = instruction.Operands[1];
    if (instruction.Info.mnemonic != ZYDIS_MNEMONIC_LEA || source.mem.base != ZYDIS_REGISTER_RIP || source.mem.index != ZYDIS_REGISTER_NONE) return std::nullopt;
    return instruction.Next() + static_cast<std::uint64_t>(source.mem.disp.value);
}

std::optional<ZydisRegister> CopiedRegister(const Instruction& instruction) {
    const auto& left = instruction.Operands[0];
    const auto& right = instruction.Operands[1];
    if (instruction.Info.mnemonic != ZYDIS_MNEMONIC_MOV || left.type != ZYDIS_OPERAND_TYPE_REGISTER || right.type != ZYDIS_OPERAND_TYPE_REGISTER || left.size != 64) return std::nullopt;
    return right.reg.value;
}

std::optional<std::uint64_t> RipRelativeValue(const DecodedFunction& function, const Predecessors& predecessors, const std::uint64_t address, const ZydisRegister reg) {
    std::optional<std::uint64_t> value;
    std::set<std::pair<std::uint64_t, ZydisRegister>> visited;
    std::vector<std::pair<std::uint64_t, ZydisRegister>> pending{{address, reg}};
    while (!pending.empty()) {
        const auto [at, current] = pending.back();
        pending.pop_back();
        if (!visited.emplace(at, current).second) continue;
        const auto definitions = ReachingDefinitions(function, predecessors, at, current);
        if (!definitions || definitions->empty()) return std::nullopt;
        for (const auto* definition : *definitions) {
            if (const auto loaded = RipRelativeAddress(*definition)) {
                if (value && *value != *loaded) return std::nullopt;
                value = loaded;
            } else if (const auto source = CopiedRegister(*definition)) {
                pending.emplace_back(definition->Address, *source);
            } else {
                return std::nullopt;
            }
        }
    }
    return value;
}

std::optional<std::uint64_t> TableBound(const DecodedFunction& function, const Predecessors& predecessors, const std::uint64_t jump, ZydisRegister index) {
    struct Compared {
        ZydisRegister Register;
        std::uint64_t Bound;
    };
    std::optional<Compared> compared;
    std::uint16_t narrowest = 0;
    const auto* instruction = UniquePredecessor(function, predecessors, jump);
    for (int depth = 0; instruction != nullptr && depth < 12; ++depth, instruction = UniquePredecessor(function, predecessors, instruction->Address)) {
        const auto mnemonic = instruction->Info.mnemonic;
        const auto& left = instruction->Operands[0];
        if (mnemonic == ZYDIS_MNEMONIC_CMP || mnemonic == ZYDIS_MNEMONIC_SUB) {
            if (left.type != ZYDIS_OPERAND_TYPE_REGISTER || instruction->Operands[1].type != ZYDIS_OPERAND_TYPE_IMMEDIATE) return std::nullopt;
            const auto bound = BranchBound(function, *instruction);
            if (!bound) return std::nullopt;
            if (Enclosing(left.reg.value) == Enclosing(index)) return left.size >= narrowest ? bound : std::nullopt;
            if (mnemonic == ZYDIS_MNEMONIC_SUB || compared) return std::nullopt;
            compared = Compared{left.reg.value, *bound};
            continue;
        }
        if (compared && WritesRegister(*instruction, compared->Register)) return CopiesRegister(*instruction, compared->Register, index) ? std::optional(compared->Bound) : std::nullopt;
        if (!WritesRegister(*instruction, index)) continue;
        if (compared && CopiesRegister(*instruction, index, compared->Register)) return compared->Bound;
        if (const auto source = ZeroExtendedFrom(*instruction, index); source && !compared) {
            index = source->reg.value;
            narrowest = source->size;
            continue;
        }
        if (instruction->Operands[1].type != ZYDIS_OPERAND_TYPE_REGISTER || !CopiesRegister(*instruction, index, instruction->Operands[1].reg.value)) return std::nullopt;
        index = instruction->Operands[1].reg.value;
    }
    return std::nullopt;
}

constexpr std::uint64_t TableEntryBytes = 4;

struct TableShape {
    std::uint64_t Address;
    ZydisRegister Index;
    std::uint64_t BoundFrom;
};

std::optional<TableShape> MatchTable(const DecodedFunction& function, const Predecessors& predecessors, const Instruction& jump) {
    const auto& target = jump.Operands[0];
    if (target.type != ZYDIS_OPERAND_TYPE_REGISTER) return std::nullopt;
    const auto* previous = SingleDefinition(function, predecessors, jump.Address, target.reg.value);
    if (previous == nullptr) return std::nullopt;
    if (previous->Info.mnemonic != ZYDIS_MNEMONIC_ADD || previous->Operands[1].type != ZYDIS_OPERAND_TYPE_REGISTER) return std::nullopt;
    const auto sum = target.reg.value;
    const auto addend = previous->Operands[1].reg.value;
    for (const auto& [loaded, base] : {std::pair{sum, addend}, std::pair{addend, sum}}) {
        const auto* load = SingleDefinition(function, predecessors, previous->Address, loaded);
        if (load == nullptr || load->Info.mnemonic != ZYDIS_MNEMONIC_MOVSXD) continue;
        const auto& entry = load->Operands[1];
        if (entry.type != ZYDIS_OPERAND_TYPE_MEMORY || entry.mem.base == ZYDIS_REGISTER_NONE || entry.mem.scale != 4 || entry.mem.disp.value != 0) continue;
        const auto table = RipRelativeValue(function, predecessors, load->Address, entry.mem.base);
        if (table && RipRelativeValue(function, predecessors, previous->Address, base) == table) return TableShape{*table, entry.mem.index, load->Address};
    }
    return std::nullopt;
}

std::optional<std::uint64_t> MemoryReference(const Instruction& instruction, const ZydisDecodedOperand& operand) {
    if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY || operand.mem.segment == ZYDIS_REGISTER_FS || operand.mem.segment == ZYDIS_REGISTER_GS) return std::nullopt;
    if (operand.mem.base == ZYDIS_REGISTER_RIP) return instruction.Next() + static_cast<std::uint64_t>(operand.mem.disp.value);
    return std::nullopt;
}

std::set<std::uint64_t> DataReferences(const GuestImage& image, const Decoder& decoder) {
    std::set<std::uint64_t> references;
    for (const auto& segment : image.Segments()) {
        if (!segment.IsExecutable()) continue;
        const std::span<const std::uint8_t> bytes(segment.Bytes);
        for (std::uint64_t offset = 0; offset < bytes.size();) {
            const auto instruction = decoder.TryDecode(bytes.subspan(offset), segment.Address + offset);
            if (!instruction) {
                ++offset;
                continue;
            }
            for (std::uint8_t i = 0; i < instruction->Info.operand_count_visible; ++i) {
                const auto reference = MemoryReference(*instruction, instruction->Operands[i]);
                if (reference && !image.IsExecutable(*reference) && image.ReadData(*reference, 1)) references.insert(*reference);
            }
            offset += instruction->Info.length;
        }
    }
    return references;
}

struct RecoveredTable {
    std::vector<std::uint64_t> Targets;
    bool Guarded = false;
    bool Closed = false;
};

RecoveredTable RecoverJumpTable(const GuestImage& image, const DecodedFunction& function, const Predecessors& predecessors, const Instruction& jump,
                                const std::set<std::uint64_t>& starts, const std::set<std::uint64_t>& references) {
    constexpr std::uint64_t MaximumEntries = 4096;
    const auto shape = MatchTable(function, predecessors, jump);
    if (!shape) return {};
    const auto bound = TableBound(function, predecessors, shape->BoundFrom, shape->Index);
    if (bound && *bound > MaximumEntries) return {};
    auto count = bound.value_or(MaximumEntries);
    if (const auto next = references.upper_bound(shape->Address); !bound && next != references.end()) count = std::min(count, (*next - shape->Address) / TableEntryBytes);
    const auto after = starts.upper_bound(jump.Address);
    const auto lower = after == starts.begin() ? std::uint64_t{0} : *std::prev(after);
    const auto upper = after == starts.end() ? ~std::uint64_t{0} : *after;
    RecoveredTable table{{}, bound.has_value()};
    auto& targets = table.Targets;
    for (std::uint64_t i = 0; i < count; ++i) {
        const auto raw = image.ReadData(shape->Address + TableEntryBytes * i, TableEntryBytes);
        if (!raw) break;
        const auto destination = shape->Address + static_cast<std::uint64_t>(static_cast<std::int64_t>(static_cast<std::int32_t>(*raw)));
        const bool inside = destination >= lower && destination < upper && image.IsExecutable(destination);
        if (!inside) {
            if (bound) return {};
            break;
        }
        targets.push_back(destination);
    }
    const auto bytes = TableEntryBytes * count;
    table.Closed = bound && targets.size() == count && image.IsReadOnly(shape->Address, bytes) && !image.Relocates(shape->Address, bytes);
    return table;
}

std::optional<std::string> CalledImport(const GuestImage& image, const Decoder& decoder, const Instruction& instruction) {
    auto import = ImportTarget(image, instruction);
    if (const auto target = DirectTarget(instruction); !import && target && !image.CodeAt(*target).empty()) {
        const auto stub = decoder.Decode(image, *target);
        if (stub.Info.mnemonic == ZYDIS_MNEMONIC_JMP) import = ImportTarget(image, stub);
    }
    if (!import) return std::nullopt;
    const auto name = image.Imports().find(*import);
    if (name == image.Imports().end()) return std::nullopt;
    return name->second;
}

bool PassesArgument(const ZydisRegister reg) {
    switch (Enclosing(reg)) {
    case ZYDIS_REGISTER_RDI:
    case ZYDIS_REGISTER_RSI:
    case ZYDIS_REGISTER_RDX:
    case ZYDIS_REGISTER_RCX:
    case ZYDIS_REGISTER_R8:
    case ZYDIS_REGISTER_R9:
        return true;
    default:
        return false;
    }
}

bool IsVectorRegister(const ZydisRegister reg) {
    const auto type = ZydisRegisterGetClass(reg);
    return type == ZYDIS_REGCLASS_XMM || type == ZYDIS_REGCLASS_YMM || type == ZYDIS_REGCLASS_ZMM;
}

bool Overwrites(const Instruction& instruction, const ZydisRegister reg) {
    for (std::uint8_t i = 0; i < instruction.Info.operand_count; ++i) {
        const auto& operand = instruction.Operands[i];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER && (operand.actions & ZYDIS_OPERAND_ACTION_WRITE) && Enclosing(operand.reg.value) == Enclosing(reg)) return true;
    }
    return false;
}

std::optional<std::uint64_t> TakenFunctionAddress(const GuestImage& image, const DecodedFunction& function, const Instruction& lea) {
    const auto address = RipRelativeAddress(lea);
    if (!address || !image.IsExecutable(*address)) return std::nullopt;
    using Holders = std::set<ZydisRegister>;
    std::set<std::pair<std::uint64_t, Holders>> visited;
    std::vector<std::pair<std::uint64_t, Holders>> pending{{lea.Next(), {Enclosing(lea.Operands[0].reg.value)}}};
    bool used = false;
    while (!pending.empty()) {
        auto path = std::move(pending.back());
        pending.pop_back();
        auto& holders = path.second;
        const auto holds = [&](const ZydisDecodedOperand& operand) { return operand.type == ZYDIS_OPERAND_TYPE_REGISTER && holders.contains(Enclosing(operand.reg.value)); };
        const auto passed = [&] { return std::ranges::any_of(holders, PassesArgument); };
        auto next = function.Instructions.find(path.first);
        while (next != function.Instructions.end() && visited.emplace(next->first, holders).second) {
            const auto& instruction = next->second;
            const auto mnemonic = instruction.Info.mnemonic;
            next = function.Instructions.find(instruction.Next());
            if (mnemonic == ZYDIS_MNEMONIC_NOP) continue;
            for (std::uint8_t i = 0; i < instruction.Info.operand_count_visible; ++i) {
                const auto& operand = instruction.Operands[i];
                if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY && (holders.contains(Enclosing(operand.mem.base)) || holders.contains(Enclosing(operand.mem.index)))) return std::nullopt;
            }
            const auto& target = instruction.Operands[0];
            const auto& source = instruction.Operands[1];
            bool readsHolder = false;
            for (std::uint8_t i = 1; i < instruction.Info.operand_count_visible; ++i) readsHolder = readsHolder || holds(instruction.Operands[i]);
            const bool stored = target.type == ZYDIS_OPERAND_TYPE_MEMORY && (target.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) && readsHolder;
            if (((mnemonic == ZYDIS_MNEMONIC_CALL || mnemonic == ZYDIS_MNEMONIC_JMP || mnemonic == ZYDIS_MNEMONIC_PUSH) && holds(target)) || stored) {
                used = true;
                break;
            }
            if (mnemonic == ZYDIS_MNEMONIC_CALL) {
                if (passed()) {
                    used = true;
                    break;
                }
                std::erase_if(holders, ClobberedByCall);
                if (holders.empty()) break;
                continue;
            }
            if (mnemonic == ZYDIS_MNEMONIC_RET) {
                used = used || holders.contains(ZYDIS_REGISTER_RAX);
                break;
            }
            if (mnemonic == ZYDIS_MNEMONIC_JMP) {
                const auto destination = DirectTarget(instruction);
                if (destination && function.Instructions.contains(*destination)) {
                    next = function.Instructions.find(*destination);
                    continue;
                }
                used = used || (destination && passed());
                break;
            }
            if (EndsFlow(instruction)) break;
            if (IsConditionalJump(mnemonic)) {
                if (const auto destination = DirectTarget(instruction); destination && function.Instructions.contains(*destination)) pending.emplace_back(*destination, holders);
                continue;
            }
            const bool copies = mnemonic == ZYDIS_MNEMONIC_MOV || instruction.Info.meta.category == ZYDIS_CATEGORY_CMOV;
            if (copies && target.type == ZYDIS_OPERAND_TYPE_REGISTER && target.size == 64 && holds(source)) {
                holders.insert(Enclosing(target.reg.value));
                continue;
            }
            if (target.type == ZYDIS_OPERAND_TYPE_REGISTER && IsVectorRegister(target.reg.value) && readsHolder) {
                holders.insert(Enclosing(target.reg.value));
                continue;
            }
            std::erase_if(holders, [&](const ZydisRegister reg) { return Overwrites(instruction, reg); });
            if (holders.empty()) break;
        }
    }
    return used ? address : std::nullopt;
}

Instruction DecodeReached(const GuestImage& image, const Decoder& decoder, const std::uint64_t entry, const std::uint64_t address, const std::uint64_t start, const std::uint64_t from) {
    try {
        return decoder.Decode(image, address);
    } catch (const UndecodableCode& error) {
        if (start == entry) throw UndecodableCode(std::string(error.what()) + ", in code from the start of the function at 0x" + Hex(entry));
        throw UndecodableCode(std::string(error.what()) + ", in code from 0x" + Hex(start) + ", where 0x" + Hex(from) + " in the function at 0x" + Hex(entry) + " leads");
    }
}

bool UsesWholeState(const GuestImage& image, const DecodedFunction& function, const Instruction& instruction) {
    switch (instruction.Info.mnemonic) {
    case ZYDIS_MNEMONIC_CALL:
        return !DirectTarget(instruction) && !ImportTarget(image, instruction);
    case ZYDIS_MNEMONIC_JMP:
        return !DirectTarget(instruction) && !ImportTarget(image, instruction) && !function.ClosedJumpTables.contains(instruction.Address);
    case ZYDIS_MNEMONIC_INT:
    case ZYDIS_MNEMONIC_INT1:
    case ZYDIS_MNEMONIC_INTO:
    case ZYDIS_MNEMONIC_SYSCALL:
    case ZYDIS_MNEMONIC_FXSAVE:
    case ZYDIS_MNEMONIC_FXSAVE64:
    case ZYDIS_MNEMONIC_FXRSTOR:
    case ZYDIS_MNEMONIC_FXRSTOR64:
        return true;
    default:
        return false;
    }
}

RegisterEffects LocalEffects(const GuestImage& image, const DecodedFunction& function) {
    RegisterEffects effects{RegisterSet{1} << (ZYDIS_REGISTER_RSP - ZYDIS_REGISTER_RAX), 0};
    for (const auto& [address, instruction] : function.Instructions) {
        if (UsesWholeState(image, function, instruction)) return {EveryRegister, EveryRegister};
        const auto own = InstructionEffects(instruction);
        effects.Named |= own.Named;
        effects.Written |= own.Written;
    }
    return effects;
}

std::vector<std::uint64_t> Exits(const GuestImage& image, const DecodedFunction& function) {
    std::set<std::uint64_t> exits;
    const auto leave = [&](const std::uint64_t address) {
        if (!function.Instructions.contains(address)) exits.insert(address);
    };
    for (const auto& [address, instruction] : function.Instructions) {
        const auto mnemonic = instruction.Info.mnemonic;
        if (const auto target = DirectTarget(instruction)) {
            if (mnemonic == ZYDIS_MNEMONIC_CALL) exits.insert(*target);
            else leave(*target);
        } else if (const auto import = ImportTarget(image, instruction); import && (mnemonic == ZYDIS_MNEMONIC_CALL || mnemonic == ZYDIS_MNEMONIC_JMP)) {
            exits.insert(*import);
        }
        if (!EndsFlow(instruction)) leave(instruction.Next());
    }
    for (const auto& [jump, targets] : function.JumpTables) {
        for (const auto target : targets) leave(target);
    }
    return {exits.begin(), exits.end()};
}

namespace {

std::set<std::string> NamesAndNids(const std::initializer_list<const char*> names) {
    std::set<std::string> result;
    for (const std::string name : names) {
        result.insert(name);
        result.insert(Nid(name));
    }
    return result;
}

bool ReturnsTwice(const std::string& import) {
    static const auto Names = NamesAndNids({"setjmp", "_setjmp", "sigsetjmp", "__sigsetjmp"});
    const auto function = JumpBufferFunction(import);
    return function && Names.contains(*function);
}

}

void CloseEffects(std::map<std::uint64_t, GuestFunction>& functions) {
    std::vector<GuestFunction*> nodes;
    std::map<std::uint64_t, std::size_t> indices;
    for (auto& [entry, function] : functions) {
        if (function.Import) function.Effects = {EveryRegister, EveryRegister};
        indices.emplace(entry, nodes.size());
        nodes.push_back(&function);
    }
    constexpr auto Unknown = ~std::size_t{0};
    std::vector<std::vector<std::size_t>> edges(nodes.size());
    for (std::size_t i = 0; i < nodes.size(); ++i) {
        for (const auto exit : nodes[i]->Exits) {
            const auto found = indices.find(exit);
            edges[i].push_back(found == indices.end() ? Unknown : found->second);
        }
    }
    for (bool changed = true; changed;) {
        changed = false;
        for (std::size_t i = 0; i < nodes.size(); ++i) {
            auto effects = nodes[i]->Effects;
            for (const auto edge : edges[i]) {
                const auto reached = edge == Unknown ? RegisterEffects{EveryRegister, EveryRegister} : nodes[edge]->Effects;
                effects.Named |= reached.Named;
                effects.Written |= reached.Written;
            }
            if (effects == nodes[i]->Effects) continue;
            nodes[i]->Effects = effects;
            changed = true;
        }
    }
}

GuestFunction DiscoverFunction(const GuestImage& image, const Decoder& decoder, const std::set<std::uint64_t>& references, const std::uint64_t entry, const std::set<std::uint64_t>& starts,
                               std::set<std::uint64_t>& reachedStarts, std::set<std::uint64_t>& takenAddresses) {
    GuestFunction discovered;
    discovered.Entry = entry;
    const auto symbol = image.FunctionSymbols().find(entry);
    discovered.Name = symbol != image.FunctionSymbols().end() ? symbol->second : "sub_" + Hex(entry);
    if (const auto import = image.Imports().find(entry); import != image.Imports().end()) {
        discovered.Import = import->second;
        discovered.Name = import->second;
        return discovered;
    }

    DecodedFunction function;
    function.Entry = entry;

    function.Leaders.insert(entry);
    std::deque<std::pair<std::uint64_t, std::uint64_t>> pending{{entry, entry}};
    if (const auto pads = image.LandingPads().find(entry); pads != image.LandingPads().end()) {
        for (const auto pad : pads->second) {
            function.Leaders.insert(pad);
            function.ResumePoints.insert(pad);
            pending.emplace_back(pad, entry);
        }
    }
    const auto ownCode = [&](const std::uint64_t address) { return address == entry || !starts.contains(address); };
    std::vector<std::uint64_t> indirectJumps;
    while (true) {
        while (!pending.empty()) {
            const auto [start, from] = pending.front();
            pending.pop_front();
            auto address = start;
            while (!function.Instructions.contains(address)) {
                const auto instruction = DecodeReached(image, decoder, entry, address, start, from);
                function.Instructions.emplace(address, instruction);
                const auto target = DirectTarget(instruction);
                const auto mnemonic = instruction.Info.mnemonic;
                if (mnemonic == ZYDIS_MNEMONIC_CALL && target) {
                    if (!image.IsExecutable(*target) && !image.Imports().contains(*target))
                        throw UndecodableCode("The call at 0x" + Hex(address) + " in the function at 0x" + Hex(entry) + " reaches 0x" + Hex(*target) + ", outside executable segments");
                    reachedStarts.insert(*target);
                }
                if (mnemonic == ZYDIS_MNEMONIC_CALL) {
                    if (const auto import = CalledImport(image, decoder, instruction); import && ReturnsTwice(*import)) {
                        if (!ownCode(instruction.Next())) throw std::runtime_error("The call to " + *import + " at 0x" + Hex(address) + " returns into another function's start at 0x" + Hex(instruction.Next()));
                        function.ResumePoints.insert(instruction.Next());
                        function.Leaders.insert(instruction.Next());
                    }
                }
                if (mnemonic == ZYDIS_MNEMONIC_JMP && !target && ImportTarget(image, instruction)) break;
                if ((mnemonic == ZYDIS_MNEMONIC_JMP || IsConditionalJump(mnemonic)) && target && ownCode(*target)) {
                    function.Leaders.insert(*target);
                    pending.emplace_back(*target, address);
                }
                if (IsConditionalJump(mnemonic) && ownCode(instruction.Next())) function.Leaders.insert(instruction.Next());
                if (mnemonic == ZYDIS_MNEMONIC_JMP && !target && instruction.Info.meta.branch_type != ZYDIS_BRANCH_TYPE_FAR) indirectJumps.push_back(address);
                if (EndsFlow(instruction)) break;
                address = instruction.Next();
                if (!ownCode(address)) break;
            }
        }
        const auto predecessors = PredecessorsOf(function);
        std::vector<std::uint64_t> unresolved;
        for (const auto jump : indirectJumps) {
            auto table = RecoverJumpTable(image, function, predecessors, function.Instructions.at(jump), starts, references);
            if (table.Targets.empty()) {
                unresolved.push_back(jump);
                continue;
            }
            for (const auto destination : table.Targets) {
                if (!ownCode(destination)) continue;
                function.Leaders.insert(destination);
                pending.emplace_back(destination, jump);
            }
            function.JumpTables.emplace(jump, std::move(table.Targets));
            if (!table.Guarded) function.UnguardedJumpTables.insert(jump);
            if (table.Closed) function.ClosedJumpTables.insert(jump);
        }
        if (unresolved.size() == indirectJumps.size()) break;
        indirectJumps = std::move(unresolved);
    }
    for (const auto& [address, instruction] : function.Instructions) {
        if (const auto taken = TakenFunctionAddress(image, function, instruction)) takenAddresses.insert(*taken);
    }
    for (auto current = function.Instructions.begin(); current != function.Instructions.end(); ++current) {
        const auto& instruction = current->second;
        const auto following = std::next(current);
        const bool adjacent = following != function.Instructions.end() && following->first == instruction.Next();
        if (!EndsFlow(instruction) && !adjacent && function.Instructions.contains(instruction.Next())) function.Leaders.insert(instruction.Next());
    }
    const auto predecessors = PredecessorsOf(function);
    for (const auto& [jump, targets] : function.JumpTables) {
        if (RecoverJumpTable(image, function, predecessors, function.Instructions.at(jump), starts, references).Targets != targets)
            throw std::runtime_error("The jump table of the jump at 0x" + Hex(jump) + " in the function at 0x" + Hex(entry) + " reads differently once the code it leads to is known");
    }
    discovered.Code.reserve(function.Instructions.size());
    for (const auto& [address, instruction] : function.Instructions) discovered.Code.push_back({address, instruction.Info.length});
    discovered.Effects = LocalEffects(image, function);
    discovered.Exits = Exits(image, function);
    static_cast<FunctionFlow&>(discovered) = std::move(function);
    return discovered;
}

bool SpansAny(const GuestFunction& function, const std::set<std::uint64_t>& added) {
    if (function.Code.empty()) return false;
    auto low = function.Code.front().Address;
    auto high = function.Code.back().Next();
    for (const auto& [jump, targets] : function.JumpTables) {
        for (const auto target : targets) {
            low = std::min(low, target);
            high = std::max(high, target + 1);
        }
    }
    const auto first = added.lower_bound(low);
    return first != added.end() && *first < high;
}

constexpr std::uint8_t Int3 = 0xcc;
constexpr std::uint64_t FunctionAlignment = 16;

std::vector<std::pair<std::uint64_t, std::uint64_t>> CoveredCode(const std::map<std::uint64_t, GuestFunction>& functions) {
    std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges;
    for (const auto& [entry, function] : functions) {
        for (const auto& span : function.Code) {
            if (!ranges.empty() && ranges.back().second == span.Address) ranges.back().second = span.Next();
            else ranges.emplace_back(span.Address, span.Next());
        }
    }
    std::sort(ranges.begin(), ranges.end());
    std::vector<std::pair<std::uint64_t, std::uint64_t>> merged;
    for (const auto& range : ranges) {
        if (!merged.empty() && range.first <= merged.back().second) merged.back().second = std::max(merged.back().second, range.second);
        else merged.push_back(range);
    }
    return merged;
}

bool Decodes(const GuestImage& image, const Decoder& decoder, const std::set<std::uint64_t>& references, const std::uint64_t address, const std::set<std::uint64_t>& known) {
    std::set<std::uint64_t> reached;
    std::set<std::uint64_t> taken;
    try {
        DiscoverFunction(image, decoder, references, address, known, reached, taken);
        return true;
    } catch (const UndecodableCode&) {
        return false;
    }
}

std::set<std::uint64_t> PaddedStarts(const GuestImage& image, const Decoder& decoder, const std::set<std::uint64_t>& references, const std::set<std::uint64_t>& known,
                                     const std::map<std::uint64_t, GuestFunction>& functions) {
    const auto covered = CoveredCode(functions);
    const auto isCovered = [&](const std::uint64_t address) {
        const auto after = std::upper_bound(covered.begin(), covered.end(), address, [](const std::uint64_t value, const auto& range) { return value < range.first; });
        return after != covered.begin() && address < std::prev(after)->second;
    };
    std::set<std::uint64_t> starts;
    for (const auto& segment : image.Segments()) {
        if (!segment.IsExecutable()) continue;
        const auto first = (segment.Address / FunctionAlignment + 1) * FunctionAlignment;
        for (auto address = first; address < segment.Address + segment.Bytes.size(); address += FunctionAlignment) {
            const auto offset = address - segment.Address;
            if (segment.Bytes[offset - 1] == Int3 && segment.Bytes[offset] != Int3 && !isCovered(address) && Decodes(image, decoder, references, address, known)) starts.insert(address);
        }
    }
    return starts;
}

}

std::uint64_t Instruction::Next() const {
    return Address + Info.length;
}

std::uint64_t InstructionSpan::Next() const {
    return Address + Length;
}

Decoder::Decoder() {
    if (ZYAN_FAILED(ZydisDecoderInit(&_decoder, ZYDIS_MACHINE_MODE_LONG_64, ZYDIS_STACK_WIDTH_64))) throw std::runtime_error("Zydis decoder initialization failed");
    if (ZYAN_FAILED(ZydisDecoderEnableMode(&_decoder, ZYDIS_DECODER_MODE_AMD_BRANCHES, ZYAN_TRUE))) throw std::runtime_error("Zydis decoder initialization failed");
}

Instruction Decoder::Decode(const GuestImage& image, const std::uint64_t address) const {
    const auto code = image.CodeAt(address);
    if (code.empty()) throw UndecodableCode("Control flow reaches 0x" + Hex(address) + ", outside executable segments");
    const auto instruction = TryDecode(code, address);
    if (!instruction) throw UndecodableCode("Cannot decode the instruction at 0x" + Hex(address));
    return *instruction;
}

std::optional<Instruction> Decoder::TryDecode(const std::span<const std::uint8_t> code, const std::uint64_t address) const {
    Instruction instruction{};
    instruction.Address = address;
    if (ZYAN_FAILED(ZydisDecoderDecodeFull(&_decoder, code.data(), code.size(), &instruction.Info, instruction.Operands.data()))) return std::nullopt;
    return instruction;
}

RegisterEffects InstructionEffects(const Instruction& instruction) {
    const auto bit = [](const ZydisRegister reg) -> RegisterSet {
        const auto enclosing = ZydisRegisterGetLargestEnclosing(ZYDIS_MACHINE_MODE_LONG_64, reg);
        if (enclosing >= ZYDIS_REGISTER_RAX && enclosing <= ZYDIS_REGISTER_R15) return RegisterSet{1} << (enclosing - ZYDIS_REGISTER_RAX);
        if (reg >= ZYDIS_REGISTER_XMM0 && reg <= ZYDIS_REGISTER_XMM15) return RegisterSet{1} << (FirstVectorBit + reg - ZYDIS_REGISTER_XMM0);
        if (reg >= ZYDIS_REGISTER_YMM0 && reg <= ZYDIS_REGISTER_YMM15) return RegisterSet{1} << (FirstVectorBit + reg - ZYDIS_REGISTER_YMM0);
        return 0;
    };
    RegisterEffects effects;
    const auto mnemonic = instruction.Info.mnemonic;
    if (mnemonic == ZYDIS_MNEMONIC_VZEROUPPER) {
        constexpr auto vectors = EveryRegister << FirstVectorBit;
        effects = {vectors, vectors};
    }
    for (unsigned i = 0; i < instruction.Info.operand_count; ++i) {
        const auto& operand = instruction.Operands[i];
        if (operand.type == ZYDIS_OPERAND_TYPE_REGISTER) {
            effects.Named |= bit(operand.reg.value);
            if ((operand.actions & ZYDIS_OPERAND_ACTION_MASK_WRITE) != 0) effects.Written |= bit(operand.reg.value);
        }
        if (operand.type == ZYDIS_OPERAND_TYPE_MEMORY) effects.Named |= bit(operand.mem.base) | bit(operand.mem.index);
    }
    return effects;
}

bool IsConditionalJump(const ZydisMnemonic mnemonic) {
    switch (mnemonic) {
    case ZYDIS_MNEMONIC_JO:
    case ZYDIS_MNEMONIC_JNO:
    case ZYDIS_MNEMONIC_JB:
    case ZYDIS_MNEMONIC_JNB:
    case ZYDIS_MNEMONIC_JZ:
    case ZYDIS_MNEMONIC_JNZ:
    case ZYDIS_MNEMONIC_JBE:
    case ZYDIS_MNEMONIC_JNBE:
    case ZYDIS_MNEMONIC_JS:
    case ZYDIS_MNEMONIC_JNS:
    case ZYDIS_MNEMONIC_JP:
    case ZYDIS_MNEMONIC_JNP:
    case ZYDIS_MNEMONIC_JL:
    case ZYDIS_MNEMONIC_JNL:
    case ZYDIS_MNEMONIC_JLE:
    case ZYDIS_MNEMONIC_JNLE:
    case ZYDIS_MNEMONIC_JRCXZ:
    case ZYDIS_MNEMONIC_JECXZ:
    case ZYDIS_MNEMONIC_LOOP:
    case ZYDIS_MNEMONIC_LOOPE:
    case ZYDIS_MNEMONIC_LOOPNE:
        return true;
    default:
        return false;
    }
}

std::optional<std::uint64_t> DirectTarget(const Instruction& instruction) {
    const auto& operand = instruction.Operands[0];
    if (operand.type != ZYDIS_OPERAND_TYPE_IMMEDIATE || !operand.imm.is_relative) return std::nullopt;
    return instruction.Next() + operand.imm.value.u;
}

std::optional<std::uint64_t> ImportTarget(const GuestImage& image, const Instruction& instruction) {
    const auto& operand = instruction.Operands[0];
    if (operand.type != ZYDIS_OPERAND_TYPE_MEMORY || operand.mem.index != ZYDIS_REGISTER_NONE || operand.mem.segment == ZYDIS_REGISTER_FS || operand.mem.segment == ZYDIS_REGISTER_GS) return std::nullopt;
    const auto displacement = static_cast<std::uint64_t>(operand.mem.disp.value);
    if (operand.mem.base == ZYDIS_REGISTER_RIP) return image.ImportAt(instruction.Next() + displacement);
    return std::nullopt;
}

std::string BridgeSymbol(const std::string& import) {
    std::string symbol = "ps5lift_import_";
    for (const char character : import) {
        if (character == '_') symbol += "__";
        else if (character == '+') symbol += "_p";
        else if (character == '-') symbol += "_m";
        else if (character == '#') symbol += "_h";
        else if ((character >= 'a' && character <= 'z') || (character >= 'A' && character <= 'Z') || (character >= '0' && character <= '9')) symbol += character;
        else throw std::runtime_error("Import name " + import + " has a character a bridge symbol cannot carry");
    }
    return symbol;
}

std::string Nid(const std::string_view name) {
    static constexpr std::uint8_t Suffix[] = {0x51, 0x8D, 0x64, 0xA6, 0x35, 0xDE, 0xD8, 0xC1, 0xE6, 0xB0, 0x39, 0xB1, 0xC3, 0xE5, 0x52, 0x30};
    llvm::SHA1 sha;
    sha.update(llvm::StringRef(name.data(), name.size()));
    sha.update(llvm::ArrayRef<std::uint8_t>(Suffix));
    const auto digest = sha.final();
    std::array<std::uint8_t, 8> prefix{};
    std::copy_n(digest.begin(), prefix.size(), prefix.begin());
    std::reverse(prefix.begin(), prefix.end());
    auto encoded = llvm::encodeBase64(prefix);
    std::replace(encoded.begin(), encoded.end(), '/', '-');
    return encoded.substr(0, 11);
}

std::optional<std::string> JumpBufferFunction(const std::string& import) {
    static const auto Names = NamesAndNids({"setjmp", "_setjmp", "sigsetjmp", "__sigsetjmp", "longjmp", "_longjmp", "siglongjmp"});
    constexpr std::string_view GuestExport = "#guest";
    const auto name = import.ends_with(GuestExport) ? import.substr(0, import.size() - GuestExport.size()) : import;
    if (!Names.contains(name)) return std::nullopt;
    return name;
}

std::map<std::uint64_t, GuestFunction> DiscoverFunctions(const GuestImage& image) {
    const Decoder decoder;
    const auto references = DataReferences(image, decoder);
    std::set<std::uint64_t> starts;
    for (const auto start : {image.Entry(), image.Init()}) {
        if (start != 0) starts.insert(start);
    }
    for (const auto& [address, name] : image.Imports()) starts.insert(address);
    for (const auto address : image.FunctionExports()) {
        if (image.IsExecutable(address)) starts.insert(address);
    }
    for (const auto address : image.CodePointers()) starts.insert(address);
    for (const auto& [address, name] : image.FunctionSymbols()) {
        if (image.IsExecutable(address)) starts.insert(address);
    }
    for (const auto address : image.ExceptionFunctionStarts()) starts.insert(address);

    std::map<std::uint64_t, GuestFunction> functions;
    auto added = starts;
    while (true) {
        std::set<std::uint64_t> reachedStarts;
        std::set<std::uint64_t> takenAddresses;
        for (auto& [entry, function] : functions) {
            if (SpansAny(function, added)) function = DiscoverFunction(image, decoder, references, entry, starts, reachedStarts, takenAddresses);
        }
        for (const auto entry : added) functions.emplace(entry, DiscoverFunction(image, decoder, references, entry, starts, reachedStarts, takenAddresses));
        for (const auto address : takenAddresses) {
            if (!starts.contains(address) && !reachedStarts.contains(address) && Decodes(image, decoder, references, address, starts)) reachedStarts.insert(address);
        }
        added.clear();
        std::set_difference(reachedStarts.begin(), reachedStarts.end(), starts.begin(), starts.end(), std::inserter(added, added.end()));
        if (added.empty()) added = PaddedStarts(image, decoder, references, starts, functions);
        if (added.empty()) {
            CloseEffects(functions);
            return functions;
        }
        starts.insert(added.begin(), added.end());
    }
}

DecodedFunction Decode(const GuestImage& image, const GuestFunction& function) {
    const Decoder decoder;
    DecodedFunction decoded;
    static_cast<FunctionFlow&>(decoded) = function;
    for (const auto& span : function.Code) {
        auto instruction = decoder.Decode(image, span.Address);
        if (instruction.Info.length != span.Length)
            throw std::logic_error("The instruction at 0x" + Hex(span.Address) + " in the function at 0x" + Hex(function.Entry) + " decodes to another length than discovery found");
        decoded.Instructions.emplace_hint(decoded.Instructions.end(), span.Address, std::move(instruction));
    }
    return decoded;
}

}
