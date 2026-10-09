#pragma once

#include <array>
#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <deque>
#include <exception>
#include <functional>
#include <mutex>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace AgcDriver {

class ProfileOutput {
public:
    explicit ProfileOutput(std::function<void(std::string_view)> writer, std::size_t capacity = 4 * 1024 * 1024) : writer(std::move(writer)), capacity(capacity) {
        if (!this->writer || capacity == 0) throw std::invalid_argument("invalid profile output writer or capacity");
        worker = std::thread([this] { run(); });
    }

    ~ProfileOutput() {
        Stop();
    }

    void Stop() {
        std::lock_guard stopLock(stopMutex);
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_one();
        if (worker.joinable()) worker.join();
    }

    bool Write(std::string text) {
        {
            std::lock_guard lock(mutex);
            if (failure) std::rethrow_exception(failure);
            if (stopping) throw std::runtime_error("profile output is stopping");
            if (text.empty()) return true;
            if (text.size() > capacity - bytes) {
                ++dropped;
                changed.notify_one();
                return false;
            }
            const auto size = text.size();
            pending.push_back(std::move(text));
            bytes += size;
        }
        changed.notify_one();
        return true;
    }

    template<typename... TArgs>
    void Print(const char* format, TArgs... args) {
        std::array<char, 2048> local{};
        const auto size = std::snprintf(local.data(), local.size(), format, args...);
        if (size < 0) throw std::runtime_error("profile output formatting failed");
        if (static_cast<std::size_t>(size) < local.size()) {
            Write(std::string(local.data(), size));
        } else if (static_cast<std::size_t>(size) > capacity) {
            std::lock_guard lock(mutex);
            if (failure) std::rethrow_exception(failure);
            ++dropped;
            changed.notify_one();
        } else {
            std::vector<char> text(static_cast<std::size_t>(size) + 1);
            if (std::snprintf(text.data(), text.size(), format, args...) != size) throw std::runtime_error("profile output formatting changed");
            Write(std::string(text.data(), size));
        }
    }

    void Flush() {
        std::unique_lock lock(mutex);
        idle.wait(lock, [&] { return failure || (pending.empty() && !writing && dropped == 0); });
        if (failure) std::rethrow_exception(failure);
    }

private:
    void run() noexcept {
        try {
            for (;;) {
                std::string batch;
                std::size_t consumed = 0;
                {
                    std::unique_lock lock(mutex);
                    changed.wait(lock, [&] { return stopping || !pending.empty() || dropped != 0; });
                    if (stopping && pending.empty() && dropped == 0) return;
                    while (!pending.empty() && (batch.empty() || batch.size() + pending.front().size() <= 65536)) {
                        consumed += pending.front().size();
                        batch += pending.front();
                        pending.pop_front();
                    }
                    if (dropped != 0) {
                        batch += "[profile-output] dropped " + std::to_string(dropped) + " chunks because the diagnostic writer fell behind\n";
                        dropped = 0;
                    }
                    writing = true;
                }
                writer(batch);
                {
                    std::lock_guard lock(mutex);
                    bytes -= consumed;
                    writing = false;
                }
                idle.notify_all();
            }
        } catch (...) {
            {
                std::lock_guard lock(mutex);
                failure = std::current_exception();
                writing = false;
            }
            idle.notify_all();
        }
    }

    std::function<void(std::string_view)> writer;
    const std::size_t capacity;
    std::mutex stopMutex;
    std::mutex mutex;
    std::condition_variable changed;
    std::condition_variable idle;
    std::deque<std::string> pending;
    std::size_t bytes = 0;
    std::size_t dropped = 0;
    bool writing = false;
    bool stopping = false;
    std::exception_ptr failure;
    std::thread worker;
};

inline std::atomic<ProfileOutput*> createdProfileOutput{nullptr};

inline ProfileOutput& ProfileOutput_nid_no_patch() {
    static ProfileOutput output([](std::string_view text) {
        if (std::fwrite(text.data(), 1, text.size(), stderr) != text.size() || std::fflush(stderr) != 0)
            throw std::runtime_error("profile output write failed");
    });
    createdProfileOutput.store(&output, std::memory_order_release);
    return output;
}

inline void StopProfileOutput_nid_no_patch() {
    if (auto* output = createdProfileOutput.load(std::memory_order_acquire)) output->Stop();
}

template<typename... TArgs>
void ProfilePrint_nid_no_patch(const char* format, TArgs... args) {
    ProfileOutput_nid_no_patch().Print(format, args...);
}

}
