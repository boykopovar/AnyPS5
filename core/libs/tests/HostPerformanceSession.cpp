#include "prx/libkernel/System/include/HostPerformance.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace HostPerformance;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

class FakeBackend final : public IBackend {
public:
    BeginResult Next{Outcome::Started, {}};
    bool ThrowOnBegin = false;
    bool ThrowOnEnd = false;
    int Begins = 0;
    int Ends = 0;

    const char* Name() const override { return "fake"; }

    BeginResult Begin() override {
        ++Begins;
        if (ThrowOnBegin) throw std::runtime_error("begin exploded");
        return Next;
    }

    void End() override {
        ++Ends;
        if (ThrowOnEnd) throw std::runtime_error("end exploded");
    }
};

struct Harness {
    FakeBackend Backend;
    std::vector<std::string> Lines;
    Session Make(Mode mode) {
        return Session(mode, Backend, [this](const std::string& line) { Lines.push_back(line); });
    }
};

void ParsingKeepsTheDefaultOff() {
    for (const char* text : {static_cast<const char*>(nullptr), "", "off"}) Require(ParseMode(text) == Mode::Off, "APS5_GAME_MODE unset, empty or off must leave the host alone");
    Require(ParseMode("on") == Mode::On, "APS5_GAME_MODE=on must request game mode");
    Require(ParseMode("auto") == Mode::Auto, "APS5_GAME_MODE=auto must request game mode only when available");
    for (const char* text : {"1", "0", "ON", "Auto", " on", "yes", "true", "off "}) {
        bool threw = false;
        try {
            static_cast<void>(ParseMode(text));
        } catch (const std::invalid_argument& error) {
            threw = std::string(error.what()).find("APS5_GAME_MODE=") != std::string::npos;
        }
        Require(threw, std::string("APS5_GAME_MODE=") + text + " must be rejected with an error naming the variable");
    }
}

void OffNeverTouchesTheBackend() {
    Harness harness;
    auto session = harness.Make(Mode::Off);
    Require(session.Start() == State::Off, "off must report the off state");
    session.Stop();
    Require(harness.Backend.Begins == 0 && harness.Backend.Ends == 0, "off must not call the backend");
    Require(harness.Lines.empty(), "off must log nothing");
}

void StartedIsReleasedOnceAndStartedOnce() {
    for (const Mode mode : {Mode::On, Mode::Auto}) {
        Harness harness;
        auto session = harness.Make(mode);
        Require(session.Start() == State::Active, "a started request must be active");
        Require(session.Start() == State::Active, "a second start must keep the state");
        Require(harness.Backend.Begins == 1, "start must reach the backend once");
        Require(harness.Lines.size() == 1, "a started request must log once");
        session.Stop();
        session.Stop();
        Require(session.Current() == State::Ended, "stop must end an active request");
        Require(harness.Backend.Ends == 1, "stop must release exactly once");
        Require(session.Start() == State::Ended, "a stopped session must not start again");
        Require(harness.Backend.Begins == 1, "a stopped session must not reach the backend again");
    }
}

void UnavailableIsQuietInAutoAndLoggedInOn() {
    Harness automatic;
    automatic.Backend.Next = {Outcome::Unavailable, "no library"};
    auto autoSession = automatic.Make(Mode::Auto);
    Require(autoSession.Start() == State::Unavailable, "auto must report an unavailable backend");
    Require(automatic.Lines.empty(), "auto must stay silent when the backend is unavailable");
    autoSession.Stop();
    Require(automatic.Backend.Ends == 0, "an unavailable backend must never be released");

    Harness forced;
    forced.Backend.Next = {Outcome::Unavailable, "no library"};
    auto onSession = forced.Make(Mode::On);
    onSession.Start();
    onSession.Start();
    Require(forced.Lines.size() == 1 && forced.Lines[0].find("no library") != std::string::npos, "on must log an unavailable backend once with its reason");
}

void RefusalIsLoggedOnceInBothModes() {
    for (const Mode mode : {Mode::On, Mode::Auto}) {
        Harness harness;
        harness.Backend.Next = {Outcome::Refused, "daemon not running"};
        auto session = harness.Make(mode);
        Require(session.Start() == State::Refused, "a refused request must report the refused state");
        session.Start();
        session.Stop();
        Require(harness.Lines.size() == 1 && harness.Lines[0].find("daemon not running") != std::string::npos, "a refusal must be logged once with its reason");
        Require(harness.Backend.Ends == 0, "a refused request must never be released");
    }
}

void ThrowingBackendsAreContained() {
    Harness harness;
    harness.Backend.ThrowOnBegin = true;
    auto session = harness.Make(Mode::On);
    Require(session.Start() == State::Refused, "a throwing begin must count as a refusal");
    Require(harness.Lines.size() == 1 && harness.Lines[0].find("begin exploded") != std::string::npos, "a throwing begin must log its message");

    Harness releasing;
    releasing.Backend.ThrowOnEnd = true;
    auto active = releasing.Make(Mode::On);
    active.Start();
    active.Stop();
    Require(active.Current() == State::Ended, "a throwing end must still end the session");
    Require(releasing.Lines.size() == 2 && releasing.Lines[1].find("end exploded") != std::string::npos, "a throwing end must be logged");
}

void ConcurrentStartsReachTheBackendOnce() {
    Harness harness;
    auto session = harness.Make(Mode::On);
    std::vector<std::thread> threads;
    for (int index = 0; index < 8; ++index) {
        threads.emplace_back([&] {
            session.Start();
            session.Stop();
        });
    }
    for (auto& thread : threads) thread.join();
    Require(harness.Backend.Begins == 1 && harness.Backend.Ends == 1, "concurrent start and stop must reach the backend once each");
}

}

int main() {
    try {
        ParsingKeepsTheDefaultOff();
        OffNeverTouchesTheBackend();
        StartedIsReleasedOnceAndStartedOnce();
        UnavailableIsQuietInAutoAndLoggedInOn();
        RefusalIsLoggedOnceInBothModes();
        ThrowingBackendsAreContained();
        ConcurrentStartsReachTheBackendOnce();
    } catch (const std::exception& error) {
        std::cerr << "host performance session: " << error.what() << "\n";
        return 1;
    }
    std::cout << "host performance session passed\n";
    return 0;
}
