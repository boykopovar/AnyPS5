#include "prx/libkernel/System/include/HostPerformance.hpp"
#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>
#include <utility>

namespace HostPerformance {

namespace {

std::string SafeName(const IBackend& backend) {
    const char* rawName = backend.Name();
    return rawName != nullptr ? std::string(rawName) : std::string("host-performance");
}

void EmitLogQuietly(const Log& log, const IBackend& backend, const char* detail) {
    if (!log) return;
    try {
        std::string message = SafeName(backend) + ": release failed";
        if (detail != nullptr) message += std::string(" (") + detail + ")";
        log(message);
    } catch (...) {
    }
}

}

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
    const std::string name = SafeName(backend);
    switch (begun.Result) {
    case Outcome::Started:
        state = State::Active;
        if (log) log(name + ": requested");
        return state;
    case Outcome::Unavailable:
        state = State::Unavailable;
        if (mode == Mode::On && log) log(name + ": unavailable, continuing without it (" + begun.Detail + ")");
        return state;
    case Outcome::Refused:
        state = State::Refused;
        if (log) log(name + ": request refused, continuing without it (" + begun.Detail + ")");
        return state;
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
        EmitLogQuietly(log, backend, error.what());
    } catch (...) {
        EmitLogQuietly(log, backend, nullptr);
    }
}

State Session::Current() const {
    std::lock_guard lock(mutex);
    return state;
}

}
