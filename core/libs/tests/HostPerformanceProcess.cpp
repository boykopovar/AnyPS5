#include "prx/libc/include/Shutdown.hpp"
#include "prx/libkernel/System/include/HostPerformance.hpp"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>

namespace {

using namespace HostPerformance;

class PrintingBackend final : public IBackend {
public:
    explicit PrintingBackend(Outcome outcome, int* beginCalls = nullptr) : outcome(outcome), beginCalls(beginCalls) {}
    const char* Name() const override { return "printing"; }

    BeginResult Begin() override {
        if (beginCalls != nullptr) ++(*beginCalls);
        std::puts("begin called");
        return {outcome, "scripted"};
    }

    void End() override {
        std::puts("release ran at exit");
        std::fflush(stdout);
    }

private:
    Outcome outcome;
    int* beginCalls;
};

int Fail(const char* message) {
    std::fprintf(stderr, "host performance process: %s\n", message);
    return 1;
}

}

int main(int argc, char** argv) {
    if (argc != 2) return Fail("usage: host_performance_process_tests <case>");
    const std::string name = argv[1];
    if (name == "exit" || name == "exit-without-handlers") {
        if (StartProcessSession(Mode::On, std::make_unique<PrintingBackend>(Outcome::Started)) == nullptr) return Fail("an on session must exist");
        std::fflush(stdout);
        if (name == "exit") LibcExit_nid_no_patch(0);
        LibcExitWithoutHandlers_nid_no_patch(0);
    }
    if (name == "refused") {
        if (StartProcessSession(Mode::Auto, std::make_unique<PrintingBackend>(Outcome::Refused)) == nullptr) return Fail("an auto session must exist");
        std::fflush(stdout);
        LibcExit_nid_no_patch(0);
    }
    if (name == "off") {
        if (StartProcessSession(Mode::Off, std::make_unique<PrintingBackend>(Outcome::Started)) != nullptr) return Fail("an off session must not exist");
        std::puts("off left the backend alone");
        std::fflush(stdout);
        LibcExit_nid_no_patch(0);
    }
    if (name == "twice") {
        if (StartProcessSession(Mode::Off, nullptr) != nullptr) return Fail("an off session with null backend must not exist");
        bool threwInvalidArgument = false;
        try {
            StartProcessSession(Mode::On, nullptr);
        } catch (const std::invalid_argument&) {
            threwInvalidArgument = true;
        }
        if (!threwInvalidArgument) return Fail("a null backend must be rejected with invalid_argument");
        int beginCalls = 0;
        Session* session = StartProcessSession(Mode::On, std::make_unique<PrintingBackend>(Outcome::Started, &beginCalls));
        if (session == nullptr || session->Current() != State::Active || beginCalls != 1) return Fail("a valid session must start after null backend rejection");
        try {
            StartProcessSession(Mode::On, std::make_unique<PrintingBackend>(Outcome::Started, &beginCalls));
        } catch (const std::logic_error&) {
            if (beginCalls != 1) return Fail("a second session must not call Begin");
            std::puts("second session refused");
            std::fflush(stdout);
            LibcExit_nid_no_patch(0);
        }
        return Fail("a second session must be refused");
    }
    if (name == "environment") {
        const char* value = std::getenv("APS5_GAME_MODE");
        try {
            StartFromEnvironment();
        } catch (const std::invalid_argument& error) {
            if (value != nullptr && std::strcmp(value, "bogus") == 0 && std::string(error.what()).find("APS5_GAME_MODE=bogus") != std::string::npos) {
                std::puts("invalid value rejected by name");
                return 0;
            }
            return Fail(error.what());
        }
        if (value != nullptr && std::strcmp(value, "bogus") == 0) return Fail("an invalid value must be rejected");
        std::puts("environment accepted");
        return 0;
    }
    return Fail("unknown case");
}
