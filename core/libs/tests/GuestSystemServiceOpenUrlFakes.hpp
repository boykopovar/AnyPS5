#pragma once

#include "prx/libSceSystemService/HostUrlPolicy.hpp"
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <string>
#include <vector>

class FakeClock final : public IUrlClock {
public:
    std::chrono::steady_clock::time_point Now() override { return now; }
    void Advance(std::chrono::seconds amount) { now += amount; }

private:
    std::chrono::steady_clock::time_point now{std::chrono::hours(1)};
};

class CountingLauncher final : public IUrlLauncher {
public:
    bool Launch(const std::string& url) override {
        const std::lock_guard lock(guard);
        launched.push_back(url);
        return result;
    }
    std::size_t Count() {
        const std::lock_guard lock(guard);
        return launched.size();
    }
    std::mutex guard;
    std::vector<std::string> launched;
    bool result = true;
};

class CountingConfirmation final : public IUrlConfirmation {
public:
    bool Confirm(const std::string& url) override {
        std::unique_lock lock(guard);
        asked.push_back(url);
        entered.notify_all();
        if (blocking) released.wait(lock, [this] { return !blocking; });
        return answer;
    }
    std::size_t Count() {
        const std::lock_guard lock(guard);
        return asked.size();
    }
    void WaitUntilAsked(std::size_t count) {
        std::unique_lock lock(guard);
        entered.wait(lock, [&] { return asked.size() >= count; });
    }
    void Release() {
        const std::lock_guard lock(guard);
        blocking = false;
        released.notify_all();
    }
    std::mutex guard;
    std::condition_variable entered;
    std::condition_variable released;
    std::vector<std::string> asked;
    bool answer = true;
    bool blocking = false;
};
