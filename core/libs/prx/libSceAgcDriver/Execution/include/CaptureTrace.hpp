#pragma once

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>

namespace AgcDriver {

class CaptureTrace {
public:
    static bool Enabled() {
        static const bool enabled = std::getenv("APS5_CAPTURE_TRACE") != nullptr;
        return enabled;
    }

    template<typename... TArgs>
    static void Log(const char* format, TArgs... args) {
        if (!Enabled()) return;
        std::array<char, 2048> line{};
        const auto size = std::snprintf(line.data(), line.size(), format, args...);
        if (size < 0 || static_cast<std::size_t>(size) >= line.size()) throw std::runtime_error("Capture trace event exceeds 2047 bytes");
        static CaptureTrace& trace = instance();
        trace.append(std::string(line.data(), static_cast<std::size_t>(size)));
    }

private:
    static CaptureTrace& instance() {
        static CaptureTrace trace;
        return trace;
    }

    CaptureTrace() {
        output.exceptions(std::ios::badbit | std::ios::failbit);
        output.open("capture-trace.log", std::ios::binary);
        worker = std::thread([this] { run(); });
    }

    ~CaptureTrace() {
        {
            std::lock_guard lock(mutex);
            stopping = true;
        }
        changed.notify_one();
        worker.join();
    }

    void append(std::string line) {
        const auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() - start).count();
        std::lock_guard lock(mutex);
        if (pending.size() + line.size() + 64 > 16 * 1024 * 1024) throw std::runtime_error("Capture trace queue exceeded 16 MiB");
        pending += std::to_string(++sequence) + " " + std::to_string(elapsed) + "us " + line + "\n";
        if (line.starts_with("blit ")) flushRequested = true;
        if (pending.size() >= 65536 || flushRequested) changed.notify_one();
    }

    void run() {
        try {
            for (;;) {
                std::string batch;
                {
                    std::unique_lock lock(mutex);
                    changed.wait_for(lock, std::chrono::milliseconds(100), [this] { return stopping || flushRequested || pending.size() >= 65536; });
                    if (stopping && pending.empty()) return;
                    batch.swap(pending);
                    flushRequested = false;
                }
                if (batch.empty()) continue;
                output.write(batch.data(), static_cast<std::streamsize>(batch.size()));
                output.flush();
            }
        } catch (const std::exception& error) {
            std::fprintf(stderr, "Capture trace failed: %s\n", error.what());
            std::terminate();
        }
    }

    std::ofstream output;
    std::mutex mutex;
    std::condition_variable changed;
    std::string pending;
    std::thread worker;
    std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();
    unsigned long long sequence = 0;
    bool stopping = false;
    bool flushRequested = false;
};

}
