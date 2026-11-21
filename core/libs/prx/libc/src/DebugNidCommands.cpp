#include "prx/libc/include/DebugNidCommands.hpp"

#include "prx/libc/include/DebugNidBreakpoints.hpp"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <string>

namespace {

constexpr std::size_t kMaxCommandLine = 256;

bool IsSeparator(char c) {
    return c == ' ' || c == '\t' || c == '\r';
}

enum class LineRead { End, Line, TooLong };

LineRead ReadLine(std::FILE* input, std::string& line) {
    line.clear();
    int ch = std::fgetc(input);
    if (ch == EOF)
        return LineRead::End;
    bool tooLong = false;
    while (ch != EOF && ch != '\n') {
        if (line.size() < kMaxCommandLine)
            line.push_back(static_cast<char>(ch));
        else
            tooLong = true;
        ch = std::fgetc(input);
    }
    return tooLong ? LineRead::TooLong : LineRead::Line;
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

namespace {

void PrintArmed(std::FILE* output, NidBreakpointTable& table) {
    std::array<std::uint64_t, NidBreakpointTable::kCapacity> nids{};
    const std::size_t listed = table.List(nids.data(), nids.size());
    for (std::size_t i = 0; i < listed; ++i)
        std::fprintf(output, "armed %s\n", FormatNid(nids[i]).c_str());
}

void PrintAdd(std::FILE* output, NidBreakpointTable& table, std::uint64_t nid) {
    const std::string text = FormatNid(nid);
    switch (table.Add(nid)) {
    case NidBreakpointTable::AddResult::Added:
        std::fprintf(output, "added %s\n", text.c_str());
        break;
    case NidBreakpointTable::AddResult::Duplicate:
        std::fprintf(output, "already armed %s\n", text.c_str());
        break;
    case NidBreakpointTable::AddResult::Full:
        std::fprintf(output, "table full, %s not added\n", text.c_str());
        break;
    }
}

void PrintRemove(std::FILE* output, NidBreakpointTable& table, std::uint64_t nid) {
    std::fprintf(output, table.Remove(nid) ? "removed %s\n" : "not armed %s\n", FormatNid(nid).c_str());
}

void PrintStop(std::FILE* output, const NidStopRegistry::Stop& stop) {
    std::fprintf(output, "stop %llu nid %s args", static_cast<unsigned long long>(stop.id), FormatNid(stop.nid).c_str());
    for (std::size_t i = 0; i < stop.argCount; ++i)
        std::fprintf(output, " 0x%llx", static_cast<unsigned long long>(stop.args[i]));
    std::fputc('\n', output);
}

}

void RunCommandSession(std::FILE* input, std::FILE* output, NidBreakpointTable& table, NidStopRegistry& registry, const NidStopRegistry::Stop& stop) {
    std::string line;
    for (;;) {
        const LineRead read = ReadLine(input, line);
        if (read == LineRead::End)
            break;
        if (read == LineRead::TooLong) {
            std::fprintf(output, "line too long, ignored\n");
            continue;
        }
        const Command command = ParseCommand(line);
        switch (command.kind) {
        case CommandKind::Empty:
            break;
        case CommandKind::List:
            PrintArmed(output, table);
            break;
        case CommandKind::Add:
            PrintAdd(output, table, command.nid);
            break;
        case CommandKind::Remove:
            PrintRemove(output, table, command.nid);
            break;
        case CommandKind::Args:
            PrintStop(output, stop);
            break;
        case CommandKind::Continue:
            std::fprintf(output, "continued %llu\n", static_cast<unsigned long long>(stop.id));
            registry.Continue(stop.id);
            return;
        case CommandKind::Unknown:
            std::fprintf(output, "unknown command: %s\n", line.c_str());
            break;
        case CommandKind::MalformedNid:
            std::fprintf(output, "malformed NID: %s\n", line.c_str());
            break;
        case CommandKind::BadArguments:
            std::fprintf(output, "bad arguments: %s\n", line.c_str());
            break;
        }
    }
    std::fprintf(output, "end of commands, continuing %llu\n", static_cast<unsigned long long>(stop.id));
    registry.Continue(stop.id);
}

NidStopRegistry::Session CommandFileSession() {
    return [](NidBreakpointTable& table, NidStopRegistry& registry, const NidStopRegistry::Stop& stop) {
        const char* path = std::getenv("ANYPS5_DEBUG_CMD");
        if (path == nullptr || *path == '\0') {
            registry.Continue(stop.id);
            return;
        }
        std::FILE* input = std::fopen(path, "r");
        if (input == nullptr) {
            std::fprintf(stderr, "ANYPS5_DEBUG_CMD: cannot open '%s', continuing %llu\n", path, static_cast<unsigned long long>(stop.id));
            registry.Continue(stop.id);
            return;
        }
        RunCommandSession(input, stderr, table, registry, stop);
        std::fclose(input);
    };
}

}
