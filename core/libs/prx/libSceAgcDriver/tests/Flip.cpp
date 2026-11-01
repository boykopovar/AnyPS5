#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/PresentMode.hpp"
#include "prx/libSceAgcDriver/Execution/include/SwapchainImages.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <future>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
#ifdef _WIN32
#include <io.h>
#define APS5_DUP _dup
#define APS5_DUP2 _dup2
#define APS5_CLOSE _close
#define APS5_FILENO _fileno
#else
#include <unistd.h>
#define APS5_DUP dup
#define APS5_DUP2 dup2
#define APS5_CLOSE close
#define APS5_FILENO fileno
#endif

namespace {

void check(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(reason);
}

template<typename TAction>
std::string expectFailure(TAction action) {
    try { action(); }
    catch (const std::runtime_error& error) { return error.what(); }
    throw std::runtime_error("expected an exception");
}

struct State {
    std::mutex mutex;
    std::condition_variable changed;
    bool block = false;
    bool entered = false;
    bool fail = false;
    bool checkSelfWait = false;
    bool gate = false;
    bool waiting = false;
    std::atomic<int> alive = 0;
    std::atomic<int> ready = 0;
    std::atomic<int> failed = 0;
    AgcDriver::FlipInfo last{};
};

class FlipRelease final {
public:
    explicit FlipRelease(std::shared_ptr<State> state) : state(std::move(state)) {}
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
            check(expectFailure([] { AgcDriverSuspendPoint_nid_postfix(); }).find("itself") != std::string::npos, "self suspend was not rejected");
            check(expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }).find("itself") != std::string::npos, "self wait was not rejected");
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

class RenderingGate final : public AgcDriver::IRenderingWait {
public:
    explicit RenderingGate(std::shared_ptr<State> value) : state(std::move(value)) {}
    void Wait() override {
        std::unique_lock lock(state->mutex);
        state->waiting = true;
        state->changed.notify_all();
        state->changed.wait(lock, [&] { return !state->gate; });
    }
private:
    std::shared_ptr<State> state;
};

class Output final : public AgcDriver::IVideoOutput {
public:
    void Fail(std::exception_ptr error) noexcept override { if (!error) std::terminate(); }
    std::shared_ptr<State> state = std::make_shared<State>();
    std::shared_ptr<AgcDriver::IRenderingWait> CaptureRenderingWait(std::uint32_t) override { return std::make_shared<RenderingGate>(state); }
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo& info) override {
        std::lock_guard lock(state->mutex);
        state->last = info;
        return std::make_shared<Request>(state);
    }
};

void submitFlip(std::uint32_t handle = 7) {
    std::array<std::uint32_t, 6> words{0xc004105c, handle, 0xfffffffeu, 1, 0x76543211u, 0xfedcba98u};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&packet) == 0, "flip submission failed");
}

