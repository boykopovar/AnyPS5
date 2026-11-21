#include "prx/libkernel/Time/include/SleepWake.hpp"

#include <windows.h>
#include <cstdio>

int main() {
    constexpr auto wakeable = SleepWake::MAX_WAKEABLE_NANOS;
    SleepWake::Sleep(wakeable);
    bool nestedReturned = false;
    if (!QueueUserAPC([](ULONG_PTR argument) {
            SleepWake::Sleep(2 * SleepWake::MAX_WAKEABLE_NANOS);
            *reinterpret_cast<bool*>(argument) = true;
        }, GetCurrentThread(), reinterpret_cast<ULONG_PTR>(&nestedReturned))) {
        std::fputs("could not queue the nested sleep\n", stderr);
        return 1;
    }
    SleepWake::Sleep(wakeable);
    if (!nestedReturned) {
        std::fputs("the nested sleep did not run\n", stderr);
        return 1;
    }
    return 0;
}
