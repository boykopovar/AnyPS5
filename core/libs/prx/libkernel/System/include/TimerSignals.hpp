#ifndef CORE_LIBS_PRX_LIBKERNEL_SYSTEM_TIMERSIGNALS_HPP
#define CORE_LIBS_PRX_LIBKERNEL_SYSTEM_TIMERSIGNALS_HPP

#include <cstdint>

constexpr int TimerSignalSlots = 32;

struct TimerSignal {
    int guest;
    std::int64_t value;
    int timer;
    std::int64_t thread;
    bool directed;
};

bool SendTimerSignal(const TimerSignal& signal, std::int64_t missed);
void ForgetTimerSignal(int timer);
int TimerSignalOverrun(int timer);

#endif
