#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDCOMMANDS_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_DEBUGNIDCOMMANDS_HPP

#include "prx/libc/include/DebugNidBreakpoints.hpp"
#include "prx/libc/include/DebugNidStops.hpp"

#include <cstdint>
#include <cstdio>
#include <string_view>

namespace Debug {

enum class CommandKind {
    Empty,
    List,
    Add,
    Remove,
    Args,
    Continue,
    Unknown,
    MalformedNid,
    BadArguments,
};

struct Command {
    CommandKind kind = CommandKind::Empty;
    std::uint64_t nid = 0;
};

Command ParseCommand(std::string_view line);

void RunCommandSession(std::FILE* input, std::FILE* output, NidBreakpointTable& table, NidStopRegistry& registry, const NidStopRegistry::Stop& stop);

NidStopRegistry::Session CommandFileSession();

}

#endif
