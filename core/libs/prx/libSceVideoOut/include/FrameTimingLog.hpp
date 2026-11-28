#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_FRAMETIMINGLOG_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_FRAMETIMINGLOG_HPP

#include <array>
#include <chrono>
#include <condition_variable>
#include <exception>
#include <fstream>
#include <functional>
#include <ios>
#include <locale>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <thread>
#include <utility>
#include <vector>
#include "prx/libc/include/HostThread.hpp"

class FrameTimingLog {
public:
    using Report = std::function<void(std::ostream&)>;

    FrameTimingLog() {
        pending.reserve(maxReports);
        file.rdbuf()->pubsetbuf(buffer.data(), static_cast<std::streamsize>(buffer.size()));
        file.imbue(std::locale::classic());
        file.open("frame-timing.log", std::ios::out | std::ios::binary | std::ios::trunc);
        if (!file) throw std::ios_base::failure("opening frame-timing.log failed");
        worker = Libc::StartHostJThread([this](std::stop_token token) { run(token); });
    }

    ~FrameTimingLog() {
        {
            std::lock_guard lock(mutex);
            worker.request_stop();
        }
        changed.notify_one();
    }

    void Enqueue(Report report) {
        if (!report) throw std::invalid_argument("frame timing log report is empty");
        {
            std::lock_guard lock(mutex);
            if (failure != nullptr) std::rethrow_exception(failure);
            if (worker.get_stop_token().stop_requested()) throw std::runtime_error("frame timing log is closed");
            if (pending.size() == maxReports) throw std::runtime_error("frame timing log queue is full");
            pending.push_back(std::move(report));
        }
        changed.notify_one();
    }

    void Finish() {
        std::lock_guard finishLock(finishMutex);
        {
            std::lock_guard lock(mutex);
            worker.request_stop();
        }
        changed.notify_one();
        if (worker.joinable()) worker.join();
        std::exception_ptr error;
        {
            std::lock_guard lock(mutex);
            error = failure;
        }
        if (error != nullptr) std::rethrow_exception(error);
    }

private:
    void requireGood() const {
        if (!file) throw std::ios_base::failure("writing frame-timing.log failed");
    }

    void run(std::stop_token token) {
        try {
            std::vector<Report> reports;
            reports.reserve(maxReports);
            auto flushAt = std::chrono::steady_clock::now() + std::chrono::seconds(1);
            for (;;) {
                bool stopping = false;
                {
                    std::unique_lock lock(mutex);
                    changed.wait_until(lock, flushAt, [&] { return token.stop_requested() || !pending.empty(); });
                    reports.swap(pending);
                    stopping = token.stop_requested();
                }
                for (const auto& report : reports) report(file);
                reports.clear();
                requireGood();
                if (stopping) break;
                const auto now = std::chrono::steady_clock::now();
                if (now >= flushAt) {
                    file.flush();
                    requireGood();
                    flushAt = now + std::chrono::seconds(1);
                }
            }
            file.close();
            requireGood();
        } catch (...) {
            const auto current = std::current_exception();
            try {
                if (file.is_open()) file.close();
            } catch (...) {
            }
            std::lock_guard lock(mutex);
            pending.clear();
            failure = current;
        }
    }

    static constexpr std::size_t maxReports = 256;
    std::array<char, 65536> buffer{};
    std::ofstream file;
    std::mutex mutex;
    std::mutex finishMutex;
    std::condition_variable changed;
    std::vector<Report> pending;
    std::exception_ptr failure;
    std::jthread worker;
};

#endif
