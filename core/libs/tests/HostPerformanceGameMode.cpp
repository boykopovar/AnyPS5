#include "prx/libkernel/System/include/GameModeClient.hpp"
#include <dlfcn.h>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace HostPerformance;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

bool Contains(const std::string& text, const std::string& part) {
    return text.find(part) != std::string::npos;
}

struct Fixture {
    void* Handle = nullptr;
    void (*Configure)(int, int) = nullptr;
    int (*Starts)() = nullptr;
    int (*Ends)() = nullptr;

    explicit Fixture(const std::string& path) {
        Handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
        Require(Handle != nullptr, "cannot load the fixture " + path);
        Configure = reinterpret_cast<void (*)(int, int)>(dlsym(Handle, "fixture_configure"));
        Starts = reinterpret_cast<int (*)()>(dlsym(Handle, "fixture_starts"));
        Ends = reinterpret_cast<int (*)()>(dlsym(Handle, "fixture_ends"));
        Require(Configure != nullptr && Starts != nullptr && Ends != nullptr, "the fixture lacks its counters");
    }

    ~Fixture() { dlclose(Handle); }
};

void MissingLibraryIsUnavailable(const std::string& missing) {
    GameModeClient client({missing});
    const auto begun = client.Begin();
    Require(begun.Result == Outcome::Unavailable, "a library that cannot be loaded must be unavailable");
    Require(Contains(begun.Detail, missing), "the reason must name the library that failed");
    client.End();
}

void NoLibraryNamesIsUnavailable() {
    GameModeClient client(std::vector<std::string>{});
    Require(client.Begin().Result == Outcome::Unavailable, "an empty library list must be unavailable");
}

void SecondNameIsTriedAfterTheFirstFails(const std::string& missing, const std::string& fixturePath) {
    Fixture fixture(fixturePath);
    GameModeClient client({missing, fixturePath});
    Require(client.Begin().Result == Outcome::Started, "the next library name must be tried when one fails");
    Require(fixture.Starts() == 1, "the request must reach the library that loaded");
    client.End();
}

void StartAndEndReachTheLibraryOnce(const std::string& fixturePath) {
    Fixture fixture(fixturePath);
    fixture.Configure(0, 0);
    const int startsBefore = fixture.Starts();
    const int endsBefore = fixture.Ends();
    {
        GameModeClient client({fixturePath});
        Require(client.Begin().Result == Outcome::Started, "a library that accepts the request must start");
        Require(client.Begin().Result == Outcome::Started, "a repeated begin must keep the request");
        Require(fixture.Starts() == startsBefore + 1, "a repeated begin must not request again");
        client.End();
        client.End();
        Require(fixture.Ends() == endsBefore + 1, "end must release once");
    }
    Require(fixture.Ends() == endsBefore + 1, "the destructor must not release an ended request again");
    {
        GameModeClient client({fixturePath});
        client.Begin();
    }
    Require(fixture.Ends() == endsBefore + 2, "the destructor must release a request still held");
}

void EndWithoutBeginDoesNothing(const std::string& fixturePath) {
    Fixture fixture(fixturePath);
    const int endsBefore = fixture.Ends();
    GameModeClient client({fixturePath});
    client.End();
    Require(fixture.Ends() == endsBefore, "end without a request must not reach the library");
}

void RefusalCarriesTheLibraryReason(const std::string& fixturePath) {
    Fixture fixture(fixturePath);
    fixture.Configure(-1, 0);
    const int endsBefore = fixture.Ends();
    {
        GameModeClient client({fixturePath});
        const auto begun = client.Begin();
        Require(begun.Result == Outcome::Refused, "a library that refuses must be reported as refused");
        Require(Contains(begun.Detail, "fixture daemon is not running"), "the refusal must carry the library's error string");
        client.End();
    }
    Require(fixture.Ends() == endsBefore, "a refused request must never be released");
    fixture.Configure(0, 0);
}

void RefusalWithoutErrorStringStillReports(const std::string& fixturePath) {
    Fixture fixture(fixturePath);
    fixture.Configure(-1, 0);
    GameModeClient client({fixturePath});
    const auto begun = client.Begin();
    Require(begun.Result == Outcome::Refused && !begun.Detail.empty(), "a refusal without an error string must still carry a reason");
    fixture.Configure(0, 0);
}

void FailingEndIsReported(const std::string& fixturePath) {
    Fixture fixture(fixturePath);
    fixture.Configure(0, -1);
    GameModeClient client({fixturePath});
    client.Begin();
    bool threw = false;
    try {
        client.End();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, "a failing end must be reported so the session can log it");
    fixture.Configure(0, 0);
}

void MissingSymbolIsUnavailable(const std::string& fixturePath, const char* symbol) {
    GameModeClient client({fixturePath});
    const auto begun = client.Begin();
    Require(begun.Result == Outcome::Unavailable, std::string("a library without ") + symbol + " must be unavailable");
    Require(Contains(begun.Detail, symbol), std::string("the reason must name ") + symbol);
}

void SessionOverTheClient(const std::string& fixturePath) {
    Fixture fixture(fixturePath);
    fixture.Configure(0, 0);
    const int startsBefore = fixture.Starts();
    const int endsBefore = fixture.Ends();
    GameModeClient client({fixturePath});
    std::vector<std::string> lines;
    Session session(Mode::On, client, [&](const std::string& line) { lines.push_back(line); });
    Require(session.Start() == State::Active, "the session must be active over a working library");
    session.Stop();
    Require(fixture.Starts() == startsBefore + 1 && fixture.Ends() == endsBefore + 1, "the session must request once and release once");
    Require(lines.size() == 1, "the session must log one line");
}

}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::cerr << "usage: host_performance_game_mode_tests <fixture> <fixture-without-end> <fixture-without-error-string> <missing-path>\n";
        return 2;
    }
    const std::string fixture = argv[1];
    try {
        MissingLibraryIsUnavailable(argv[4]);
        NoLibraryNamesIsUnavailable();
        SecondNameIsTriedAfterTheFirstFails(argv[4], fixture);
        StartAndEndReachTheLibraryOnce(fixture);
        EndWithoutBeginDoesNothing(fixture);
        RefusalCarriesTheLibraryReason(fixture);
        RefusalWithoutErrorStringStillReports(argv[3]);
        FailingEndIsReported(fixture);
        MissingSymbolIsUnavailable(argv[2], "real_gamemode_request_end");
        SessionOverTheClient(fixture);
    } catch (const std::exception& error) {
        std::cerr << "host performance game mode: " << error.what() << "\n";
        return 1;
    }
    std::cout << "host performance game mode passed\n";
    return 0;
}
