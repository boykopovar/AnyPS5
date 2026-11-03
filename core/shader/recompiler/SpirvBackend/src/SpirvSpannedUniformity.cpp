#include "SpirvInstructionOperands.hpp"
#include "SpirvBackend/SpirvSpannedUniformity.hpp"
#include <algorithm>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {
namespace {

using Instruction = std::vector<std::uint32_t>;
using SpirvInstructions::Opcode;
using SpirvInstructions::Result;
using SpirvInstructions::VisitIdOperands;

struct Block {
    std::uint32_t label = 0u;
    std::vector<Instruction> body;
};

struct Function {
    std::uint32_t id = 0u;
    std::vector<std::uint32_t> parameters;
    std::vector<Block> blocks;
};

struct Module {
    std::unordered_map<std::uint32_t, std::uint32_t> builtins;
    std::unordered_map<std::uint32_t, std::uint32_t> storageClasses;
    std::unordered_map<std::uint32_t, std::uint32_t> pointerClasses;
    std::unordered_map<std::uint32_t, std::uint32_t> constants;
    std::vector<Function> functions;
};

Module Parse(std::span<const std::uint32_t> words) {
    if (words.size() < 5u) throw std::runtime_error("SPIR-V module has no header");
    Module module;
    Function* function = nullptr;
    Block* block = nullptr;
    for (std::size_t cursor = 5u; cursor < words.size();) {
        const auto count = words[cursor] >> 16u;
        if (count == 0u || cursor + count > words.size()) throw std::runtime_error("SPIR-V instruction runs past the module");
        Instruction instruction(words.begin() + static_cast<std::ptrdiff_t>(cursor), words.begin() + static_cast<std::ptrdiff_t>(cursor + count));
        cursor += count;
        switch (Opcode(instruction)) {
        case spv::OpDecorate:
            if (instruction.size() >= 4u && instruction[2] == spv::DecorationBuiltIn) module.builtins[instruction[1]] = instruction[3];
            break;
        case spv::OpTypePointer:
            if (instruction.size() >= 4u) module.pointerClasses[instruction[1]] = instruction[2];
            break;
        case spv::OpConstant:
            if (instruction.size() >= 4u) module.constants[instruction[2]] = instruction[3];
            break;
        case spv::OpVariable:
            if (instruction.size() >= 4u) module.storageClasses[instruction[2]] = instruction[3];
            if (block != nullptr) block->body.push_back(std::move(instruction));
            break;
        case spv::OpFunction:
            function = &module.functions.emplace_back();
            function->id = instruction.at(2);
            break;
        case spv::OpFunctionParameter:
            if (function != nullptr) function->parameters.push_back(instruction.at(2));
            break;
        case spv::OpLabel:
            if (function == nullptr) throw std::runtime_error("SPIR-V label outside a function");
            block = &function->blocks.emplace_back();
            block->label = instruction.at(1);
            break;
        case spv::OpFunctionEnd:
            function = nullptr;
            block = nullptr;
            break;
        default:
            if (block != nullptr) block->body.push_back(std::move(instruction));
            break;
        }
    }
    return module;
}

bool VaryingBuiltin(std::uint32_t builtin) {
    switch (builtin) {
    case spv::BuiltInLocalInvocationId:
    case spv::BuiltInGlobalInvocationId:
    case spv::BuiltInLocalInvocationIndex:
    case spv::BuiltInSubgroupId:
    case spv::BuiltInSubgroupLocalInvocationId:
    case spv::BuiltInSubgroupEqMask:
    case spv::BuiltInSubgroupGeMask:
    case spv::BuiltInSubgroupGtMask:
    case spv::BuiltInSubgroupLeMask:
    case spv::BuiltInSubgroupLtMask:
        return true;
    default:
        return false;
    }
}

bool PerInvocationResult(spv::Op op) {
    const auto raw = static_cast<std::uint32_t>(op);
    if (raw >= spv::OpAtomicExchange && raw <= spv::OpAtomicXor) return true;
    if (raw >= spv::OpGroupAll && raw <= spv::OpGroupSMax) return true;
    if (raw >= spv::OpGroupNonUniformElect && raw <= spv::OpGroupNonUniformQuadSwap) return true;
    switch (op) {
    case spv::OpAtomicFlagTestAndSet:
    case spv::OpAtomicFMinEXT:
    case spv::OpAtomicFMaxEXT:
    case spv::OpAtomicFAddEXT:
    case spv::OpSubgroupBallotKHR:
    case spv::OpSubgroupFirstInvocationKHR:
    case spv::OpSubgroupAllKHR:
    case spv::OpSubgroupAnyKHR:
    case spv::OpSubgroupAllEqualKHR:
    case spv::OpSubgroupReadInvocationKHR:
    case spv::OpGroupNonUniformRotateKHR:
        return true;
    default:
        return false;
    }
}

bool PrivateStorage(std::uint32_t storageClass) {
    return storageClass == spv::StorageClassFunction || storageClass == spv::StorageClassPrivate;
}

std::uint32_t MemoryClass(std::uint32_t storageClass) {
    switch (storageClass) {
    case spv::StorageClassUniform:
    case spv::StorageClassPhysicalStorageBuffer:
        return spv::StorageClassStorageBuffer;
    default:
        return storageClass;
    }
}

bool AtomicWrite(spv::Op op) {
    const auto raw = static_cast<std::uint32_t>(op);
    return (raw >= spv::OpAtomicExchange && raw <= spv::OpAtomicXor) || op == spv::OpAtomicFlagTestAndSet || op == spv::OpAtomicFMinEXT || op == spv::OpAtomicFMaxEXT ||
        op == spv::OpAtomicFAddEXT;
}

bool ImageRead(spv::Op op) {
    return op == spv::OpImageRead || op == spv::OpImageFetch || op == spv::OpImageSparseRead || op == spv::OpImageSparseFetch;
}

class Analysis {
public:
    explicit Analysis(const Module& module) : module(module) {
        for (const auto& function : module.functions) {
            for (const auto& block : function.blocks) {
                for (const auto& instruction : block.body) {
                    const auto op = Opcode(instruction);
                    if (op == spv::OpControlBarrier) barriers.insert(function.id);
                    bool hasResult = false;
                    bool hasType = false;
                    spv::HasResultAndType(op, &hasResult, &hasType);
                    if (hasResult && hasType && instruction.size() > 2u) types[instruction[2]] = instruction[1];
                }
            }
        }
        for (const auto& function : module.functions) {
            for (const auto& block : function.blocks) {
                for (const auto& instruction : block.body) {
                    const auto pointer = WrittenPointer(instruction);
                    if (pointer != 0u) written.insert(ClassOf(pointer));
                    if (Opcode(instruction) == spv::OpImageWrite) written.insert(spv::StorageClassImage);
                }
            }
        }
        for (bool changed = true; changed;) {
            changed = false;
            for (const auto& function : module.functions) {
                if (barriers.contains(function.id)) continue;
                for (const auto& block : function.blocks) {
                    for (const auto& instruction : block.body) {
                        if (Opcode(instruction) == spv::OpFunctionCall && instruction.size() > 3u && barriers.contains(instruction[3]) && barriers.insert(function.id).second) changed = true;
                    }
                }
            }
        }
    }

