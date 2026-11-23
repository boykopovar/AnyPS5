#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DIAGNOSTICS_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DIAGNOSTICS_HPP

#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace AgcDriver::DriverDetail {

double TraceMs();

bool SkippedShader(std::uint64_t address);

class RefusedDraw final : public std::runtime_error {
public:
    RefusedDraw(std::uint64_t address, const std::string& what) : std::runtime_error(what), address(address) {}
    std::uint64_t address;
};

class DrawCompilePending final : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

bool SkipRefusedDraws();
bool AsyncDrawCompile();
std::size_t AsyncComputePipelineWords();
std::chrono::milliseconds AsyncDispatchWait();
std::chrono::milliseconds AsyncComputeWait();

class SlidingWaitBudget {
public:
    using Clock = std::chrono::steady_clock;
    struct Grant {
        std::uint64_t id = 0;
        Clock::duration allowed{};
    };
    SlidingWaitBudget(Clock::duration perWait, Clock::duration window, Clock::duration total);
    Grant Take(Clock::time_point now);
    void Finish(const Grant& grant, Clock::time_point end);

private:
    Clock::duration perWait;
    Clock::duration window;
    Clock::duration total;
    std::mutex mutex;
    std::uint64_t nextId = 1;
    std::map<std::uint64_t, std::pair<Clock::time_point, Clock::time_point>> waits;
};

SlidingWaitBudget& ComputePipelineWaitBudget();

class BackgroundWaitScope {
public:
    BackgroundWaitScope();
    ~BackgroundWaitScope();
    BackgroundWaitScope(const BackgroundWaitScope&) = delete;
    BackgroundWaitScope& operator=(const BackgroundWaitScope&) = delete;

private:
    bool previous;
};

bool AwaitBackgroundJob(std::mutex& mutex, const bool& finished, bool& waited);
void NoteDispatchDropped(std::uint64_t address);
void NoteDispatchRecorded(std::uint64_t address);

void ReportRefusedDraw(const RefusedDraw& refused);
std::optional<std::size_t> RefusedShaderCount();

bool SkipRefusedDispatches();
void ReportRefusedDispatch(std::uint64_t address, std::string_view reason);
bool RefusedDispatch(std::uint64_t address);
std::size_t MaxComputeSpirvWords();

void require(bool condition, const char* reason);

std::uint32_t readRegister(const Registers& registers, std::uint32_t offset);

std::uint32_t readUserData(const Registers& shader, std::uint32_t offset);

}

#endif
