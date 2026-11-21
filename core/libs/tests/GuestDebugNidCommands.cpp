#include "prx/libc/include/DebugNidBreakpoints.hpp"
#include "prx/libc/include/DebugNidCommands.hpp"
#include "prx/libc/include/DebugNidStops.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#ifdef _WIN32
#include <stdlib.h>
#endif

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "DebugNidCommands check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

namespace {

std::uint64_t NidOf(const char* text) {
    std::uint64_t nid = 0;
    Require(Debug::ParseNid(text, nid), __LINE__);
    return nid;
}

void CheckBlankLines() {
    Check(Debug::ParseCommand("").kind == Debug::CommandKind::Empty);
    Check(Debug::ParseCommand("   \t ").kind == Debug::CommandKind::Empty);
    Check(Debug::ParseCommand("\r").kind == Debug::CommandKind::Empty);
}

void CheckNoArgumentCommands() {
    Check(Debug::ParseCommand("list").kind == Debug::CommandKind::List);
    Check(Debug::ParseCommand("  list\r").kind == Debug::CommandKind::List);
    Check(Debug::ParseCommand("args").kind == Debug::CommandKind::Args);
    Check(Debug::ParseCommand("continue").kind == Debug::CommandKind::Continue);
    Check(Debug::ParseCommand("\tcontinue \t").kind == Debug::CommandKind::Continue);
    Check(Debug::ParseCommand("list extra").kind == Debug::CommandKind::BadArguments);
    Check(Debug::ParseCommand("args 1").kind == Debug::CommandKind::BadArguments);
    Check(Debug::ParseCommand("continue now").kind == Debug::CommandKind::BadArguments);
    Check(Debug::ParseCommand("continue 1 2 3").kind == Debug::CommandKind::BadArguments);
}

void CheckNidCommands() {
    const Debug::Command added = Debug::ParseCommand("add 4J2sUJmuHZQ");
    Check(added.kind == Debug::CommandKind::Add);
    Check(added.nid == NidOf("4J2sUJmuHZQ"));
    const Debug::Command removed = Debug::ParseCommand("remove hcuQgD53UxM");
    Check(removed.kind == Debug::CommandKind::Remove);
    Check(removed.nid == NidOf("hcuQgD53UxM"));
    Check(Debug::ParseCommand("add").kind == Debug::CommandKind::BadArguments);
    Check(Debug::ParseCommand("remove").kind == Debug::CommandKind::BadArguments);
    Check(Debug::ParseCommand("add 4J2sUJmuHZQ extra").kind == Debug::CommandKind::BadArguments);
    Check(Debug::ParseCommand("add zz").kind == Debug::CommandKind::MalformedNid);
    Check(Debug::ParseCommand("add AAAAAAAAAA!").kind == Debug::CommandKind::MalformedNid);
    Check(Debug::ParseCommand("remove AAAAAAAAAAB").kind == Debug::CommandKind::MalformedNid);
}

struct SessionRun {
    std::uint64_t id = 0;
    std::string output;
};

std::FILE* ScriptFile(const std::string& script) {
    std::FILE* file = std::tmpfile();
    Require(file != nullptr, __LINE__);
    Require(std::fwrite(script.data(), 1, script.size(), file) == script.size(), __LINE__);
    std::rewind(file);
    return file;
}

std::string ReadBack(std::FILE* file) {
    std::rewind(file);
    std::string text;
    char buffer[256];
    std::size_t count = 0;
    while ((count = std::fread(buffer, 1, sizeof buffer, file)) > 0)
        text.append(buffer, count);
    return text;
}

SessionRun RunSession(Debug::NidBreakpointTable& table, std::uint64_t nid, const std::string& script, const std::uint64_t* args, std::size_t argCount) {
    Debug::NidStopRegistry registry;
    std::FILE* input = ScriptFile(script);
    std::FILE* output = std::tmpfile();
    Require(output != nullptr, __LINE__);
    SessionRun run;
    run.id = registry.Hit(table, nid, args, argCount, [&](Debug::NidBreakpointTable& t, Debug::NidStopRegistry& r, const Debug::NidStopRegistry::Stop& s) {
        Debug::RunCommandSession(input, output, t, r, s);
    });
    Check(registry.Size() == 0);
    run.output = ReadBack(output);
    std::fclose(input);
    std::fclose(output);
    return run;
}

void CheckSessionCommands() {
    Debug::NidBreakpointTable table;
    const std::uint64_t armed = NidOf("4J2sUJmuHZQ");
    const std::uint64_t other = NidOf("hcuQgD53UxM");
    Check(table.Add(armed) == Debug::NidBreakpointTable::AddResult::Added);
    const std::uint64_t args[2] = {0x11, 0x22};
    const SessionRun run = RunSession(table, armed,
        "list\n"
        "add 4J2sUJmuHZQ\n"
        "add hcuQgD53UxM\n"
        "remove hcuQgD53UxM\n"
        "remove Q3VBxCXhUHs\n"
        "frob\n"
        "add zz\n"
        "add\n"
        "list extra\n"
        "\n"
        "args\n"
        "continue\n"
        "list\n",
        args, 2);
    Check(run.id == 1);
    Check(table.IsArmed(armed));
    Check(!table.IsArmed(other));
    const std::string expected =
        "armed 4J2sUJmuHZQ\n"
        "already armed 4J2sUJmuHZQ\n"
        "added hcuQgD53UxM\n"
        "removed hcuQgD53UxM\n"
        "not armed Q3VBxCXhUHs\n"
        "unknown command: frob\n"
        "malformed NID: add zz\n"
        "bad arguments: add\n"
        "bad arguments: list extra\n"
        "stop 1 nid 4J2sUJmuHZQ args 0x11 0x22\n"
        "continued 1\n";
    Check(run.output == expected);
}

void CheckSessionEndOfInputContinues() {
    Debug::NidBreakpointTable table;
    const std::uint64_t armed = NidOf("4J2sUJmuHZQ");
    Check(table.Add(armed) == Debug::NidBreakpointTable::AddResult::Added);
    const SessionRun run = RunSession(table, armed, "list\nadd Q3VBxCXhUHs", nullptr, 0);
    Check(run.id == 1);
    Check(table.IsArmed(NidOf("Q3VBxCXhUHs")));
    const std::string expected =
        "armed 4J2sUJmuHZQ\n"
        "added Q3VBxCXhUHs\n"
        "end of commands, continuing 1\n";
    Check(run.output == expected);
}

void CheckSessionLineEndingsAndLongLines() {
    Debug::NidBreakpointTable table;
    const std::uint64_t armed = NidOf("4J2sUJmuHZQ");
    Check(table.Add(armed) == Debug::NidBreakpointTable::AddResult::Added);
    const SessionRun crlf = RunSession(table, armed, "\tcontinue \t\r\n", nullptr, 0);
    Check(crlf.id == 1);
    Check(crlf.output == "continued 1\n");
    const SessionRun longLine = RunSession(table, armed, std::string(300, 'x') + "\nargs\ncontinue\n", nullptr, 0);
    Check(longLine.id == 1);
    Check(longLine.output == "line too long, ignored\nstop 1 nid 4J2sUJmuHZQ args\ncontinued 1\n");
}

void SetCommandFile(const char* path) {
#ifdef _WIN32
    _putenv_s("ANYPS5_DEBUG_CMD", path != nullptr ? path : "");
#else
    if (path != nullptr)
        setenv("ANYPS5_DEBUG_CMD", path, 1);
    else
        unsetenv("ANYPS5_DEBUG_CMD");
#endif
}

void CheckCommandFileFallbacks() {
    Debug::NidBreakpointTable table;
    Debug::NidStopRegistry registry;
    const std::uint64_t armed = NidOf("4J2sUJmuHZQ");
    Check(table.Add(armed) == Debug::NidBreakpointTable::AddResult::Added);

    SetCommandFile(nullptr);
    Check(registry.Hit(table, armed, nullptr, 0, Debug::CommandFileSession()) == 1);
    Check(registry.Size() == 0);

    SetCommandFile("debug_nid_commands_missing.txt");
    Check(registry.Hit(table, armed, nullptr, 0, Debug::CommandFileSession()) == 2);
    Check(registry.Size() == 0);

    const char* path = "debug_nid_commands_test.txt";
    std::FILE* file = std::fopen(path, "w");
    Require(file != nullptr, __LINE__);
    std::fputs("add Q3VBxCXhUHs\ncontinue\n", file);
    std::fclose(file);
    SetCommandFile(path);
    Check(registry.Hit(table, armed, nullptr, 0, Debug::CommandFileSession()) == 3);
    Check(registry.Size() == 0);
    Check(table.IsArmed(NidOf("Q3VBxCXhUHs")));
    std::remove(path);
    SetCommandFile(nullptr);
}

void CheckUnknownWords() {
    Check(Debug::ParseCommand("frob").kind == Debug::CommandKind::Unknown);
    Check(Debug::ParseCommand("frob 4J2sUJmuHZQ").kind == Debug::CommandKind::Unknown);
    Check(Debug::ParseCommand("LIST").kind == Debug::CommandKind::Unknown);
    Check(Debug::ParseCommand("Continue").kind == Debug::CommandKind::Unknown);
    Check(Debug::ParseCommand("step").kind == Debug::CommandKind::Unknown);
    Check(Debug::ParseCommand("listx").kind == Debug::CommandKind::Unknown);
}

}

int main() {
    CheckBlankLines();
    CheckNoArgumentCommands();
    CheckNidCommands();
    CheckUnknownWords();
    CheckSessionCommands();
    CheckSessionEndOfInputContinues();
    CheckSessionLineEndingsAndLongLines();
    CheckCommandFileFallbacks();
    return 0;
}