    std::optional<std::string> Run() {
        if (barriers.empty()) return std::nullopt;
        std::vector<Flow> flows;
        flows.reserve(module.functions.size());
        for (const auto& function : module.functions) flows.push_back(BuildFlow(function));
        for (;;) {
            const auto before = Size();
            report.reset();
            for (std::size_t index = 0u; index < module.functions.size(); ++index) Visit(module.functions[index], flows[index]);
            if (Size() == before) return report;
        }
    }

private:
    struct Flow {
        std::vector<std::uint32_t> order;
        std::unordered_map<std::uint32_t, std::size_t> index;
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> successors;
        std::unordered_map<std::uint32_t, std::uint32_t> idom;
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> children;
    };

    static std::uint32_t WrittenPointer(const Instruction& instruction) {
        const auto op = Opcode(instruction);
        if ((op == spv::OpStore || op == spv::OpAtomicStore || op == spv::OpAtomicFlagClear || op == spv::OpCopyMemory) && instruction.size() > 1u) return instruction[1];
        if (AtomicWrite(op) && instruction.size() > 3u) return instruction[3];
        return 0u;
    }

    std::uint32_t ClassOf(std::uint32_t pointer) const {
        if (const auto variable = module.storageClasses.find(pointer); variable != module.storageClasses.end()) return MemoryClass(variable->second);
        const auto type = types.find(pointer);
        if (type == types.end()) return UnknownClass;
        const auto storage = module.pointerClasses.find(type->second);
        return storage == module.pointerClasses.end() ? UnknownClass : MemoryClass(storage->second);
    }

