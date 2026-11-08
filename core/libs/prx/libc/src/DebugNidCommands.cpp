#include "prx/libc/include/DebugNidCommands.hpp"

#include "prx/libc/include/DebugNidBreakpoints.hpp"

#include <array>
#include <cstddef>

namespace {

bool IsSeparator(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

}

namespace Debug {

Command ParseCommand(std::string_view line) {
    std::array<std::string_view, 2> words{};
    std::size_t count = 0;
    std::size_t pos = 0;
    while (pos < line.size()) {
        while (pos < line.size() && IsSeparator(line[pos]))
            ++pos;
        if (pos == line.size())
            break;
        const std::size_t start = pos;
        while (pos < line.size() && !IsSeparator(line[pos]))
            ++pos;
        if (count < words.size())
            words[count] = line.substr(start, pos - start);
        ++count;
    }

    if (count == 0)
        return Command{CommandKind::Empty, 0};

    const std::string_view command = words[0];
    if (command == "list")
        return Command{count == 1 ? CommandKind::List : CommandKind::BadArguments, 0};
    if (command == "args")
        return Command{count == 1 ? CommandKind::Args : CommandKind::BadArguments, 0};
    if (command == "continue")
        return Command{count == 1 ? CommandKind::Continue : CommandKind::BadArguments, 0};
    if (command == "add" || command == "remove") {
        if (count != 2)
            return Command{CommandKind::BadArguments, 0};
        std::uint64_t nid = 0;
        if (!ParseNid(words[1], nid))
            return Command{CommandKind::MalformedNid, 0};
        return Command{command == "add" ? CommandKind::Add : CommandKind::Remove, nid};
    }
    return Command{CommandKind::Unknown, 0};
}

}
