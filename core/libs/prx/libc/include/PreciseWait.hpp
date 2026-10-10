#ifndef CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP
#define CORE_LIBS_PRX_LIBC_INCLUDE_PRECISEWAIT_HPP

#include <chrono>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

inline constexpr unsigned long long kSubTickWaitUs = 2000;

inline void PreciseSleepUs(unsigned long long micros) {
    if (micros == 0) {
        return;
    }
#ifdef _WIN32
    struct Timer {
        HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr,
            CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        ~Timer() {
            if (handle != nullptr) {
                CloseHandle(handle);
            }
        }
    };
    thread_local Timer timer;
    if (timer.handle != nullptr) {
        LARGE_INTEGER due{};
        due.QuadPart = -static_cast<LONGLONG>(micros * 10ULL);
        if (SetWaitableTimerEx(timer.handle, &due, 0, nullptr, nullptr, nullptr, 0) != FALSE) {
            WaitForSingleObject(timer.handle, INFINITE);
            return;
        }
    }
#endif
    std::this_thread::sleep_for(std::chrono::microseconds(micros));
}

template <class TClock, class TDuration>
inline void PreciseSleepUntil(const std::chrono::time_point<TClock, TDuration>& deadline) {
    for (auto now = TClock::now(); now < deadline; now = TClock::now()) {
        PreciseSleepUs(static_cast<unsigned long long>(std::chrono::ceil<std::chrono::microseconds>(deadline - now).count()));
    }
}

#endif