    bool Written(std::uint32_t storageClass) const {
        return written.contains(UnknownClass) || written.contains(storageClass);
    }

    bool Fence(const Instruction& instruction) const {
        if (Opcode(instruction) != spv::OpControlBarrier || instruction.size() < 4u) return false;
        const auto scope = module.constants.find(instruction[1]);
        const auto semantics = module.constants.find(instruction[3]);
        return scope != module.constants.end() && semantics != module.constants.end() && scope->second <= spv::ScopeWorkgroup &&
            (semantics->second & spv::MemorySemanticsWorkgroupMemoryMask) != 0u;
    }

    bool Fenced(bool fenced, const Instruction& instruction) const {
        if (Fence(instruction)) return true;
        if (Opcode(instruction) == spv::OpFunctionCall) return false;
        const auto pointer = WrittenPointer(instruction);
        if (pointer == 0u) return fenced;
        const auto storageClass = ClassOf(pointer);
        return fenced && storageClass != spv::StorageClassWorkgroup && storageClass != UnknownClass;
    }

    bool Racy(std::uint32_t pointer, bool fenced) const {
        const auto storageClass = ClassOf(pointer);
        if (storageClass != UnknownClass && (PrivateStorage(storageClass) || storageClass == spv::StorageClassInput)) return false;
        if (!Written(storageClass)) return false;
        return !fenced || storageClass != spv::StorageClassWorkgroup;
    }

    std::unordered_map<std::uint32_t, bool> FencedEntries(const Function& function, const Flow& flow, const std::unordered_map<std::uint32_t, const Block*>& blocks) const {
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> predecessors;
        for (const auto node : flow.order) {
            for (const auto successor : flow.successors.at(node)) predecessors[successor].push_back(node);
        }
        std::unordered_map<std::uint32_t, bool> entries;
        std::unordered_map<std::uint32_t, bool> exits;
        for (const auto node : flow.order) exits[node] = true;
        const auto root = function.blocks.front().label;
        for (bool changed = true; changed;) {
            changed = false;
            for (const auto node : flow.order) {
                bool fenced = node != root;
                for (const auto predecessor : predecessors[node]) fenced = fenced && exits.at(predecessor);
                entries[node] = fenced;
                for (const auto& instruction : blocks.at(node)->body) fenced = Fenced(fenced, instruction);
                if (exits[node] != fenced) {
                    exits[node] = fenced;
                    changed = true;
                }
            }
        }
        return entries;
    }

    std::size_t Size() const {
        return nonUniform.size() + storedNonUniform.size() + varyingFunctions.size();
    }

    bool NonUniform(std::uint32_t id) const {
        return nonUniform.contains(id);
    }

    static std::vector<std::uint32_t> Successors(const Block& block) {
        if (block.body.empty()) return {};
        const auto& terminator = block.body.back();
        switch (Opcode(terminator)) {
        case spv::OpBranch: return {terminator.at(1)};
        case spv::OpBranchConditional: return {terminator.at(2), terminator.at(3)};
        case spv::OpSwitch: {
            std::vector<std::uint32_t> targets{terminator.at(2)};
            for (std::size_t operand = 4u; operand < terminator.size(); operand += 2u) targets.push_back(terminator[operand]);
            return targets;
        }
        default: return {};
        }
    }

