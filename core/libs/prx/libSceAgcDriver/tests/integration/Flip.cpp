#include <Testing/Test.hpp>
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <future>
#include <memory>
#include <mutex>
#include <source_location>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr std::uint32_t FlipHandle = 7;

enum class Mode { Flip, ComputeReset, GraphicsReset };

Mode CurrentMode() {
    const auto& arguments = Testing::Arguments();
    if (arguments.empty()) return Mode::Flip;
    return arguments[0] == "compute" ? Mode::ComputeReset : Mode::GraphicsReset;
}

void RequireMode(Mode mode) {
    if (CurrentMode() == mode) return;
    if (mode == Mode::Flip) Testing::Skip("runs only in the default registration");
    if (mode == Mode::ComputeReset) Testing::Skip("runs only in the compute registration");
    Testing::Skip("runs only in the graphics reset mode");
}

template<typename TAction>
std::string RequireRuntimeError(const TAction& action, std::string_view message, std::source_location location = std::source_location::current()) {
    return Testing::RequireThrows<std::runtime_error>(action, message, location).what();
}

template<typename TAction>
std::string RuntimeErrorOf(const TAction& action) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        return error.what();
    }
    return {};
}

bool Mentions(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

struct State {
    std::mutex mutex;
    std::condition_variable changed;
    bool block = false;
    bool entered = false;
    bool fail = false;
    bool checkSelfWait = false;
    std::string selfSuspendError;
    std::string selfWaitError;
    std::atomic<int> alive = 0;
    std::atomic<int> ready = 0;
    std::atomic<int> failed = 0;
    AgcDriver::FlipInfo last{};
};

class FlipRelease final {
public:
    explicit FlipRelease(std::shared_ptr<State> state) : state(std::move(state)) {}
    FlipRelease(const FlipRelease&) = delete;
    FlipRelease& operator=(const FlipRelease&) = delete;
    ~FlipRelease() { Release(); }

    void Release() {
        {
            std::lock_guard lock(state->mutex);
            state->block = false;
        }
        state->changed.notify_all();
    }

private:
    std::shared_ptr<State> state;
};

class Request final : public AgcDriver::IFlipRequest {
public:
    explicit Request(std::shared_ptr<State> value) : state(std::move(value)) { ++state->alive; }
    ~Request() override { --state->alive; }

    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override {
        std::unique_lock lock(state->mutex);
        if (state->checkSelfWait) {
            state->selfSuspendError = RuntimeErrorOf([] { AgcDriverSuspendPoint_nid_postfix(); });
            state->selfWaitError = RuntimeErrorOf([] { AgcDriverWaitIdle_nid_postfix(); });
        }
        state->entered = true;
        state->changed.notify_all();
        state->changed.wait(lock, [&] { return !state->block; });
        if (state->fail) throw std::runtime_error("intentional flip failure");
        ++state->ready;
    }

    void Fail(std::exception_ptr error) noexcept override {
        if (!error) std::terminate();
        ++state->failed;
    }

private:
    std::shared_ptr<State> state;
};

class Output final : public AgcDriver::IVideoOutput {
public:
    void Fail(std::exception_ptr error) noexcept override {
        if (!error) std::terminate();
    }

    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo& info) override {
        std::lock_guard lock(state->mutex);
        state->last = info;
        return std::make_shared<Request>(state);
    }

    std::shared_ptr<State> state = std::make_shared<State>();
};

class RegisteredOutput final {
public:
    RegisteredOutput() : output(std::make_shared<Output>()) {
        AgcDriverRegisterVideoOutput_nid_postfix(FlipHandle, output);
    }
    RegisteredOutput(const RegisteredOutput&) = delete;
    RegisteredOutput& operator=(const RegisteredOutput&) = delete;
    ~RegisteredOutput() {
        try {
            Unregister();
        } catch (...) {
        }
    }

    void Unregister() {
        if (!registered) return;
        registered = false;
        AgcDriverUnregisterVideoOutput_nid_postfix(FlipHandle, output);
    }

    const std::shared_ptr<Output>& Get() const noexcept { return output; }
    State& Flips() const noexcept { return *output->state; }

private:
    std::shared_ptr<Output> output;
    bool registered = true;
};

void submitFlip(std::uint32_t handle = FlipHandle, std::source_location location = std::source_location::current()) {
    std::array<std::uint32_t, 6> words{0xc004105c, handle, 0xfffffffeu, 1, 0x76543211u, 0xfedcba98u};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    RequireEqual(sceAgcDriverSubmitDcb(&packet), 0, "flip submission result", location);
}

void requireNoReservation(const State& state, std::string_view what) {
    Require(state.alive == 0 && state.ready == 0, std::string(what) + " retained or executed a reservation");
}

