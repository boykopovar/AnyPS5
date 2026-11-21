#include "prx/libc/include/DebugNidBreakpoints.hpp"

#include <cstdlib>
#ifdef _WIN32
#include <stdlib.h>
#endif

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>

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

static std::string reportedEntries;

static void RecordMalformed(std::string_view entry) {
    reportedEntries.append(entry);
    reportedEntries.push_back('|');
}

static void CheckMalformedReport() {
    Debug::NidBreakpointTable table;
    reportedEntries.clear();
    Check(Debug::AddNidList("4J2sUJmuHZQ,AAAAAAAAAA!, bad ,AAAAAAAAAAB,hcuQgD53UxM", table, RecordMalformed) == 2);
    Check(reportedEntries == "AAAAAAAAAA!|bad|AAAAAAAAAAB|");
    Check(table.Size() == 2);
    reportedEntries.clear();
    Check(Debug::AddNidList("4J2sUJmuHZQ", table, RecordMalformed) == 0);
    Check(reportedEntries.empty());
    Check(Debug::AddNidList("AAAAAAAAAAE,", table, nullptr) == 1);
}

static void CheckEnvironmentList() {
    Debug::NidBreakpointTable table;
    Check(Debug::AddNidList("", table, nullptr) == 0);
    Check(Debug::AddNidList(",,", table, nullptr) == 0);
    Check(table.Empty());
    Check(Debug::AddNidList(" 4J2sUJmuHZQ , hcuQgD53UxM,Q3VBxCXhUHs ", table, nullptr) == 3);
    Check(table.Size() == 3);
    Check(table.IsArmed(0) == false);
    std::uint64_t nid = 0;
    Check(Debug::ParseNid("hcuQgD53UxM", nid));
    Check(table.IsArmed(nid));
    Check(Debug::AddNidList("4J2sUJmuHZQ,AAAAAAAAAA!,hcuQgD53UxM", table, nullptr) == 0);
    Check(table.Size() == 3);
    Check(table.DuplicateCount() == 2);
}

static void CheckList() {
    Debug::NidBreakpointTable table;
    std::uint64_t listed[4] = {};
    Check(table.List(listed, 4) == 0);
    Check(Debug::AddNidList("4J2sUJmuHZQ,hcuQgD53UxM,Q3VBxCXhUHs", table, nullptr) == 3);
    Check(table.List(listed, 4) == 3);
    std::uint64_t a = 0;
    std::uint64_t b = 0;
    std::uint64_t c = 0;
    Check(Debug::ParseNid("4J2sUJmuHZQ", a) && Debug::ParseNid("hcuQgD53UxM", b) && Debug::ParseNid("Q3VBxCXhUHs", c));
    Check(listed[0] == a && listed[1] == b && listed[2] == c);
    Check(table.List(listed, 2) == 2);
    Check(listed[0] == a && listed[1] == b);
}

static void CheckUnsetLeavesTableEmpty() {
#ifdef _WIN32
    _putenv_s("ANYPS5_DEBUG_NIDS", "");
#else
    unsetenv("ANYPS5_DEBUG_NIDS");
#endif
    Debug::LoadNidBreakpointsFromEnvironment();
    Check(Debug::NidBreakpoints().Empty());
    Check(Debug::NidBreakpoints().Size() == 0);
    Check(Debug::NidBreakpoints().DuplicateCount() == 0);
    std::uint64_t probe = 0;
    Check(Debug::ParseNid("4J2sUJmuHZQ", probe));
    Check(!Debug::NidBreakpoints().IsArmed(probe));
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
    CheckMalformedReport();
    CheckList();
    CheckUnsetLeavesTableEmpty();
    return 0;
}
