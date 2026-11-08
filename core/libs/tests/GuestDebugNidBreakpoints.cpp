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
    Check(table.Add(a) == Debug::NidBreakpointTable::AddResult::Added);
    Check(table.Add(b) == Debug::NidBreakpointTable::AddResult::Added);
    Check(table.Remove(a));
    Check(!table.Remove(a));
    Check(table.Remove(b));
    Check(!table.Remove(b));
}

static void CheckDuplicates() {
    Debug::NidBreakpointTable table;
    std::uint64_t a = 0;
    Check(Debug::ParseNid("4J2sUJmuHZQ", a));
    Check(table.Add(a) == Debug::NidBreakpointTable::AddResult::Added);
    Check(table.Add(a) == Debug::NidBreakpointTable::AddResult::Duplicate);
    Check(table.Add(a) == Debug::NidBreakpointTable::AddResult::Duplicate);
    Check(table.Size() == 1);
    Check(table.DuplicateCount() == 2);
    Check(table.Remove(a));
    Check(table.Add(a) == Debug::NidBreakpointTable::AddResult::Added);
    Check(table.DuplicateCount() == 2);
}

static void CheckCapacity() {
    Debug::NidBreakpointTable table;
    for (std::uint64_t i = 0; i < Debug::NidBreakpointTable::kCapacity; ++i)
        Check(table.Add(i << 2) == Debug::NidBreakpointTable::AddResult::Added);
    Check(table.Size() == Debug::NidBreakpointTable::kCapacity);
    Check(table.Add(0xFFFFFFFFFFFFFFFCULL) == Debug::NidBreakpointTable::AddResult::Full);
    Check(table.Size() == Debug::NidBreakpointTable::kCapacity);
    Check(table.Add(0) == Debug::NidBreakpointTable::AddResult::Duplicate);
    Check(table.Remove(4));
    Check(table.Add(0xFFFFFFFFFFFFFFFCULL) == Debug::NidBreakpointTable::AddResult::Added);
    Check(table.Size() == Debug::NidBreakpointTable::kCapacity);
}

static void CheckArmedLockFree() {
    Debug::NidBreakpointTable table;
    std::uint64_t a = 0;
    Check(Debug::ParseNid("4J2sUJmuHZQ", a));
    Check(table.Empty());
    Check(!table.IsArmed(a));
    Check(table.Add(a) == Debug::NidBreakpointTable::AddResult::Added);
    Check(!table.Empty());
    Check(table.IsArmed(a));
    Check(!table.IsArmed(a + 4));
    Check(table.Remove(a));
    Check(table.Empty());
    Check(!table.IsArmed(a));
}

static void CheckEnvironmentList() {
    Debug::NidBreakpointTable table;
    Check(Debug::AddNidList("", table) == 0);
    Check(Debug::AddNidList(",,", table) == 0);
    Check(table.Empty());
    Check(Debug::AddNidList(" 4J2sUJmuHZQ , hcuQgD53UxM,Q3VBxCXhUHs ", table) == 3);
    Check(table.Size() == 3);
    Check(table.IsArmed(0) == false);
    std::uint64_t nid = 0;
    Check(Debug::ParseNid("hcuQgD53UxM", nid));
    Check(table.IsArmed(nid));
    Check(Debug::AddNidList("4J2sUJmuHZQ,AAAAAAAAAA!,hcuQgD53UxM", table) == 0);
    Check(table.Size() == 3);
    Check(table.DuplicateCount() == 2);
}

int main() {
    CheckParseKnownValues();
    CheckParseRejectsMalformed();
    CheckRoundTrip();
    CheckAddRemove();
    CheckDuplicates();
    CheckCapacity();
    CheckArmedLockFree();
    CheckEnvironmentList();
    return 0;
}
