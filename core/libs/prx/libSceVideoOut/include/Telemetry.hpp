#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_TELEMETRY_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_TELEMETRY_HPP

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>

#include "prx/libScePad/include/PadState.hpp"

namespace VideoOutTelemetry {

class BoundedLineQueue {
public:
    explicit BoundedLineQueue(std::size_t capacity);
    bool TryPush(std::string line);
    bool Push(std::string line);
    std::deque<std::string> Drain();
    std::uint64_t Dropped() const;

private:
    bool pushLocked(std::string line);

    std::mutex mutex;
    std::deque<std::string> lines;
    std::size_t capacity;
    std::atomic<std::uint64_t> dropped{0};
};

class Writer {
public:
    Writer(const std::string& directory, std::size_t capacity, const std::string& commit);
    ~Writer();
    Writer(const Writer&) = delete;
    Writer& operator=(const Writer&) = delete;

    bool EmitPad(std::uint64_t frame, const PadInputState& state);
    void EmitPresent(std::uint64_t frame, double fps);
    std::uint64_t Dropped() const;

private:
    bool emit(std::string line, bool mustKeep);
    void run(std::stop_token token);
    void writeAll(const std::string& text);
    void writeDrops(bool force);

    std::ofstream output;
    BoundedLineQueue queue;
    std::uint64_t reportedDrops = 0;
    std::jthread thread;
};

class PadChangeFilter {
public:
    bool Changed(const PadInputState& state) const;
    void Commit(const PadInputState& state);

private:
    PadInputState last{};
};

bool EmitPadIfChanged(Writer& writer, PadChangeFilter& filter, std::uint64_t frame, const PadInputState& state);

std::unique_ptr<Writer> OpenFromEnvironment(const char* directory, const std::string& commit);
Writer* Instance();

}

#endif