    static Flow BuildFlow(const Function& function) {
        Flow flow;
        for (const auto& block : function.blocks) flow.successors[block.label] = Successors(block);
        if (function.blocks.empty()) return flow;
        std::vector<std::uint32_t> postorder;
        std::unordered_set<std::uint32_t> seen{function.blocks.front().label};
        std::vector<std::pair<std::uint32_t, std::size_t>> stack{{function.blocks.front().label, 0u}};
        while (!stack.empty()) {
            auto& [node, next] = stack.back();
            const auto& successors = flow.successors[node];
            if (next == successors.size()) {
                postorder.push_back(node);
                stack.pop_back();
                continue;
            }
            const auto successor = successors[next++];
            if (seen.insert(successor).second) stack.emplace_back(successor, 0u);
        }
        flow.order.assign(postorder.rbegin(), postorder.rend());
        for (std::size_t position = 0u; position < flow.order.size(); ++position) flow.index[flow.order[position]] = position;
        std::unordered_map<std::uint32_t, std::vector<std::uint32_t>> predecessors;
        for (const auto node : flow.order) {
            for (const auto successor : flow.successors[node]) predecessors[successor].push_back(node);
        }
        const auto root = flow.order.front();
        flow.idom[root] = root;
        const auto intersect = [&](std::uint32_t left, std::uint32_t right) {
            while (left != right) {
                while (flow.index[left] > flow.index[right]) left = flow.idom[left];
                while (flow.index[right] > flow.index[left]) right = flow.idom[right];
            }
            return left;
        };
        for (bool changed = true; changed;) {
            changed = false;
            for (std::size_t position = 1u; position < flow.order.size(); ++position) {
                const auto node = flow.order[position];
                std::uint32_t dominator = 0u;
                for (const auto predecessor : predecessors[node]) {
                    if (!flow.idom.contains(predecessor)) continue;
                    dominator = dominator == 0u ? predecessor : intersect(predecessor, dominator);
                }
                if (dominator != 0u && flow.idom[node] != dominator) {
                    flow.idom[node] = dominator;
                    changed = true;
                }
            }
        }
        for (const auto node : flow.order) {
            if (node != root) flow.children[flow.idom[node]].push_back(node);
        }
        return flow;
    }

    static bool Exits(spv::Op op) {
        return op == spv::OpReturn || op == spv::OpReturnValue || op == spv::OpKill || op == spv::OpTerminateInvocation || op == spv::OpUnreachable;
    }

    static std::vector<std::uint32_t> Reachable(const Flow& flow, std::uint32_t from) {
        std::vector<std::uint32_t> blocks;
        std::unordered_set<std::uint32_t> seen{from};
        std::vector<std::uint32_t> pending{from};
        while (!pending.empty()) {
            const auto node = pending.back();
            pending.pop_back();
            blocks.push_back(node);
            if (const auto found = flow.successors.find(node); found != flow.successors.end()) {
                for (const auto successor : found->second) {
                    if (seen.insert(successor).second) pending.push_back(successor);
                }
            }
        }
        return blocks;
    }

    static std::vector<std::uint32_t> Construct(const Flow& flow, std::uint32_t header, std::uint32_t merge) {
        std::vector<std::uint32_t> blocks;
        std::vector<std::uint32_t> pending;
        if (const auto found = flow.children.find(header); found != flow.children.end()) pending = found->second;
        while (!pending.empty()) {
            const auto node = pending.back();
            pending.pop_back();
            if (node == merge) continue;
            blocks.push_back(node);
            if (const auto found = flow.children.find(node); found != flow.children.end()) pending.insert(pending.end(), found->second.begin(), found->second.end());
        }
        return blocks;
    }

    std::uint32_t BaseVariable(std::uint32_t pointer, const std::unordered_map<std::uint32_t, std::uint32_t>& chains) const {
        for (std::size_t depth = 0u; depth < 64u; ++depth) {
            const auto found = chains.find(pointer);
            if (found == chains.end()) break;
            pointer = found->second;
        }
        return pointer;
    }

