#include "prx/libSceVideoOut/include/PresentLoopTrace.hpp"

#include <cstdio>
#include <cstring>
#include <string>

namespace {

bool check(bool condition, const char* reason) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", reason);
        return false;
    }
    return true;
}

}  // namespace

int main() {
    char line[512];

    // Disabled: nothing is emitted and nothing is counted.
    VideoOutTrace::PresentLoopTrace off(false);
    off.NoteWait(1000);
    off.NotePresent(2000);
    off.NoteFrame();
    if (!check(!off.Report(0, line, sizeof line), "disabled trace reported at 0")) return 1;
    if (!check(!off.Report(10000000000ull, line, sizeof line), "disabled trace reported later")) return 1;

    // Enabled: the first sample only opens the window; the second closes it after 3 s.
    VideoOutTrace::PresentLoopTrace on(true);
    on.NoteWait(1000000ull);
    on.NoteWait(3000000ull);
    on.NotePresent(10000000ull);
    on.NotePresent(20000000ull);
    on.NoteFrame();
    on.NoteFrame();
    if (!check(!on.Report(0, line, sizeof line), "the first sample must only open the window")) return 1;
    if (!check(!on.Report(2000000000ull, line, sizeof line), "2 s must not close the window")) return 1;
    if (!check(on.Report(3000000000ull, line, sizeof line), "3 s must close the window")) return 1;

    const std::string text(line);
    if (!check(text.rfind("[present-loop 0.000..3.000 s]", 0) == 0, "unexpected window label")) return 1;
    if (!check(text.find("frames 2") != std::string::npos, "frame count")) return 1;
    if (!check(text.find("flip-wait x2 4.0ms avg 2.00 max 3.00") != std::string::npos, "flip-wait aggregate")) return 1;
    if (!check(text.find("present x2 30.0ms avg 15.00 max 20.00") != std::string::npos, "present aggregate")) return 1;

    // Counters reset after the report: the next window starts empty.
    if (!check(!on.Report(4000000000ull, line, sizeof line), "an empty new window must not report")) return 1;
    if (!check(on.Report(6000000000ull, line, sizeof line), "the new window must report after 3 s")) return 1;
    if (!check(std::string(line).find("frames 0") != std::string::npos, "counters must reset")) return 1;

    std::printf("present loop trace test passed\n");
    return 0;
}
