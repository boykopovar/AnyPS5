#include "prx/libSceVideoOut/include/SubmitFlipTrace.hpp"

#include <cstdio>
#include <cstring>
#include <string>
#ifdef _WIN32
#include <cstdlib>
#endif

namespace {

unsigned long gFocusLast = 0;
int gFocusCalls = 0;

bool check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", reason);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    char line[1024];

    // Default off: an instance built without the variable counts nothing and reports nothing.
    {
        VideoOutTrace::SubmitFlipTrace off;
        if (!check(!off.Enabled(), "the probe must be off by default")) return 1;
        off.Note(1, 100, 0, 500000, 3, true);
        if (!check(!off.Report(5000000000ull, line, sizeof line), "off probe must not report")) return 1;
    }

    // Enabled through the variable the production class reads in its constructor.
#ifdef _WIN32
    if (!check(_putenv_s("APS5_TRACE_SUBMIT_FLIP", "1") == 0, "env")) return 1;
#else
    if (!check(setenv("APS5_TRACE_SUBMIT_FLIP", "1", 1) == 0, "env")) return 1;
#endif
    VideoOutTrace::SubmitFlipTrace on;
    if (!check(on.Enabled(), "the probe must be enabled by the variable")) return 1;

    on.Note(1, 100, 0, 2000000, 3, true);                 // no interval yet
    on.Note(1, 100, 40000000, 42000000, 4, true);          // 40 ms
    on.Note(1, 100, 90000000, 99000000, 5, true);          // 50 ms
    on.Note(2, 200, 100000000, 101000000, 1, true);        // other key, no interval
    on.Note(1, 100, 200000000, 203000000, 32, false);      // 110 ms, rejected, queue 32

    if (!check(!on.Report(0ull, line, sizeof line), "first report only opens the window")) return 1;
    if (!check(!on.Report(2000000000ull, line, sizeof line), "2 s must not close the window")) return 1;
    if (!check(on.Report(3000000000ull, line, sizeof line), "3 s must close the window")) return 1;

    const std::string text(line);
    if (!check(text.rfind("[submit-flip 0.000..3.000 s]", 0) == 0, text.c_str())) return 1;
    if (!check(text.find("h1 tid 100 x4 ok3 err1") != std::string::npos, "h1 counts")) return 1;
    if (!check(text.find("h2 tid 200 x1 ok1 err0") != std::string::npos, "h2 counts")) return 1;
    if (!check(text.find("interval avg 66.67 max 110.00 ms") != std::string::npos, "h1 intervals")) return 1;
    if (!check(text.find("call avg 4.000 max 9.000 ms") != std::string::npos, "h1 durations")) return 1;
    if (!check(text.find("queue max 32") != std::string::npos, "depth")) return 1;

    // Counters reset: an empty window produces no line.
    if (!check(!on.Report(5500000000ull, line, sizeof line), "an empty window must not report")) return 1;

    // Dynamic bounded selection: twelve distinct keys, one submission each. Only the eight busiest
    // pairs are printed, and the remaining four fold into a single aggregate line tagged h-1.
    VideoOutTrace::SubmitFlipTrace many;
    for (int key = 0; key < 12; ++key) many.Note(key + 1, 300 + static_cast<unsigned long>(key), static_cast<std::uint64_t>(key) * 10000000ull,
                                                static_cast<std::uint64_t>(key) * 10000000ull + 1000000ull, 1, true);
    if (!check(!many.Report(1000000000ull, line, sizeof line), "many: open window")) return 1;
    if (!check(many.Report(5000000000ull, line, sizeof line), "many: close window")) return 1;
    const std::string manyText(line);
    std::size_t entries = 0;
    for (std::size_t at = manyText.find(" | h"); at != std::string::npos; at = manyText.find(" | h", at + 1)) ++entries;
    if (!check(entries == 9, "the report must stay bounded to eight keys plus one folded line")) return 1;
    if (!check(manyText.find("| h-1 tid 0 x4 ok4 err0") != std::string::npos, "the four extra keys must fold into the h-1 line")) return 1;

    // Dynamic selection by activity: ten keys with counts 10..1. The eight busiest are printed, the
    // two quietest fold, so a transient thread cannot push the dominant one out of the report.
    VideoOutTrace::SubmitFlipTrace ranked;
    for (int key = 0; key < 10; ++key) {
        const int hits = 10 - key;
        for (int n = 0; n < hits; ++n) {
            const std::uint64_t t = static_cast<std::uint64_t>((key * 16 + n) * 5000000ull);
            ranked.Note(1, 500 + static_cast<unsigned long>(key), t, t + 1000000ull, 1, true);
        }
    }
    if (!check(!ranked.Report(0ull, line, sizeof line), "ranked: open window")) return 1;
    if (!check(ranked.Report(3000000000ull, line, sizeof line), "ranked: close window")) return 1;
    const std::string rankedText(line);
    if (!check(rankedText.find("h1 tid 500 x10") != std::string::npos, "the busiest pair must be reported")) return 1;
    if (!check(rankedText.find("h1 tid 509 x1") == std::string::npos, "the quietest pair must be folded")) return 1;
    if (!check(rankedText.find("| h-1 tid 0 x3 ") != std::string::npos, "the two quietest pairs fold with exact sums")) return 1;