void blockNextFlip(State& state, bool checkSelfWait) {
    std::lock_guard lock(state.mutex);
    state.block = true;
    state.checkSelfWait = checkSelfWait;
}

void waitUntilFlipEntered(State& state) {
    std::unique_lock lock(state.mutex);
    Require(state.changed.wait_for(lock, std::chrono::seconds(5), [&] { return state.entered; }), "worker did not reach flip");
}

const Case duplicateRegistration{"RegisterVideoOutput_DuplicateHandle_Throws", [] {
    RequireMode(Mode::Flip);
    const RegisteredOutput output;
    Testing::RequireThrows<std::runtime_error>([&] { AgcDriverRegisterVideoOutput_nid_postfix(FlipHandle, output.Get()); },
                                               "a second registration of the same handle was accepted");
}};

const Case malformedFlips{"SubmitDcb_MalformedFlip_IsRejectedWithoutReservation", [] {
    RequireMode(Mode::Flip);
    const RegisteredOutput output;
    std::array<std::uint32_t, 6> words{0xc004105c, FlipHandle, 0xfffffffeu, 1, 0, 0};
    Packet packet{words.data(), 6, 0, {}};
    Testing::RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitAcb(0x20, &packet); }, "a flip on a compute queue was accepted");
    words[0] = 0xc004105d;
    Testing::RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a flip with a wrong header was accepted");
    words[0] = 0xc004105c;
    packet.dw_num = 5;
    Testing::RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&packet); }, "a truncated flip was accepted");
    Testing::RequireThrows<std::runtime_error>([] { submitFlip(8); }, "a flip to an unregistered handle was accepted");
    std::array<std::uint32_t, 12> rollback{0xc004105c, FlipHandle, 0, 1, 0, 0, 0xc004105c, 8, 0, 1, 0, 0};
    Packet rejected{rollback.data(), 12, 0, {}};
    Testing::RequireThrows<std::runtime_error>([&] { sceAgcDriverSubmitDcb(&rejected); }, "a submission ending with an invalid flip was accepted");
    requireNoReservation(output.Flips(), "a rejected submission");
}};

const Case conditionalFlip{"SubmitDcb_FlipInsideCondExec_IsRejectedWithoutReservation", [] {
    RequireMode(Mode::Flip);
    const RegisteredOutput output;
    alignas(4) static std::uint32_t condition = 1;
    const auto conditionAddress = reinterpret_cast<std::uintptr_t>(&condition);
    std::array<std::uint32_t, 11> guarded{0xc0032200, static_cast<std::uint32_t>(conditionAddress), static_cast<std::uint32_t>(conditionAddress >> 32u), 0, 6, 0xc004105c, FlipHandle, 0, 1, 0, 0};
    Packet guardedFlip{guarded.data(), 11, 0, {}};
    const auto error = RequireRuntimeError([&] { sceAgcDriverSubmitDcb(&guardedFlip); }, "a flip inside a COND_EXEC range was accepted");
    Require(Mentions(error, "a flip inside a conditional execution range"), "a flip inside a COND_EXEC range was rejected for another reason: " + error);
    requireNoReservation(output.Flips(), "a rejected conditional flip");
}};

const Case blockedFlip{"SuspendPoint_DuringBlockedFlip_DoesNotWaitForIt", [] {
    RequireMode(Mode::Flip);
    const RegisteredOutput output;
    auto& state = output.Flips();
    blockNextFlip(state, true);
    FlipRelease release(output.Get()->state);
    submitFlip();
    waitUntilFlipEntered(state);
    {
        std::lock_guard lock(state.mutex);
        RequireEqual(state.last.argument, -0x123456789abcdefLL, "decoded flip argument");
        RequireEqual(state.last.index, -2, "decoded flip index");
        Require(Mentions(state.selfSuspendError, "itself"), "a suspend point from the flip worker was not rejected: " + state.selfSuspendError);
        Require(Mentions(state.selfWaitError, "itself"), "a wait for idle from the flip worker was not rejected: " + state.selfWaitError);
    }
    auto boundary = std::async(std::launch::async, [] { AgcDriverSuspendPoint_nid_postfix(); });
    Require(boundary.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "suspend blocked on preceding work");
    boundary.get();
    RequireEqual(state.ready.load(), 0, "flips completed before the blocked flip was released");
    release.Release();
    AgcDriverWaitIdle_nid_postfix();
    RequireEqual(state.ready.load(), 1, "completed flips after release");
    RequireEqual(state.failed.load(), 0, "failed flips");
}};

