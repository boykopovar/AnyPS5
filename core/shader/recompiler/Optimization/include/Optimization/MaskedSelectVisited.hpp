#ifndef CORE_SHADER_RECOMPILER_OPTIMIZATION_INCLUDE_OPTIMIZATION_MASKEDSELECTVISITED_HPP
#define CORE_SHADER_RECOMPILER_OPTIMIZATION_INCLUDE_OPTIMIZATION_MASKEDSELECTVISITED_HPP

#include "IntermediateRepresentation/IrProgram.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace ShaderRecompiler {

template<typename Generation = std::uint32_t>
class MaskedSelectVisited {
    static_assert(std::is_integral_v<Generation> && std::is_unsigned_v<Generation> && !std::is_same_v<Generation, bool>);

public:
    explicit MaskedSelectVisited(const IrProgram& program) : program(program), marks(program.Values().size()) {}

    void Begin() {
        generation = static_cast<Generation>(generation + 1u);
        if (generation == 0) {
            std::fill(marks.begin(), marks.end(), Generation{});
            generation = 1;
        }
        others.clear();
        count = 0;
    }

    bool Insert(const IrValue* value) {
        const auto id = static_cast<std::size_t>(value->Id());
        if (id < marks.size() && program.Values()[id].get() == value) {
            if (marks[id] == generation) return false;
            marks[id] = generation;
        } else if (!others.insert(value).second) {
            return false;
        }
        ++count;
        return true;
    }

    [[nodiscard]] std::size_t Size() const {
        return count;
    }

private:
    const IrProgram& program;
    std::vector<Generation> marks;
    std::unordered_set<const IrValue*> others;
    Generation generation = 0;
    std::size_t count = 0;
};

}

#endif