    // A different run has a different submitter TID: a fresh trace carries no stale key, and the
    // printed identity is the one seen in this window.
    VideoOutTrace::SubmitFlipTrace otherRun;
    otherRun.Note(1, 777, 0, 2000000ull, 0, true);
    otherRun.Note(1, 777, 45000000ull, 47000000ull, 0, true);
    if (!check(!otherRun.Report(0ull, line, sizeof line), "other run: open window")) return 1;
    if (!check(otherRun.Report(3000000000ull, line, sizeof line), "other run: close window")) return 1;
    const std::string otherText(line);
    if (!check(otherText.find("h1 tid 777 x2 ok2 err0") != std::string::npos, "the new TID must be the reported one")) return 1;
    if (!check(otherText.find("tid 100") == std::string::npos, "no key may leak from another run")) return 1;

    // Hard cap: forty distinct pairs. The table keeps 32, the aggregate adds the eight over the cap,
    // and the folded line carries the exact total (24 folded slots + 8 over-cap = 32).
    VideoOutTrace::SubmitFlipTrace capped;
    for (int key = 0; key < 40; ++key) capped.Note(1, 900 + static_cast<unsigned long>(key), static_cast<std::uint64_t>(key) * 1000000ull,
                                                   static_cast<std::uint64_t>(key) * 1000000ull + 1000ull, 0, true);
    if (!check(!capped.Report(0ull, line, sizeof line), "capped: open window")) return 1;
    if (!check(capped.Report(3000000000ull, line, sizeof line), "capped: close window")) return 1;
    const std::string cappedText(line);
    std::size_t cappedEntries = 0;
    for (std::size_t at = cappedText.find(" | h"); at != std::string::npos; at = cappedText.find(" | h", at + 1)) ++cappedEntries;
    if (!check(cappedEntries == 9, "the capped report stays bounded to eight keys plus the folded line")) return 1;
    if (!check(cappedText.find("| h-1 tid 0 x32 ok32 err0") != std::string::npos, "the cap must not lose submissions")) return 1;

    // Room-wait aggregate: single bucket, one line per window, bounded output.
    VideoOutTrace::RoomWaitTrace room;
    if (!check(room.Enabled(), "flip-room probe must follow the same opt-in variable")) return 1;
    room.Note(0, 250000);            // 0.25 ms
    room.Note(1000000, 3500000);     // 2.5 ms, counts as over 1 ms
    if (!check(!room.Report(0ull, line, sizeof line), "flip-room: open window")) return 1;
    if (!check(!room.Report(1000000000ull, line, sizeof line), "flip-room: 1 s must not close the window")) return 1;
    if (!check(room.Report(4000000000ull, line, sizeof line), "flip-room: 4 s must close the window")) return 1;
    const std::string roomText(line);
    if (!check(roomText.rfind("[flip-room 0.000..4.000 s]", 0) == 0, roomText.c_str())) return 1;
    if (!check(roomText.find("wait x2 total 2.8ms avg 1.375 max 2.500 ms over1ms 1") != std::string::npos, roomText.c_str())) return 1;

    // Focus sink: the probe publishes the busiest submitting thread once per change, so libkernel can
    // narrow its waits report to it; a thread that only appears later takes the focus over.
    {
        unsigned long lastFocus = 0;
        int focusCalls = 0;
        VideoOutTrace::SubmitFlipTrace focusTrace;        focusTrace.SetFocusSink([](unsigned long thread) {
            gFocusLast = thread;
            ++gFocusCalls;
        });
        focusTrace.Note(1, 100, 0, 1000000ull, 0, true);
        if (!check(gFocusLast == 100 && gFocusCalls == 1, "the first submission must publish its thread")) return 1;
        focusTrace.Note(1, 100, 40000000ull, 41000000ull, 0, true);
        if (!check(gFocusCalls == 1, "the same thread must not republish the focus")) return 1;
        focusTrace.Note(1, 200, 80000000ull, 81000000ull, 0, true);
        if (!check(gFocusLast == 200 && gFocusCalls == 2, "a new submitting thread must take the focus")) return 1;
        lastFocus = gFocusLast;
        if (!check(!focusTrace.Report(0ull, line, sizeof line), "focus: open window")) return 1;
        if (!check(focusTrace.Report(3000000000ull, line, sizeof line), "focus: close window")) return 1;
        // Closing a window re-derives the focus from the busiest thread of that window: tid 100 has two
        // submissions against tid 200's one, so the focus must move back to it.
        if (!check(gFocusLast == 100, "closing a window must re-focus the busiest thread")) return 1;
        if (!check(gFocusCalls == 3, "the re-focus must be published once")) return 1;
        if (!check(std::string(line).find("h1 tid 100") != std::string::npos, "the busiest thread must be reported")) return 1;
    }

    std::printf("submit flip trace test passed\n");
    return 0;
}
