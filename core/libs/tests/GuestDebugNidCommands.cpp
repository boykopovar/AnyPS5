#include "prx/libc/include/DebugNidBreakpoints.hpp"
#include "prx/libc/include/DebugNidCommands.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string_view>

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
    return 0;
}
