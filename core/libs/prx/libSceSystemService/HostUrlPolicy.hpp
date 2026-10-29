#pragma once

#include <chrono>
#include <cstddef>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include "prx/libSceSystemService/HostUrl.hpp"

enum class OpenUrlMode {
    Never,
    Ask,
    Always,
};

inline OpenUrlMode ParseOpenUrlMode(const char* configured, std::string_view caller) {
    if (configured == nullptr || configured[0] == '\0') return OpenUrlMode::Never;
    const std::string_view text(configured);
    if (text == "never") return OpenUrlMode::Never;
    if (text == "ask") return OpenUrlMode::Ask;
    if (text == "always") return OpenUrlMode::Always;
    throw std::runtime_error(std::string(caller) + ": ANYPS5_OPEN_URLS must be never, ask or always, not \"" + std::string(text) + "\"");
}

inline OpenUrlMode OpenUrlModeFromEnvironment(std::string_view caller) {
    return ParseOpenUrlMode(std::getenv("ANYPS5_OPEN_URLS"), caller);
}

class IUrlLauncher {
public:
    virtual ~IUrlLauncher() = default;
    virtual bool Launch(const std::string& url) = 0;
};

class IUrlConfirmation {
public:
    virtual ~IUrlConfirmation() = default;
    virtual bool Confirm(const std::string& url) = 0;
};

enum class OpenUrlOutcome {
    Disabled,
    Rejected,
    Declined,
    Failed,
    Launched,
};

inline OpenUrlOutcome OpenUrlOnHost(OpenUrlMode mode, std::string_view url, IUrlLauncher& launcher, IUrlConfirmation& confirmation) {
    if (mode == OpenUrlMode::Never) return OpenUrlOutcome::Disabled;
    if (ValidateHostUrl(url) != HostUrlVerdict::Ok) return OpenUrlOutcome::Rejected;
    const std::string address(url);
    if (mode == OpenUrlMode::Ask && !confirmation.Confirm(address)) return OpenUrlOutcome::Declined;
    return launcher.Launch(address) ? OpenUrlOutcome::Launched : OpenUrlOutcome::Failed;
}

class IUrlClock {
public:
    virtual ~IUrlClock() = default;
    virtual std::chrono::steady_clock::time_point Now() = 0;
};

class SteadyUrlClock final : public IUrlClock {
public:
    std::chrono::steady_clock::time_point Now() override { return std::chrono::steady_clock::now(); }
};

constexpr std::size_t OPEN_URL_MAX_ATTEMPTS = 3;
constexpr std::chrono::seconds OPEN_URL_WINDOW{60};
constexpr std::chrono::seconds OPEN_URL_DECLINE_COOLDOWN{600};

enum class OpenUrlGateOutcome {
    Disabled,
    Rejected,
    Busy,
    Limited,
    Suppressed,
    Declined,
    Failed,
    Launched,
};

class OpenUrlGate {
public:
    explicit OpenUrlGate(IUrlClock& clock) : clock(&clock) {}

    void Reset(IUrlClock& replacement) {
        const std::lock_guard lock(state);
        clock = &replacement;
    }

    OpenUrlGateOutcome Run(OpenUrlMode mode, std::string_view url, IUrlLauncher& launcher, IUrlConfirmation& confirmation) {
        if (mode == OpenUrlMode::Never) return OpenUrlGateOutcome::Disabled;
        if (ValidateHostUrl(url) != HostUrlVerdict::Ok) return OpenUrlGateOutcome::Rejected;
        std::unique_lock lock(state, std::try_to_lock);
        if (!lock.owns_lock()) return OpenUrlGateOutcome::Busy;
        const auto now = clock->Now();
        if (suppressed && now < suppressedUntil) return OpenUrlGateOutcome::Suppressed;
        while (!attempts.empty() && now - attempts.front() >= OPEN_URL_WINDOW) attempts.pop_front();
        if (attempts.size() >= OPEN_URL_MAX_ATTEMPTS) return OpenUrlGateOutcome::Limited;
        attempts.push_back(now);
        switch (OpenUrlOnHost(mode, url, launcher, confirmation)) {
        case OpenUrlOutcome::Launched: return OpenUrlGateOutcome::Launched;
        case OpenUrlOutcome::Declined:
            suppressed = true;
            suppressedUntil = now + OPEN_URL_DECLINE_COOLDOWN;
            return OpenUrlGateOutcome::Declined;
        case OpenUrlOutcome::Failed: return OpenUrlGateOutcome::Failed;
        case OpenUrlOutcome::Rejected: return OpenUrlGateOutcome::Rejected;
        case OpenUrlOutcome::Disabled: return OpenUrlGateOutcome::Disabled;
        }
        return OpenUrlGateOutcome::Failed;
    }

private:
    std::mutex state;
    IUrlClock* clock;
    std::deque<std::chrono::steady_clock::time_point> attempts;
    bool suppressed = false;
    std::chrono::steady_clock::time_point suppressedUntil;
};