void testFlipAndBoundary() {
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    expectFailure([&] { AgcDriverRegisterVideoOutput_nid_postfix(7, output); });
    std::array<std::uint32_t, 6> words{0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet packet{words.data(), 6, 0, {}};
    words[0] = 0xc004105d;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    words[0] = 0xc004105c;
    packet.dw_num = 5;
    expectFailure([&] { sceAgcDriverSubmitDcb(&packet); });
    expectFailure([] { submitFlip(8); });
    std::array<std::uint32_t, 12> rollback{0xc004105c, 7, 0, 1, 0, 0, 0xc004105c, 8, 0, 1, 0, 0};
    Packet rejected{rollback.data(), 12, 0, {}};
    expectFailure([&] { sceAgcDriverSubmitDcb(&rejected); });
    check(output->state->alive == 0 && output->state->ready == 0, "rejected submission retained or executed a reservation");
    alignas(4) static std::uint32_t condition = 1;
    const auto conditionAddress = reinterpret_cast<std::uintptr_t>(&condition);
    std::array<std::uint32_t, 11> guarded{0xc0032200, static_cast<std::uint32_t>(conditionAddress), static_cast<std::uint32_t>(conditionAddress >> 32u), 0, 6, 0xc004105c, 7, 0, 1, 0, 0};
    Packet conditionalFlip{guarded.data(), 11, 0, {}};
    check(expectFailure([&] { sceAgcDriverSubmitDcb(&conditionalFlip); }).find("a flip inside a conditional execution range") != std::string::npos, "a flip inside a COND_EXEC range was not rejected");
    check(output->state->alive == 0 && output->state->ready == 0, "a rejected conditional flip retained or executed a reservation");
    {
        std::lock_guard lock(output->state->mutex);
        output->state->block = true;
        output->state->checkSelfWait = true;
    }
    std::future<void> boundary;
    FlipRelease release(output->state);
    submitFlip();
    {
        std::unique_lock lock(output->state->mutex);
        check(output->state->changed.wait_for(lock, std::chrono::seconds(5), [&] { return output->state->entered; }), "worker did not reach flip");
        check(output->state->last.argument == -0x123456789abcdefLL && output->state->last.index == -2, "decoded flip arguments changed");
    }
    boundary = std::async(std::launch::async, [] { AgcDriverSuspendPoint_nid_postfix(); });
    check(boundary.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "suspend blocked on preceding work");
    boundary.get();
    check(output->state->ready == 0, "blocked flip completed before release");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    auto replacement = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, replacement);
    submitFlip();
    release.Release();
    AgcDriverWaitIdle_nid_postfix();
    check(output->state->ready == 1 && replacement->state->ready == 1, "registration lifetime or FIFO was lost");
    check(output->state->failed == 0, "successful request failed");
    std::vector<std::thread> producers;
    std::array<std::exception_ptr, 4> errors{};
    for (std::size_t i = 0; i < errors.size(); ++i) {
        producers.emplace_back([&, i] {
            try {
                for (int j = 0; j < 50; ++j) {
                    submitFlip();
                    if (j % 5 == 0) AgcDriverSuspendPoint_nid_postfix();
                }
            } catch (...) { errors[i] = std::current_exception(); }
        });
    }
    for (auto& producer : producers) producer.join();
    for (auto error : errors) if (error) std::rethrow_exception(error);
    AgcDriverSuspendPoint_nid_postfix();
    AgcDriverWaitIdle_nid_postfix();
    check(replacement->state->ready == 201, "concurrent submissions were lost");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, replacement);
}

