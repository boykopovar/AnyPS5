#include "prx/libkernel/System/include/HostPerformance.hpp"
#include <cstring>
#include <exception>
#include <stdexcept>
#include <utility>

namespace HostPerformance {

Mode ParseMode(const char* text) {
    if (text == nullptr || *text == '\0' || std::strcmp(text, "off") == 0) return Mode::Off;
    if (std::strcmp(text, "on") == 0) return Mode::On;
    if (std::strcmp(text, "auto") == 0) return Mode::Auto;
    throw std::invalid_argument(std::string("APS5_GAME_MODE=") + text + " is not off, on or auto");
}

Session::Session(Mode mode, IBackend& backend, Log log) : mode(mode), backend(backend), log(std::move(log)) {}

State Session::Start() {
    std::lock_guard lock(mutex);
    if (state != State::Idle) return state;
    if (mode == Mode::Off) return state = State::Off;
    BeginResult begun;
    try {
        begun = backend.Begin();
    } catch (const std::exception& error) {
        begun = {Outcome::Refused, error.what()};
    } catch (...) {
        begun = {Outcome::Refused, "unknown failure"};
    }
    const std::string name = backend.Name();
    switch (begun.Result) {
    case Outcome::Started:
        log(name + ": requested");
        return state = State::Active;
    case Outcome::Unavailable:
        if (mode == Mode::On) log(name + ": unavailable, continuing without it (" + begun.Detail + ")");
        return state = State::Unavailable;
    case Outcome::Refused:
        log(name + ": request refused, continuing without it (" + begun.Detail + ")");
        return state = State::Refused;
    }
    return state;
}

void Session::Stop() {
    std::lock_guard lock(mutex);
    if (state != State::Active) return;
    state = State::Ended;
    try {
        backend.End();
    } catch (const std::exception& error) {
        log(std::string(backend.Name()) + ": release failed (" + error.what() + ")");
    } catch (...) {
        log(std::string(backend.Name()) + ": release failed");
    }
}

State Session::Current() const {
    std::lock_guard lock(mutex);
    return state;
}

}