    void Visit(const Function& function, const Flow& flow) {
        if (function.blocks.empty()) return;
        std::unordered_map<std::uint32_t, const Block*> blocks;
        std::unordered_map<std::uint32_t, std::uint32_t> chains;
        for (const auto& block : function.blocks) {
            blocks[block.label] = &block;
            for (const auto& instruction : block.body) {
                const auto op = Opcode(instruction);
                if ((op == spv::OpAccessChain || op == spv::OpInBoundsAccessChain || op == spv::OpPtrAccessChain) && instruction.size() > 3u) chains[instruction[2]] = instruction[3];
            }
        }
        const auto fencedEntries = FencedEntries(function, flow, blocks);
        for (const auto parameter : function.parameters) nonUniform.insert(parameter);
        std::unordered_set<std::uint32_t> divergent;
        std::unordered_set<std::uint32_t> divergentMerges;
        for (auto position = flow.order.size(); position-- > 0u;) {
            const auto header = flow.order[position];
            const auto& body = blocks.at(header)->body;
            if (body.size() < 2u) continue;
            const auto& mergeInstruction = body[body.size() - 2u];
            const auto mergeOp = Opcode(mergeInstruction);
            if (mergeOp != spv::OpSelectionMerge && mergeOp != spv::OpLoopMerge) continue;
            const auto merge = mergeInstruction.at(1);
            const auto& terminator = body.back();
            const auto terminatorOp = Opcode(terminator);
            bool split = (terminatorOp == spv::OpBranchConditional || terminatorOp == spv::OpSwitch) && NonUniform(terminator.at(1));
            const auto construct = Construct(flow, header, merge);
            if (mergeOp == spv::OpLoopMerge && !split) {
                for (const auto node : construct) {
                    const auto& successors = flow.successors.at(node);
                    if (std::find(successors.begin(), successors.end(), merge) == successors.end()) continue;
                    const auto& exit = blocks.at(node)->body.back();
                    const auto exitOp = Opcode(exit);
                    if (divergent.contains(node) || ((exitOp == spv::OpBranchConditional || exitOp == spv::OpSwitch) && NonUniform(exit.at(1)))) {
                        split = true;
                        break;
                    }
                }
            }
            if (!split) {
                const auto continueTarget = mergeOp == spv::OpLoopMerge ? mergeInstruction.at(2) : 0u;
                for (const auto node : construct) {
                    const auto& nodeBody = blocks.at(node)->body;
                    if (nodeBody.empty()) continue;
                    const auto& exit = nodeBody.back();
                    const auto exitOp = Opcode(exit);
                    if ((exitOp != spv::OpBranchConditional && exitOp != spv::OpSwitch) || !NonUniform(exit.at(1))) continue;
                    if (nodeBody.size() >= 2u && (Opcode(nodeBody[nodeBody.size() - 2u]) == spv::OpSelectionMerge || Opcode(nodeBody[nodeBody.size() - 2u]) == spv::OpLoopMerge)) continue;
                    const auto& successors = flow.successors.at(node);
                    if (std::find(successors.begin(), successors.end(), merge) != successors.end() || (continueTarget != 0u && std::find(successors.begin(), successors.end(), continueTarget) != successors.end())) {
                        split = true;
                        break;
                    }
                }
            }
            if (!split) continue;
            divergent.insert(construct.begin(), construct.end());
            if (mergeOp == spv::OpLoopMerge) divergent.insert(header);
            divergentMerges.insert(merge);
            const std::unordered_set<std::uint32_t> inside(construct.begin(), construct.end());
            bool exits = false;
            bool escapes = false;
            for (const auto node : construct) {
                exits = exits || (!blocks.at(node)->body.empty() && Exits(Opcode(blocks.at(node)->body.back())));
                for (const auto successor : flow.successors.at(node)) escapes = escapes || (successor != merge && successor != header && !inside.contains(successor));
            }
            if (exits) {
                const auto after = Reachable(flow, merge);
                divergent.insert(after.begin(), after.end());
            } else if (escapes) {
                const auto after = Construct(flow, merge, 0u);
                divergent.insert(merge);
                divergent.insert(after.begin(), after.end());
            }
        }
        for (const auto label : flow.order) {
            const bool inDivergentFlow = divergent.contains(label);
            bool fenced = fencedEntries.at(label);
            for (const auto& instruction : blocks.at(label)->body) {
                const auto op = Opcode(instruction);
                const bool fencedBefore = fenced;
                fenced = Fenced(fenced, instruction);
                if (op == spv::OpStore && instruction.size() >= 3u) {
                    const auto variable = BaseVariable(instruction[1], chains);
                    const auto storage = module.storageClasses.find(variable);
                    if (storage != module.storageClasses.end() && PrivateStorage(storage->second) && (inDivergentFlow || NonUniform(instruction[1]) || NonUniform(instruction[2]))) storedNonUniform.insert(variable);
                    continue;
                }
                if (inDivergentFlow && !report && (op == spv::OpControlBarrier || (op == spv::OpFunctionCall && instruction.size() > 3u && barriers.contains(instruction[3])))) {
                    report = std::string(op == spv::OpControlBarrier ? "an OpControlBarrier" : "a call reaching an OpControlBarrier") + " in block %" + std::to_string(label) + " of function %" + std::to_string(function.id) + " sits in control flow a per-invocation value decides";
                }
                const auto result = Result(instruction);
                if (result == 0u || NonUniform(result)) continue;
                bool varying = false;
                switch (op) {
                case spv::OpLoad: {
                    const auto variable = BaseVariable(instruction.at(3), chains);
                    const auto builtin = module.builtins.find(variable);
                    const auto storage = module.storageClasses.find(variable);
                    varying = NonUniform(instruction[3]) || (builtin != module.builtins.end() && VaryingBuiltin(builtin->second)) || storedNonUniform.contains(variable) ||
                        (storage != module.storageClasses.end() && storage->second == spv::StorageClassInput && builtin == module.builtins.end()) || Racy(instruction[3], fencedBefore);
                    break;
                }
                case spv::OpAtomicLoad:
                    varying = NonUniform(instruction.at(3)) || Racy(instruction[3], fencedBefore);
                    break;
                case spv::OpPhi:
                    varying = inDivergentFlow || divergentMerges.contains(label);
                    for (std::size_t operand = 3u; !varying && operand < instruction.size(); operand += 2u) varying = NonUniform(instruction[operand]);
                    break;
                case spv::OpFunctionCall:
                    varying = instruction.size() > 3u && varyingFunctions.contains(instruction[3]);
                    for (std::size_t operand = 4u; !varying && operand < instruction.size(); ++operand) varying = NonUniform(instruction[operand]);
                    break;
                default:
                    varying = PerInvocationResult(op) || (ImageRead(op) && Written(spv::StorageClassImage));
                    if (!varying) VisitIdOperands(instruction, 1u, [&](std::size_t operand) { varying = varying || NonUniform(instruction[operand]); });
                    break;
                }
                if (!varying) continue;
                nonUniform.insert(result);
                if (op != spv::OpFunctionCall) varyingFunctions.insert(function.id);
            }
        }
    }

    static constexpr std::uint32_t UnknownClass = 0xffffffffu;

    const Module& module;
    std::unordered_map<std::uint32_t, std::uint32_t> types;
    std::unordered_set<std::uint32_t> written;
    std::unordered_set<std::uint32_t> barriers;
    std::unordered_set<std::uint32_t> nonUniform;
    std::unordered_set<std::uint32_t> storedNonUniform;
    std::unordered_set<std::uint32_t> varyingFunctions;
    std::optional<std::string> report;
};

}

std::optional<std::string> SpannedBarrierInDivergentFlow(std::span<const std::uint32_t> words) {
    bool barrier = false;
    for (std::size_t cursor = 5u; !barrier && cursor < words.size(); cursor += std::max(words[cursor] >> 16u, 1u)) barrier = (words[cursor] & 0xffffu) == spv::OpControlBarrier;
    if (!barrier) return std::nullopt;
    const auto module = Parse(words);
    return Analysis(module).Run();
}

}