void testFlipHold() {
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    {
        std::lock_guard lock(output->state->mutex);
        output->state->gate = true;
    }
    struct GateRelease {
        std::shared_ptr<State> state;
        ~GateRelease() {
            {
                std::lock_guard lock(state->mutex);
                state->gate = false;
            }
            state->changed.notify_all();
        }
    } gateRelease{output->state};
    std::array<std::uint32_t, 10> gated{0xc0021018, 7, 0, 0, 0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet gatedFrame{gated.data(), static_cast<std::uint32_t>(gated.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&gatedFrame) == 0, "gated flip submission failed");
    {
        std::unique_lock lock(output->state->mutex);
        check(output->state->changed.wait_for(lock, std::chrono::seconds(5), [&] { return output->state->waiting; }), "worker did not reach the rendering wait");
    }
    auto held = std::async(std::launch::async, [] { submitFlip(); });
    check(held.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout, "a flip was accepted before the worker reached the previous one");
    check(output->state->ready == 0, "a flip passed a closed rendering wait");
    {
        std::lock_guard lock(output->state->mutex);
        output->state->gate = false;
    }
    output->state->changed.notify_all();
    check(held.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "a flip stayed held after the worker reached the previous one");
    held.get();
    AgcDriverWaitIdle_nid_postfix();
    check(output->state->ready == 2, "held flips were lost");
    alignas(4) static std::atomic<std::uint32_t> label = 0;
    struct LabelRelease {
        ~LabelRelease() { label.store(1, std::memory_order_release); }
    } labelRelease;
    const auto labelAddress = reinterpret_cast<std::uintptr_t>(&label);
    std::array<std::uint32_t, 13> waiting{0xc0053c00, 0x13, static_cast<std::uint32_t>(labelAddress), static_cast<std::uint32_t>(static_cast<std::uint64_t>(labelAddress) >> 32u), 1, 0xffffffffu, 0x19, 0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet waitingFrame{waiting.data(), static_cast<std::uint32_t>(waiting.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&waitingFrame) == 0, "waiting flip submission failed");
    auto released = std::async(std::launch::async, [] { submitFlip(); });
    check(released.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "a flip was held while the worker waited for a label only the title stores");
    released.get();
    check(output->state->ready == 2, "a flip passed an unsatisfied wait");
    label.store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    check(output->state->ready == 4, "flips behind a title-stored label were lost");
    {
        std::lock_guard lock(output->state->mutex);
        output->state->gate = true;
        output->state->waiting = false;
    }
    alignas(4) static std::atomic<std::uint32_t> later = 0;
    struct LaterRelease {
        ~LaterRelease() { later.store(1, std::memory_order_release); }
    } laterRelease;
    const auto laterAddress = reinterpret_cast<std::uintptr_t>(&later);
    std::array<std::uint32_t, 17> gatedWait{0xc0021018, 7, 0, 0, 0xc0053c00, 0x13, static_cast<std::uint32_t>(laterAddress), static_cast<std::uint32_t>(static_cast<std::uint64_t>(laterAddress) >> 32u), 1, 0xffffffffu, 0x19, 0xc004105c, 7, 0xfffffffeu, 1, 0, 0};
    Packet gatedWaitFrame{gatedWait.data(), static_cast<std::uint32_t>(gatedWait.size()), 0, {}};
    check(sceAgcDriverSubmitDcb(&gatedWaitFrame) == 0, "gated waiting flip submission failed");
    {
        std::unique_lock lock(output->state->mutex);
        check(output->state->changed.wait_for(lock, std::chrono::seconds(5), [&] { return output->state->waiting; }), "worker did not reach the second rendering wait");
    }
    auto progressed = std::async(std::launch::async, [] { submitFlip(); });
    check(progressed.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout, "a flip was accepted while the worker was held before a label wait");
    {
        std::lock_guard lock(output->state->mutex);
        output->state->gate = false;
    }
    output->state->changed.notify_all();
    check(progressed.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "a flip stayed held after the worker went on to wait for a label only the title stores");
    progressed.get();
    check(output->state->ready == 4, "a flip passed an unsatisfied wait after the rendering wait");
    later.store(1, std::memory_order_release);
    AgcDriverWaitIdle_nid_postfix();
    check(output->state->ready == 6, "flips behind a later title-stored label were lost");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testComputeRenderingWait() {
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, 4> wait{0xc0021018, 8, 0, 0};
    Packet packet{wait.data(), 4, 0, {}};
    check(expectFailure([&] { sceAgcDriverSubmitAcb(0x20, &packet); }).find("unregistered video output") != std::string::npos, "a compute rendering wait on an unregistered output was accepted");
    wait = {0xc0021018, 7, 0, 1};
    check(expectFailure([&] { sceAgcDriverSubmitAcb(0x20, &packet); }).find("rendering wait mode") != std::string::npos, "a compute rendering wait with a mode was accepted");
    {
        std::lock_guard lock(output->state->mutex);
        output->state->gate = true;
    }
    struct GateRelease {
        std::shared_ptr<State> state;
        ~GateRelease() {
            {
                std::lock_guard lock(state->mutex);
                state->gate = false;
            }
            state->changed.notify_all();
        }
    } gateRelease{output->state};
    wait = {0xc0021018, 7, 0, 0};
    check(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "compute rendering wait submission failed");
    {
        std::unique_lock lock(output->state->mutex);
        check(output->state->changed.wait_for(lock, std::chrono::seconds(5), [&] { return output->state->waiting; }), "compute worker did not reach the rendering wait");
    }
    auto idle = std::async(std::launch::async, [] { AgcDriverWaitIdle_nid_postfix(); });
    check(idle.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout, "the driver went idle past a closed compute rendering wait");
    {
        std::lock_guard lock(output->state->mutex);
        output->state->gate = false;
    }
    output->state->changed.notify_all();
    check(idle.wait_for(std::chrono::seconds(5)) == std::future_status::ready, "a compute rendering wait stayed blocked after release");
    idle.get();
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testComputeFlip() {
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, 6> words{0xc004105c, 7, 0xfffffffeu, 1, 0x76543211u, 0xfedcba98u};
    Packet packet{words.data(), static_cast<std::uint32_t>(words.size()), 0, {}};
    check(sceAgcDriverSubmitAcb(0x20, &packet) == 0, "compute flip submission failed");
    AgcDriverWaitIdle_nid_postfix();
    check(output->state->ready == 1 && output->state->failed == 0, "a flip submitted on a compute queue was not handed to its output");
    check(output->state->last.argument == -0x123456789abcdefLL && output->state->last.index == -2 && output->state->last.mode == 1, "decoded compute flip arguments changed");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testFailure() {
    auto output = std::make_shared<Output>();
    output->state->fail = true;
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    submitFlip();
    std::array<std::string, 4> messages;
    std::vector<std::thread> waiters;
    for (auto& message : messages) waiters.emplace_back([&message] { message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }); });
    for (auto& waiter : waiters) waiter.join();
    for (auto& message : messages) check(message == "intentional flip failure", "asynchronous failure was lost");
    check(expectFailure([] { AgcDriverWaitIdle_nid_postfix(); }) == messages[0], "idle lost flip failure");
    check(expectFailure([] { AgcDriverSuspendPoint_nid_postfix(); }) == messages[0], "suspend lost flip failure");
    check(expectFailure([] { submitFlip(); }) == messages[0], "submit lost flip failure");
    check(output->state->ready == 0 && output->state->failed == 1, "failed flip was completed successfully");
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
}

void testFailureLog() {
    const auto path = std::filesystem::temp_directory_path() / ("aps5_worker_failure_log_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt");
    std::fflush(stderr);
    const int saved = APS5_DUP(APS5_FILENO(stderr));
    check(saved >= 0, "stderr could not be saved");
#ifdef _WIN32
    check(_wfreopen(path.wstring().c_str(), L"wb", stderr) != nullptr, "stderr could not be redirected");
#else
    check(std::freopen(path.c_str(), "wb", stderr) != nullptr, "stderr could not be redirected");
#endif
    std::string message;
    std::string error;
    try {
        auto output = std::make_shared<Output>();
        output->state->fail = true;
        AgcDriverRegisterVideoOutput_nid_postfix(7, output);
        submitFlip();
        message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); });
        check(expectFailure([] { submitFlip(); }) == message, "the stored failure was not rethrown on the next driver call");
        AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    } catch (const std::exception& caught) {
        error = caught.what();
    }
    std::fflush(stderr);
    APS5_DUP2(saved, APS5_FILENO(stderr));
    APS5_CLOSE(saved);
    std::string captured;
    {
        std::ifstream file(path, std::ios::binary);
        captured.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    }
    std::filesystem::remove(path);
    if (!error.empty()) throw std::runtime_error(error);
    check(message == "intentional flip failure", "the worker failure was not rethrown");
    check(captured.find("[gpu] driver failure: intentional flip failure\n") != std::string::npos, "the worker failure was not printed");
    check(captured.find("driver failure") == captured.rfind("driver failure"), "the worker failure was printed more than once");
}

