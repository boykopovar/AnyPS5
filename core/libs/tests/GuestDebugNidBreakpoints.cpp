#include "prx/libc/include/DebugNidBreakpoints.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>

static void Require(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "DebugNidBreakpoints check failed at line %d\n", line);
        std::abort();
    }
}
#define Check(value) Require((value), __LINE__)

static void CheckParseKnownValues() {
    std::uint64_t nid = 1;
    Check(Debug::ParseNid("AAAAAAAAAAA", nid));
    Check(nid == 0);
    Check(Debug::ParseNid("AAAAAAAAAAE", nid));
    Check(nid == 1);
    Check(Debug::ParseNid("AAAAAAAAAAI", nid));
    Check(nid == 2);
}

static void CheckParseRejectsMalformed() {
    std::uint64_t nid = 0;
    Check(!Debug::ParseNid("", nid));
    Check(!Debug::ParseNid("AAAAAAAAAA", nid));
    Check(!Debug::ParseNid("AAAAAAAAAAAA", nid));
    Check(!Debug::ParseNid("AAAAAAAAAA!", nid));
    Check(!Debug::ParseNid("AAAAAAAAAAB", nid));
    Check(!Debug::ParseNid(" AAAAAAAAAA", nid));
}

static void CheckRoundTrip() {
    std::uint64_t state = 0x9E3779B97F4A7C15ULL;
    for (int i = 0; i < 4096; ++i) {
        state = state * 6364136223846793005ULL + 1442695040888963407ULL;
        const std::uint64_t value = state & ~3ULL;
        const std::string text = Debug::FormatNid(value);
        Check(text.size() == 11);
        std::uint64_t parsed = 0;
        Check(Debug::ParseNid(text, parsed));
        Check(parsed == value);
        Check(Debug::FormatNid(parsed) == text);
    }
}

static void CheckAddRemove() {
    Debug::NidBreakpointTable table;
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    Check(Debug::ParseNid("4J2sUJmuHZQ", a));
    Check(Debug::ParseNid("hcuQgD53UxM", b));
    Check(!table.Remove(a));
    Check(table.Add(a));
    Check(table.Add(b));
    Check(table.Remove(a));
    Check(!table.Remove(a));
    Check(table.Remove(b));
    Check(!table.Remove(b));
}

int main() {
    CheckParseKnownValues();
    CheckParseRejectsMalformed();
    CheckRoundTrip();
    CheckAddRemove();
    return 0;
}
