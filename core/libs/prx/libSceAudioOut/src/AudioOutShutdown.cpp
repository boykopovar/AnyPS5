#include <atomic>
#include <cstdint>
#include "prx/libc/include/Shutdown.hpp"
#include "AudioOutShutdown.hpp"

namespace {

std::atomic<std::uint32_t> inside{0};
std::atomic<bool> stopping{false};

void Leave() {
    if (inside.fetch_sub(1) == 1) inside.notify_all();
}

void Stop() {
    stopping.store(true);
    for (auto count = inside.load(); count != 0; count = inside.load()) inside.wait(count);
}

}

AudioOutCall::AudioOutCall() {
    static const bool registered = (LibcRegisterShutdown_nid_postfix(Stop), true);
    static_cast<void>(registered);
    inside.fetch_add(1);
    if (stopping.load() || LibcShutdownToken_nid_postfix().stop_requested()) {
        Leave();
        throw ProcessShutdown{};
    }
}

AudioOutCall::~AudioOutCall() {
    Leave();
}