void testReset(bool compute) {
    std::array<std::uint32_t, 4> registers{0xc0027600, 0x20c, 1, 0};
    Packet packet{registers.data(), 4, 0, {}};
    if (compute) sceAgcDriverSubmitAcb(0x20, &packet);
    else sceAgcDriverSubmitDcb(&packet);
    AgcDriverSuspendPoint_nid_postfix();
    std::array<std::uint32_t, 5> dispatch{0xc0031500, 1, 1, 1, 0x41};
    packet = Packet{dispatch.data(), 5, 0, {}};
    if (compute) sceAgcDriverSubmitAcb(0x20, &packet);
    else sceAgcDriverSubmitDcb(&packet);
    const auto message = expectFailure([] { AgcDriverWaitIdle_nid_postfix(); });
    check(message.find(compute ? "not readable" : "required shader register") != std::string::npos, "suspend reset wrong queue state");
}

}

void testPresentMode() {
    using AgcDriver::SelectPresentMode;
    VkSurfaceCapabilitiesKHR surface{};
    surface.minImageCount = 2;
    surface.maxImageCount = 8;
    check(AgcDriver::SwapchainImageCount(surface) == 3, "the swapchain has no image beyond the surface minimum");
    surface.minImageCount = 3;
    check(AgcDriver::SwapchainImageCount(surface) == 3, "a three-image surface minimum gained an image");
    surface.minImageCount = 5;
    check(AgcDriver::SwapchainImageCount(surface) == 5, "the swapchain does not follow a larger surface minimum");
    surface.minImageCount = 2;
    surface.maxImageCount = 2;
    check(AgcDriver::SwapchainImageCount(surface) == 2, "the swapchain exceeds the surface maximum");
    surface.maxImageCount = 0;
    check(AgcDriver::SwapchainImageCount(surface) == 3, "an unlimited surface maximum capped the swapchain");
    const std::array wayland{VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR};
    const std::array all{VK_PRESENT_MODE_IMMEDIATE_KHR, VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR};
    check(SelectPresentMode(nullptr, wayland) == VK_PRESENT_MODE_FIFO_KHR, "the default present mode is not FIFO");
    check(SelectPresentMode("fifo", wayland) == VK_PRESENT_MODE_FIFO_KHR, "fifo was not selected");
    check(SelectPresentMode("immediate", wayland) == VK_PRESENT_MODE_IMMEDIATE_KHR, "immediate was not selected");
    check(SelectPresentMode("mailbox", all) == VK_PRESENT_MODE_MAILBOX_KHR, "mailbox was not selected");
    const std::array latest{VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_FIFO_LATEST_READY_KHR};
    check(SelectPresentMode("fifo_latest_ready", latest) == VK_PRESENT_MODE_FIFO_LATEST_READY_KHR, "fifo_latest_ready was not selected");
    check(SelectPresentMode(nullptr, latest) == VK_PRESENT_MODE_FIFO_KHR, "the default changed on a surface with fifo_latest_ready");
    check(expectFailure([&] { SelectPresentMode("fifo_latest_ready", wayland); }).find("does not support") != std::string::npos, "fifo_latest_ready was accepted on a surface without it");
    check(expectFailure([&] { SelectPresentMode("mailbox", wayland); }).find("does not support") != std::string::npos, "an unsupported present mode was accepted");
    check(expectFailure([&] { SelectPresentMode("Immediate", all); }).find("must be fifo, fifo_latest_ready, mailbox or immediate") != std::string::npos, "an unknown present mode name was accepted");
    check(expectFailure([&] { SelectPresentMode("", all); }).find("must be fifo, fifo_latest_ready, mailbox or immediate") != std::string::npos, "an empty present mode name was accepted");
    const std::array<VkPresentModeKHR, 0> none{};
    check(expectFailure([&] { SelectPresentMode(nullptr, none); }).find("does not support") != std::string::npos, "FIFO was selected on a surface without it");
    using AgcDriver::SelectPresentQueue;
    check(SelectPresentQueue(16, 2) == 2u, "the presentation queue does not follow the main and settle queues");
    check(SelectPresentQueue(16, 1) == 1u, "the presentation queue does not follow the main queue alone");
    check(!SelectPresentQueue(2, 2).has_value(), "a presentation queue was taken from a family without a free queue");
    check(!SelectPresentQueue(1, 1).has_value(), "a presentation queue was taken from a family with one queue");
}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "present-mode") {
            testPresentMode();
            std::puts("present mode selection tests passed");
            return 0;
        }
        if (argc == 2 && std::string(argv[1]) == "failure-log") {
            testFailureLog();
            const auto shutdown = expectFailure([] { LibcRunShutdown_nid_postfix(); });
            check(shutdown == "intentional flip failure", "shutdown lost worker failure");
            std::puts("AGC worker failure log test passed");
            return 0;
        }
        if (argc == 2) testReset(std::string(argv[1]) == "compute");
        else { testFlipAndBoundary(); testFlipHold(); testComputeRenderingWait(); testComputeFlip(); testFailure(); }
        const auto shutdown = expectFailure([] { LibcRunShutdown_nid_postfix(); });
        check(shutdown.find(argc == 2 ? (std::string(argv[1]) == "compute" ? "not readable" : "required shader register") : "intentional flip failure") != std::string::npos, "shutdown lost worker failure");
        std::puts("AGC flip and suspend tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        try { LibcRunShutdown_nid_postfix(); }
        catch (const std::exception& shutdown) { std::fprintf(stderr, "shutdown: %s\n", shutdown.what()); }
        return 1;
    }
}
