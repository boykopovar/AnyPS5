#ifndef PS5LIFT_COMPILE_HPP
#define PS5LIFT_COMPILE_HPP

#include <lifter/Codegen.hpp>
#include <lifter/Discovery.hpp>
#include <lifter/Lifter.hpp>
#include <lifter/Program.hpp>

#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace Ps5Lift {

struct CompileOptions {
    unsigned Jobs = 1;
    std::string Archive;
    std::string IrPrefix;
};

LiftReport CompileProgram(const GuestProgram& program, const std::vector<std::map<std::uint64_t, GuestFunction>>& functions, const LiftOptions& lift, const Target& target,
                          const CompileOptions& options);

}

#endif