const Case replacedOutput{"UnregisterVideoOutput_DuringBlockedFlip_KeepsRequestAndFifo", [] {
    RequireMode(Mode::Flip);
    RegisteredOutput output;
    auto& state = output.Flips();
    blockNextFlip(state, false);
    FlipRelease release(output.Get()->state);
    submitFlip();
    waitUntilFlipEntered(state);
    output.Unregister();
    const RegisteredOutput replacement;
    submitFlip();
    release.Release();
    AgcDriverWaitIdle_nid_postfix();
    RequireEqual(state.ready.load(), 1, "flips completed by the unregistered output");
    RequireEqual(replacement.Flips().ready.load(), 1, "flips completed by the replacement output");
    RequireEqual(state.failed.load(), 0, "failed flips of the unregistered output");
}};

const Case concurrentFlips{"SubmitDcb_ConcurrentProducers_DeliverEveryFlip", [] {
    RequireMode(Mode::Flip);
    const RegisteredOutput output;
    std::vector<std::thread> producers;
    std::array<std::exception_ptr, 4> errors{};
    for (std::size_t i = 0; i < errors.size(); ++i) {
        producers.emplace_back([&, i] {
            try {
                for (int j = 0; j < 50; ++j) {
                    submitFlip();
                    if (j % 5 == 0) AgcDriverSuspendPoint_nid_postfix();
                }
            } catch (...) {
                errors[i] = std::current_exception();
            }
        });
    }
    for (auto& producer : producers) producer.join();
    for (const auto& error : errors) {
        if (error) std::rethrow_exception(error);
    }
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
    RequireEqual(output.Flips().ready.load(), 200, "completed concurrent flips");
}};

const Case failedFlip{"WaitIdle_FailedFlip_PropagatesToEveryEntryPoint", [] {
    RequireMode(Mode::Flip);
    const RegisteredOutput output;
    output.Flips().fail = true;
    submitFlip();
    std::array<std::string, 4> messages;
    std::vector<std::thread> waiters;
    for (auto& message : messages) waiters.emplace_back([&message] { message = RuntimeErrorOf([] { AgcDriverWaitIdle_nid_postfix(); }); });
    for (auto& waiter : waiters) waiter.join();
    for (const auto& message : messages) RequireEqual(message, std::string("intentional flip failure"), "failure seen by a concurrent waiter");
    RequireEqual(RequireRuntimeError([] { AgcDriverWaitIdle_nid_postfix(); }, "idle lost flip failure"), messages[0], "failure reported by idle");
    RequireEqual(RequireRuntimeError([] { AgcDriverSuspendPoint_nid_postfix(); }, "suspend lost flip failure"), messages[0], "failure reported by suspend");
    RequireEqual(RequireRuntimeError([] { submitFlip(); }, "submit lost flip failure"), messages[0], "failure reported by submit");
    RequireEqual(output.Flips().ready.load(), 0, "completed flips of a failed flip");
    RequireEqual(output.Flips().failed.load(), 1, "failed flip requests");
}};

void submitToQueue(bool compute, Packet& packet) {
    if (compute) sceAgcDriverSubmitAcb(0x20, &packet);
    else sceAgcDriverSubmitDcb(&packet);
}

void requireSuspendResetsQueueState(bool compute) {
    std::array<std::uint32_t, 4> registers{0xc0027600, 0x20c, 1, 0};
    Packet packet{registers.data(), 4, 0, {}};
    submitToQueue(compute, packet);
    AgcDriverSuspendPoint_nid_postfix();
    std::array<std::uint32_t, 5> dispatch{0xc0031500, 1, 1, 1, 0x41};
    packet = Packet{dispatch.data(), 5, 0, {}};
    submitToQueue(compute, packet);
    const auto message = RequireRuntimeError([] { AgcDriverWaitIdle_nid_postfix(); }, "a dispatch after a suspend point kept the previous queue state");
    Require(Mentions(message, compute ? "not readable" : "required shader register"), "suspend reset wrong queue state: " + message);
}

const Case computeReset{"SuspendPoint_ComputeQueue_ResetsQueueState", [] {
    RequireMode(Mode::ComputeReset);
    requireSuspendResetsQueueState(true);
}};

const Case graphicsReset{"SuspendPoint_GraphicsQueue_ResetsQueueState", [] {
    RequireMode(Mode::GraphicsReset);
    requireSuspendResetsQueueState(false);
}};

const Case shutdown{"LibcRunShutdown_AfterWorkerFailure_ReportsIt", [] {
    const auto mode = CurrentMode();
    const std::string_view expected = mode == Mode::Flip ? "intentional flip failure" : mode == Mode::ComputeReset ? "not readable" : "required shader register";
    const auto message = RequireRuntimeError([] { LibcRunShutdown_nid_postfix(); }, "shutdown lost worker failure");
    Require(Mentions(message, expected), "shutdown reported another failure: " + message);
}};

} // namespace
