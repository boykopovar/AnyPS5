#include "GuestSystemServiceOpenUrlFakes.hpp"
#include "prx/libSceSystemService/HostUrlPolicy.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

void Require(bool value) { if (!value) std::abort(); }

void CheckBusy() {
    FakeClock clock;
    OpenUrlGate gate(clock);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    confirmation.blocking = true;
    OpenUrlGateOutcome first = OpenUrlGateOutcome::Failed;
    std::thread active([&] { first = gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation); });
    confirmation.WaitUntilAsked(1);

    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Busy);
    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/b", launcher, confirmation) == OpenUrlGateOutcome::Busy);
    Require(gate.Run(OpenUrlMode::Always, "https://example.com/c", launcher, confirmation) == OpenUrlGateOutcome::Busy);
    Require(confirmation.Count() == 1 && launcher.Count() == 0);
    Require(gate.Run(OpenUrlMode::Never, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Disabled);
    Require(gate.Run(OpenUrlMode::Ask, "psno://example.com", launcher, confirmation) == OpenUrlGateOutcome::Rejected);

    confirmation.Release();
    active.join();
    Require(first == OpenUrlGateOutcome::Launched);
    Require(confirmation.Count() == 1 && launcher.Count() == 1);
}

void CheckNever() {
    FakeClock clock;
    OpenUrlGate gate(clock);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    for (int index = 0; index < 1000; ++index)
        Require(gate.Run(OpenUrlMode::Never, "https://example.com/" + std::to_string(index), launcher, confirmation) == OpenUrlGateOutcome::Disabled);
    Require(confirmation.Count() == 0 && launcher.Count() == 0);
}

void CheckRateCap() {
    FakeClock clock;
    for (const OpenUrlMode mode : {OpenUrlMode::Ask, OpenUrlMode::Always}) {
        OpenUrlGate gate(clock);
        CountingLauncher launcher;
        CountingConfirmation confirmation;
        for (std::size_t index = 0; index < OPEN_URL_MAX_ATTEMPTS; ++index) {
            Require(gate.Run(mode, "https://example.com/" + std::to_string(index), launcher, confirmation) == OpenUrlGateOutcome::Launched);
            clock.Advance(std::chrono::seconds(1));
        }
        for (int index = 0; index < 1000; ++index)
            Require(gate.Run(mode, index % 2 == 0 ? "https://example.com/same" : "https://example.org/" + std::to_string(index), launcher, confirmation) ==
                    OpenUrlGateOutcome::Limited);
        Require(launcher.Count() == OPEN_URL_MAX_ATTEMPTS);
        Require(confirmation.Count() == (mode == OpenUrlMode::Ask ? OPEN_URL_MAX_ATTEMPTS : 0));

        clock.Advance(OPEN_URL_WINDOW - std::chrono::seconds(OPEN_URL_MAX_ATTEMPTS) - std::chrono::seconds(1));
        Require(gate.Run(mode, "https://example.com/late", launcher, confirmation) == OpenUrlGateOutcome::Limited);
        clock.Advance(std::chrono::seconds(1));
        Require(gate.Run(mode, "https://example.com/recovered", launcher, confirmation) == OpenUrlGateOutcome::Launched);
        Require(launcher.Count() == OPEN_URL_MAX_ATTEMPTS + 1);
        Require(gate.Run(mode, "https://example.com/again", launcher, confirmation) == OpenUrlGateOutcome::Limited);
        clock.Advance(std::chrono::seconds(1));
        Require(gate.Run(mode, "https://example.com/again", launcher, confirmation) == OpenUrlGateOutcome::Launched);
        Require(gate.Run(mode, "https://example.com/again2", launcher, confirmation) == OpenUrlGateOutcome::Limited);
        clock.Advance(OPEN_URL_WINDOW);
    }
}

void CheckRejectedAndFailedCount() {
    FakeClock clock;
    OpenUrlGate gate(clock);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    for (int index = 0; index < 50; ++index)
        Require(gate.Run(OpenUrlMode::Always, "https://u:p@example.com/", launcher, confirmation) == OpenUrlGateOutcome::Rejected);
    Require(launcher.Count() == 0);
    launcher.result = false;
    for (std::size_t index = 0; index < OPEN_URL_MAX_ATTEMPTS; ++index)
        Require(gate.Run(OpenUrlMode::Always, "https://example.com/", launcher, confirmation) == OpenUrlGateOutcome::Failed);
    Require(gate.Run(OpenUrlMode::Always, "https://example.com/", launcher, confirmation) == OpenUrlGateOutcome::Limited);
    Require(launcher.Count() == OPEN_URL_MAX_ATTEMPTS);
}

void CheckDeclineCooldown() {
    FakeClock clock;
    OpenUrlGate gate(clock);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    confirmation.answer = false;
    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Declined);
    for (int index = 0; index < 1000; ++index) {
        Require(gate.Run(OpenUrlMode::Ask, index % 2 == 0 ? "https://example.com/a" : "https://example.net/" + std::to_string(index), launcher, confirmation) ==
                OpenUrlGateOutcome::Suppressed);
    }
    Require(gate.Run(OpenUrlMode::Always, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Suppressed);
    Require(confirmation.Count() == 1 && launcher.Count() == 0);

    clock.Advance(OPEN_URL_DECLINE_COOLDOWN - std::chrono::seconds(1));
    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Suppressed);
    Require(confirmation.Count() == 1);
    clock.Advance(std::chrono::seconds(1));
    confirmation.answer = true;
    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Launched);
    Require(confirmation.Count() == 2 && launcher.Count() == 1);
    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/b", launcher, confirmation) == OpenUrlGateOutcome::Launched);
}

void CheckRepeatedCancel() {
    FakeClock clock;
    OpenUrlGate gate(clock);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    confirmation.answer = false;
    for (int round = 0; round < 5; ++round) {
        Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Declined);
        Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Suppressed);
        clock.Advance(OPEN_URL_DECLINE_COOLDOWN);
    }
    Require(confirmation.Count() == 5 && launcher.Count() == 0);
}

void CheckSuppressedDoesNotUseAttempts() {
    FakeClock clock;
    OpenUrlGate gate(clock);
    CountingLauncher launcher;
    CountingConfirmation confirmation;
    confirmation.answer = false;
    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Declined);
    clock.Advance(OPEN_URL_DECLINE_COOLDOWN - std::chrono::seconds(1));
    for (int index = 0; index < 10; ++index) Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Suppressed);
    clock.Advance(std::chrono::seconds(1));
    confirmation.answer = true;
    for (std::size_t index = 0; index < OPEN_URL_MAX_ATTEMPTS; ++index)
        Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Launched);
    Require(gate.Run(OpenUrlMode::Ask, "https://example.com/a", launcher, confirmation) == OpenUrlGateOutcome::Limited);
}

}

int main() {
    CheckBusy();
    CheckNever();
    CheckRateCap();
    CheckRejectedAndFailedCount();
    CheckDeclineCooldown();
    CheckRepeatedCancel();
    CheckSuppressedDoesNotUseAttempts();
    return 0;
}
