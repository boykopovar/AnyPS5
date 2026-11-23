#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_REWINDKICK_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_QUEUES_REWINDKICK_HPP

#include <atomic>
#include <cstdint>
#ifndef _WIN32
#include <condition_variable>
#include <mutex>
#endif

namespace AgcDriver::DriverDetail {

class RewindKick {
public:
    enum class Wake : std::uint8_t { Kick, Timer };

    class Waiter {
    public:
        Waiter(RewindKick& owner, const std::uint32_t* control);
        ~Waiter();
        Waiter(const Waiter&) = delete;
        Waiter& operator=(const Waiter&) = delete;
        Wake Sleep();

    private:
        RewindKick& kick;
        std::uint64_t seen = 0;
    };

    static RewindKick& Get();
    void Kick(const std::uint32_t* control);
    void WakeIdle();
    bool SleepIdle(const std::atomic<std::uint64_t>& queued);

private:
    RewindKick();
    Wake sleep(std::uint64_t& seen);
    void signal();

    std::atomic<const std::uint32_t*> awaited{nullptr};
    std::atomic<std::uint32_t> waiters{0};
    std::atomic<bool> idle{false};
#ifdef _WIN32
    void* event = nullptr;
    void* timer = nullptr;
#else
    std::mutex mutex;
    std::condition_variable changed;
    std::uint64_t signals = 0;
#endif
};

}

#endif
