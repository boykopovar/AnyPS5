#ifndef CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTPLANBUILDER_HPP
#define CORE_SHADER_RECOMPILIER_OPTIMIZATION_SRTWALKER_SRTPLANBUILDER_HPP

#include "IntermediateRepresentation/IrProgram.hpp"

#include <cstdint>
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
    void PatchReads();

    IrProgram& _program;
    std::vector<IrValue*> _visiting;
    std::unordered_set<IrValue*> _visitingSet;
    std::unordered_set<IrValue*> _visited;
    std::vector<Patch> _patches;
};

}

#endif
