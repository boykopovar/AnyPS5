#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTPLANBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTPLANBUILDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler::Detail {

class PlanBuilder {
public:
    explicit PlanBuilder(IrProgram& program) : _program(program) {}

    void Run();

private:
    struct Patch {
        IrValue* inst = nullptr;
        std::uint32_t slot = 0;
        bool keep = false;
    };

    void Collect(IrValue* raw, std::uint32_t usePc);
    void Record(IrValue* inst);
    void PatchReads();

    IrProgram& _program;
    std::vector<IrValue*> _visiting;
    std::unordered_map<IrValue*, std::size_t> _visitingIndex;
    std::unordered_set<IrValue*> _visited;
    std::vector<Patch> _patches;
};

}

#endif
