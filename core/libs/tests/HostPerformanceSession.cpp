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
    const char* BackendName = "fake";
    BeginResult Next{Outcome::Started, {}};
    bool ThrowOnBegin = false;
    bool ThrowOnEnd = false;
    bool ThrowNonStdOnEnd = false;
    int Begins = 0;
    int Ends = 0;

    const char* Name() const override { return BackendName; }

    BeginResult Begin() override {
        ++Begins;
        if (ThrowOnBegin) throw std::runtime_error("begin exploded");
        return Next;
    }

    void End() override {
        ++Ends;
        if (ThrowOnEnd) throw std::runtime_error("end exploded");
        if (ThrowNonStdOnEnd) throw 42;
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

    Harness releasingNonStd;
    releasingNonStd.Backend.ThrowNonStdOnEnd = true;
    auto activeNonStd = releasingNonStd.Make(Mode::On);
    activeNonStd.Start();
    activeNonStd.Stop();
    Require(activeNonStd.Current() == State::Ended, "a non-std throwing end must still end the session");
    Require(releasingNonStd.Lines.size() == 2 && releasingNonStd.Lines[1] == "fake: release failed", "a non-std throwing end must log without parenthesized detail");
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

void ThrowingLoggerCommitsStateBeforeThrowing() {
    struct Case {
        Mode SessionMode;
        BeginResult Result;
        State Expected;
        int ExpectedEnds;
        const char* Label;
    };
    const Case cases[] = {
        {Mode::Auto, {Outcome::Started, {}}, State::Active, 1, "started"},
        {Mode::On, {Outcome::Unavailable, "missing"}, State::Unavailable, 0, "unavailable"},
        {Mode::Auto, {Outcome::Refused, "denied"}, State::Refused, 0, "refused"},
    };
    for (const auto& entry : cases) {
        FakeBackend backend;
        backend.Next = entry.Result;
        Session session(
            entry.SessionMode,
            backend,
            [](const std::string&) { throw std::runtime_error("logger failed"); });
        bool threw = false;
        try {
            static_cast<void>(session.Start());
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw, std::string(entry.Label) + " throwing logger must propagate out of start");
        Require(session.Current() == entry.Expected, std::string(entry.Label) + " state must be committed before logging");
        Require(backend.Begins == 1, std::string(entry.Label) + " must call begin once");
        Require(session.Start() == entry.Expected, std::string(entry.Label) + " repeat start must observe committed state");
        Require(backend.Begins == 1, std::string(entry.Label) + " repeat start must not re-invoke begin");
        session.Stop();
        Require(backend.Ends == entry.ExpectedEnds, std::string(entry.Label) + " stop must match committed state");
        if (entry.Expected == State::Active) {
            Require(session.Current() == State::Ended, "active session must end after stop");
        }
    }
}

void EmptyLoggerSucceedsInStartAndStop() {
    FakeBackend startedBackend;
    startedBackend.ThrowOnEnd = true;
    Session startedSession(Mode::On, startedBackend, Session::Log{});
    Require(startedSession.Start() == State::Active, "empty logger must allow active start");
    Require(startedSession.Current() == State::Active, "empty logger must keep active state");
    startedSession.Stop();
    Require(startedBackend.Ends == 1, "empty logger stop must release backend");
    Require(startedSession.Current() == State::Ended, "empty logger stop must end active session");

    FakeBackend unavailableBackend;
    unavailableBackend.Next = {Outcome::Unavailable, "missing"};
    Session unavailableSession(Mode::On, unavailableBackend, Session::Log{});
    Require(unavailableSession.Start() == State::Unavailable, "empty logger must allow unavailable start");

    FakeBackend refusedBackend;
    refusedBackend.Next = {Outcome::Refused, "denied"};
    Session refusedSession(Mode::Auto, refusedBackend, Session::Log{});
    Require(refusedSession.Start() == State::Refused, "empty logger must allow refused start");
}

void NullBackendNameDoesNotCrash() {
    Harness harness;
    harness.Backend.BackendName = nullptr;
    harness.Backend.ThrowOnEnd = true;
    auto session = harness.Make(Mode::Auto);
    Require(session.Start() == State::Active, "null backend name must still start");
    session.Stop();
    Require(session.Current() == State::Ended, "null backend name must still end");
    Require(harness.Lines.size() == 2, "null backend name must log start and release failure");
    Require(harness.Lines[0] == "host-performance: requested", "null backend name must fall back on start");
    Require(harness.Lines[1] == "host-performance: release failed (end exploded)", "null backend name must fall back on stop");
}

void ThrowingLoggerDoesNotEscapeStop() {
    FakeBackend backend;
    backend.ThrowOnEnd = true;
    int logCalls = 0;
    Session session(
        Mode::Auto,
        backend,
        [&logCalls](const std::string&) {
            ++logCalls;
            if (logCalls >= 2) throw std::runtime_error("stop logger failed");
        });
    Require(session.Start() == State::Active, "session must start before stop logger test");
    session.Stop();
    Require(backend.Ends == 1, "stop must release backend once");
    Require(logCalls == 2, "stop must attempt failure log");
    Require(session.Current() == State::Ended, "stop must leave session ended when logger throws");
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
        ThrowingLoggerCommitsStateBeforeThrowing();
        EmptyLoggerSucceedsInStartAndStop();
        NullBackendNameDoesNotCrash();
        ThrowingLoggerDoesNotEscapeStop();
    } catch (const std::exception& error) {
        std::cerr << "host performance session: " << error.what() << "\n";
        return 1;
    }
    std::cout << "host performance session passed\n";
    return 0;
}
