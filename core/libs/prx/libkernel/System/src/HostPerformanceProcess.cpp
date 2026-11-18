#include "prx/libkernel/System/include/HostPerformance.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <utility>

#if defined(__linux__)
#include "prx/libkernel/System/include/GameModeClient.hpp"
#elif defined(__APPLE__)
#include "prx/libkernel/System/include/MacActivity.hpp"
#endif

namespace HostPerformance {

namespace {

#if !defined(__linux__) && !defined(__APPLE__)
class UnsupportedBackend final : public IBackend {
public:
    const char* Name() const override { return "host-performance"; }
    BeginResult Begin() override { return {Outcome::Unavailable, "this host has no game mode request"}; }
    void End() override {}
};
#endif

struct ProcessSession {
    std::unique_ptr<IBackend> Backend;
    Session Held;

    ProcessSession(Mode mode, std::unique_ptr<IBackend> backend)
        : Backend(std::move(backend)), Held(mode, *Backend, [](const std::string& line) { std::fprintf(stderr, "[host-performance] %s\n", line.c_str()); }) {}
};

ProcessSession* process = nullptr;

void StopProcessSession() {
    if (process != nullptr) process->Held.Stop();
}

}

std::unique_ptr<IBackend> CreateHostBackend() {
#if defined(__linux__)
    return std::make_unique<GameModeClient>();
#elif defined(__APPLE__)
    return std::make_unique<MacActivity>();
#else
    return std::make_unique<UnsupportedBackend>();
#endif
}

Session* StartProcessSession(Mode mode, std::unique_ptr<IBackend> backend) {
    if (process != nullptr) throw std::logic_error("host performance: the process session already exists");
    if (mode == Mode::Off) return nullptr;
    process = new ProcessSession(mode, std::move(backend));
    if (process->Held.Start() == State::Active) LibcRegisterShutdown_nid_postfix(StopProcessSession);
    return &process->Held;
}

void StartFromEnvironment() {
    const Mode mode = ParseMode(std::getenv("APS5_GAME_MODE"));
    if (mode == Mode::Off) return;
    StartProcessSession(mode, CreateHostBackend());
}

}
