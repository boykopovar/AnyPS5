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

int main() {
    CheckParseKnownValues();
    CheckParseRejectsMalformed();
    CheckRoundTrip();
    return 0;
}
