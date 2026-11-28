#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_ENCODERREPLAYGATE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_TESTS_ENCODERREPLAYGATE_HPP

#include "prx/libSceAgcDriver/Graphics/include/CommandEncoder.hpp"
#include <atomic>
#include <functional>
#include <utility>

class EncoderReplayGate {
public:
    explicit EncoderReplayGate(std::function<void()> atReplay) : atReplay(std::move(atReplay)) {
        AgcDriver::Graphics::CommandEncoder::SetDelayForTesting([this] {
            held.wait(true, std::memory_order_acquire);
            if (!observed.exchange(true, std::memory_order_acq_rel)) this->atReplay();
        });
        AgcDriver::Graphics::CommandEncoder::SetBlockedHookForTesting([this] {
            held.store(false, std::memory_order_release);
            held.notify_all();
        });
    }

    EncoderReplayGate(const EncoderReplayGate&) = delete;
    EncoderReplayGate& operator=(const EncoderReplayGate&) = delete;

    ~EncoderReplayGate() {
        held.store(false, std::memory_order_release);
        held.notify_all();
        AgcDriver::Graphics::CommandEncoder::SetBlockedHookForTesting({});
        AgcDriver::Graphics::CommandEncoder::SetDelayForTesting({});
    }

    bool Observed() const { return observed.load(std::memory_order_acquire); }

private:
    std::function<void()> atReplay;
    std::atomic<bool> held{true};
    std::atomic<bool> observed{false};
};

#endif
