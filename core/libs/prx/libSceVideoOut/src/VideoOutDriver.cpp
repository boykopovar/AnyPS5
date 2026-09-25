#include <bit>
#include <chrono>
#include <cstdlib>
#include <limits>
#include <stdexcept>

#include "SDL.h"
#include "SDL_vulkan.h"
#include "prx/libSceVideoOut/include/PadInput.hpp"
#include "prx/libScePad/include/PadState.hpp"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/PerformanceTimer.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"

namespace {

void require(bool condition, const char* reason) {
    if (!condition) throw std::runtime_error(std::string("VideoOut: ") + reason);
}

void checkConfig(const VideoOutConfig& cfg) {
    cfg.Check();
}

class VideoOutput final : public AgcDriver::IVideoOutput {
public:
    VideoOutput(std::shared_ptr<VideoOutConfig> config, std::shared_ptr<FlipQueue> requests) : cfg(std::move(config)), queue(std::move(requests)) {}

    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo& info) override {
        // All flip modes are presented at the next vsync.
        require(info.mode >= VIDEO_OUT_FLIP_MODE_VSYNC && info.mode <= 6, "unsupported flip mode");
        require(info.index >= VIDEO_OUT_BUFFER_INDEX_BLACK && info.index < VIDEO_OUT_BUFFER_NUM_MAX, "invalid flip index");
        auto request = std::make_shared<FlipRequest>();
        request->cfg = cfg;
        request->queue = queue;
        request->index = info.index;
        request->outputHandle = info.handle;
        request->flipMode = static_cast<int>(info.mode);
        request->flipArg = info.argument;
        std::lock_guard queueLock(queue->mutex);
        if (queue->failure) std::rethrow_exception(queue->failure);
        require(!queue->stopping, "flip during shutdown");
        std::lock_guard lock(cfg->mutex);
        checkConfig(*cfg);
        require(queue->reservations.load() < VIDEO_OUT_FLIP_QUEUE_CAPACITY, "flip queue full");
        if (info.index >= 0) {
            request->buffer = cfg->buffers[info.index];
            require(request->buffer.Occupied(), "flip buffer is not registered");
            require(request->buffer.groupIndex < VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX, "invalid buffer group");
            request->group = cfg->groups[request->buffer.groupIndex];
            require(request->group.occupied, "buffer group is not registered");
            require(request->buffer.dataAddress != 0, "null registered buffer address");
            static_cast<void>(DescribeVideoOutBuffer(request->buffer, request->group));
            request->width = request->group.attribute.width;
            request->height = request->group.attribute.height;
        } else {
            request->width = cfg->width;
            request->height = cfg->height;
        }
        request->generation = cfg->generation;
        request->flipRate = cfg->flipRate;
        ++queue->reservations;
        ++cfg->flipStatus.flipPendingNum;
        if (info.index >= 0) ++cfg->bufferPending[info.index];
        request->reserved = true;
        return request;
    }

    void Fail(std::exception_ptr error) noexcept override {
        if (!error) std::terminate();
        {
            std::lock_guard lock(queue->mutex);
            if (!queue->failure) queue->failure = error;
        }
        {
            std::lock_guard lock(cfg->mutex);
            if (!cfg->failure) cfg->failure = error;
            cfg->vblankCond.notify_all();
        }
        queue->changed.notify_all();
    }

private:
    std::shared_ptr<VideoOutConfig> cfg;
    std::shared_ptr<FlipQueue> queue;
};

}

FlipRequest::~FlipRequest() {
    if (!reserved) return;
    std::lock_guard lock(cfg->mutex);
    if (!terminal) {
        --cfg->flipStatus.flipPendingNum;
        --queue->reservations;
        if (index >= 0) --cfg->bufferPending[index];
        cfg->vblankCond.notify_all();
    }
}

void FlipRequest::GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>& frameTiming) {
    require(frameTiming != nullptr, "missing frame timing");
    timing = frameTiming;
    {
        AgcDriver::PerformanceContext timingContext(timing.get());
        AgcDriver::PerformanceTimer readiness("VideoOut.Readiness");
        std::lock_guard queueLock(queue->mutex);
        if (queue->failure) std::rethrow_exception(queue->failure);
        require(!queue->stopping, "GPU flip during shutdown");
        std::lock_guard lock(cfg->mutex);
        checkConfig(*cfg);
        require(reserved && !ready && !terminal && cfg->generation == generation, "invalid flip readiness transition");
        readiness.Mark("locks_validate");
        queuedAt = AgcDriver::FrameTiming::Clock::now();
        queue->requests.push_back(shared_from_this());
        ready = true;
    }
    queue->changed.notify_all();
    // The worker is done once the request is queued: hardware does not stall the command processor
    // on a flip, and the title observes completion through flipPendingNum, which processFlip drops
    // after presenting. A presentation failure reaches the worker through ReportFailure at its next
    // packet. Debug aid: APS5_SYNC_FLIP=1 parks the worker until the presenter is done, as before.
    static const bool syncFlip = std::getenv("APS5_SYNC_FLIP") != nullptr;
    if (!syncFlip) return;
    std::unique_lock lock(cfg->mutex);
    cfg->vblankCond.wait(lock, [&] { return gpuComplete || cfg->failure || cfg->closing; });
    checkConfig(*cfg);
    require(gpuComplete, "flip preparation did not complete on GPU");
}

void FlipRequest::Fail(std::exception_ptr error) noexcept {
    if (!error) std::terminate();
    std::lock_guard lock(cfg->mutex);
    if (!cfg->failure) cfg->failure = error;
    if (reserved && !terminal) {
        --cfg->flipStatus.flipPendingNum;
        --queue->reservations;
        if (index >= 0) --cfg->bufferPending[index];
        terminal = true;
    }
    cfg->vblankCond.notify_all();
    queue->changed.notify_all();
}

VideoOutDriver& VideoOutDriver::Get() {
    static VideoOutDriver instance;
    return instance;
}

VideoOutDriver::VideoOutDriver() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("SDL_InitSubSystem(VIDEO) failed: ") + SDL_GetError());
    }
    try {
        AgcDriverWaitIdle_nid_postfix();
        presentThread = std::jthread([this](std::stop_token token) { presentLoop(token); });
        vblankThread = std::jthread([this](std::stop_token token) { vblankLoop(token); });
        LibcRegisterShutdown_nid_postfix([] { VideoOutDriver::Get().Shutdown(); });
    } catch (...) {
        if (vblankThread.joinable()) {
            vblankThread.request_stop();
            flipQueue->changed.notify_all();
            vblankThread.join();
        }
        if (presentThread.joinable()) {
            presentThread.request_stop();
            flipQueue->changed.notify_all();
            presentThread.join();
        }
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        throw;
    }
}

VideoOutDriver::~VideoOutDriver() {
    if (!stopped) Shutdown();
}

void VideoOutDriver::Shutdown() {
    require(std::this_thread::get_id() != presentThread.get_id(), "presentation thread cannot stop itself");
    std::lock_guard shutdownLock(shutdownMutex);
    if (stopped) return;
    {
        std::lock_guard lock(mutex);
        {
            std::lock_guard queueLock(flipQueue->mutex);
            flipQueue->stopping = true;
        }
        for (int handle = 1; handle < VIDEO_OUT_NUM_MAX; ++handle) {
            if (outputs[handle]) close(handle);
        }
    }
    presentThread.request_stop();
    vblankThread.request_stop();
    flipQueue->changed.notify_all();
    presentThread.join();
    vblankThread.join();
    if (window.Handle() != nullptr) {
        AgcDriverReleaseWindow_nid_postfix(window.Handle());
        window.Destroy();
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    stopped = true;
    std::lock_guard lock(flipQueue->mutex);
    if (flipQueue->failure) std::rethrow_exception(flipQueue->failure);
}

int VideoOutDriver::Open(int busType) {
    std::lock_guard lock(mutex);
    {
        std::lock_guard queueLock(flipQueue->mutex);
        if (flipQueue->failure) std::rethrow_exception(flipQueue->failure);
        require(!flipQueue->stopping, "open during shutdown");
    }
    const int handle = busType + 1;
    require(handle > 0 && handle < VIDEO_OUT_NUM_MAX, "invalid output bus");
    auto previous = contexts[handle];
    uint64_t generation = 1;
    if (previous) {
        std::lock_guard cfgLock(previous->mutex);
        require(!previous->opened, "port already open");
        require(previous->generation != std::numeric_limits<uint64_t>::max(), "port generation overflow");
        generation = previous->generation + 1;
    }
    auto cfg = std::make_shared<VideoOutConfig>();
    cfg->generation = generation;
    cfg->opened = true;
    cfg->flipStatus.flipArg = -1;
    cfg->flipStatus.currentBuffer = -1;
    auto output = std::make_shared<VideoOutput>(cfg, flipQueue);
    AgcDriverRegisterVideoOutput_nid_postfix(static_cast<uint32_t>(handle), output);
    contexts[handle] = std::move(cfg);
    outputs[handle] = std::move(output);
    return handle;
}

bool VideoOutDriver::Close(int handle) {
    std::lock_guard lock(mutex);
    return close(handle);
}

bool VideoOutDriver::close(int handle) {
    require(handle > 0 && handle < VIDEO_OUT_NUM_MAX && outputs[handle] != nullptr, "invalid close handle");
    AgcDriverUnregisterVideoOutput_nid_postfix(static_cast<uint32_t>(handle), outputs[handle]);
    outputs[handle].reset();
    auto cfg = contexts[handle];
    std::lock_guard cfgLock(cfg->mutex);
    cfg->opened = false;
    cfg->closing = true;
    const auto removeEvents = [](const auto& events, int kind) {
        for (const auto& event : events) {
            const auto result = EqueueDeleteEvent_nid_postfix(event.eq, static_cast<uintptr_t>(kind), EVFILT_VIDEO_OUT);
            require(result == EQUEUE_OK || result == EQUEUE_ERROR_EBADF || result == EQUEUE_ERROR_ENOENT, "event removal during close failed");
        }
    };
    removeEvents(cfg->flipEvents, VIDEO_OUT_EVENT_FLIP);
    removeEvents(cfg->vblankEvents, VIDEO_OUT_EVENT_VBLANK);
    removeEvents(cfg->preVblankEvents, VIDEO_OUT_EVENT_PRE_VBLANK_START);
    removeEvents(cfg->outputModeEvents, VIDEO_OUT_EVENT_SET_MODE);
    cfg->flipEvents.clear();
    cfg->vblankEvents.clear();
    cfg->preVblankEvents.clear();
    cfg->outputModeEvents.clear();
    cfg->vblankCond.notify_all();
    flipQueue->changed.notify_all();
    return true;
}

std::shared_ptr<VideoOutConfig> VideoOutDriver::GetConfig(int handle) {
    {
        std::lock_guard lock(flipQueue->mutex);
        if (flipQueue->failure) std::rethrow_exception(flipQueue->failure);
        require(!flipQueue->stopping, "output is shutting down");
    }
    std::lock_guard lock(mutex);
    require(handle > 0 && handle < VIDEO_OUT_NUM_MAX && contexts[handle] != nullptr, "invalid output handle");
    auto cfg = contexts[handle];
    std::lock_guard cfgLock(cfg->mutex);
    checkConfig(*cfg);
    return cfg;
}

bool VideoOutDriver::IsOpen(int handle) {
    return GetConfig(handle) != nullptr;
}

int VideoOutDriver::SubmitFlip(int handle, int index, int flipMode, int64_t flipArg) {
    // A title that does not pace on flipPendingNum can run ahead of the presenter now that the queue
    // worker no longer waits per flip; a full queue is the documented error, not a failure.
    if (flipQueue->reservations.load() >= VIDEO_OUT_FLIP_QUEUE_CAPACITY) return VIDEO_OUT_ERROR_FLIP_QUEUE_FULL;
    std::array<uint32_t, AgcDriver::FlipPacketWords> words{AgcDriver::FlipPacketHeader, static_cast<uint32_t>(handle), static_cast<uint32_t>(index), static_cast<uint32_t>(flipMode), static_cast<uint32_t>(static_cast<uint64_t>(flipArg)), static_cast<uint32_t>(static_cast<uint64_t>(flipArg) >> 32u)};
    Packet packet{words.data(), static_cast<uint32_t>(words.size()), 0, {}};
    const auto result = sceAgcDriverSubmitDcb(&packet);
    require(result == 0, "driver rejected flip submission");
    return 0;
}

void VideoOutDriver::triggerEvents(VideoOutConfig& cfg, int eventKind, void* triggerData) {
    std::vector<EventRegistration>* events = nullptr;
    if (eventKind == VIDEO_OUT_EVENT_FLIP) events = &cfg.flipEvents;
    else if (eventKind == VIDEO_OUT_EVENT_VBLANK) events = &cfg.vblankEvents;
    else if (eventKind == VIDEO_OUT_EVENT_PRE_VBLANK_START) events = &cfg.preVblankEvents;
    else if (eventKind == VIDEO_OUT_EVENT_SET_MODE) events = &cfg.outputModeEvents;
    else throw std::runtime_error("VideoOut: unknown event kind");
    for (auto it = events->begin(); it != events->end();) {
        require(it->generation == cfg.generation, "stale event registration");
        const auto result = EqueueTriggerEvent_nid_postfix(it->eq, static_cast<uintptr_t>(eventKind), EVFILT_VIDEO_OUT, triggerData);
        if (result == EQUEUE_ERROR_EBADF || result == EQUEUE_ERROR_ENOENT) it = events->erase(it);
        else {
            require(result == EQUEUE_OK, "event delivery failed");
            ++it;
        }
    }
}

void VideoOutDriver::vblankEnd() {
    std::lock_guard lock(mutex);
    for (const auto& cfg : contexts) {
        if (!cfg) continue;
        std::lock_guard cfgLock(cfg->mutex);
        if (!cfg->opened || cfg->failure) continue;
        require(cfg->vblankStatus.count != std::numeric_limits<uint64_t>::max(), "vblank counter overflow");
        ++cfg->vblankStatus.count;
        cfg->vblankStatus.processTime = sceKernelGetProcessTime();
        cfg->vblankStatus.processTimeCounter = sceKernelGetProcessTimeCounter();
        triggerEvents(*cfg, VIDEO_OUT_EVENT_VBLANK, reinterpret_cast<void*>(cfg->vblankStatus.count));
        cfg->vblankCond.notify_all();
    }
}

void VideoOutDriver::processFlip(FlipRequest& req) {
    AgcDriver::PerformanceContext timingContext(req.timing.get());
    AgcDriver::PerformanceTimer timing("VideoOut.Flip");
    {
        std::unique_lock lock(req.cfg->mutex);
        timing.Mark("config_mutex_wait");
        checkConfig(*req.cfg);
        require(req.ready && !req.terminal && req.generation == req.cfg->generation, "stale or incomplete flip request");
        const auto interval = static_cast<uint64_t>(req.flipRate + 1);
        require(req.cfg->lastFlipVblank <= std::numeric_limits<uint64_t>::max() - interval, "flip interval overflow");
        const auto target = req.cfg->lastFlipVblank + interval;
        timing.Mark("validate");
        req.cfg->vblankCond.wait(lock, [&] { return req.cfg->vblankStatus.count >= target || req.cfg->failure || req.cfg->closing; });
        timing.Mark("vblank_wait");
        checkConfig(*req.cfg);
    }
    require(req.width != 0 && req.height != 0 && req.width <= static_cast<uint32_t>(std::numeric_limits<int>::max()) && req.height <= static_cast<uint32_t>(std::numeric_limits<int>::max()), "invalid window dimensions");
    window.Ensure(req.width, req.height);
    unsigned extensionCount = 0;
    if (!SDL_Vulkan_GetInstanceExtensions(window.Handle(), &extensionCount, nullptr)) throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    std::vector<const char*> extensions(extensionCount);
    if (!SDL_Vulkan_GetInstanceExtensions(window.Handle(), &extensionCount, extensions.data())) throw std::runtime_error(std::string("SDL_Vulkan_GetInstanceExtensions failed: ") + SDL_GetError());
    extensions.resize(extensionCount);
    const AgcDriver::PresentationWindow target{window.Handle(), extensions, [](void* context, VkInstance instance) {
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        if (!SDL_Vulkan_CreateSurface(static_cast<SDL_Window*>(context), instance, &surface)) throw std::runtime_error(std::string("SDL_Vulkan_CreateSurface failed: ") + SDL_GetError());
        return surface;
    }, [](void* context, std::uint32_t* width, std::uint32_t* height) {
        if ((SDL_GetWindowFlags(static_cast<SDL_Window*>(context)) & SDL_WINDOW_MINIMIZED) != 0) {
            *width = 0;
            *height = 0;
            return;
        }
        int drawableWidth = 0;
        int drawableHeight = 0;
        SDL_Vulkan_GetDrawableSize(static_cast<SDL_Window*>(context), &drawableWidth, &drawableHeight);
        *width = drawableWidth > 0 ? static_cast<std::uint32_t>(drawableWidth) : 0;
        *height = drawableHeight > 0 ? static_cast<std::uint32_t>(drawableHeight) : 0;
    }, req.width, req.height, req.timing};
    timing.Mark("window_prepare");
    const auto gpuReady = [](void* context) {
        auto& request = *static_cast<FlipRequest*>(context);
        std::lock_guard lock(request.cfg->mutex);
        checkConfig(*request.cfg);
        require(!request.terminal && !request.gpuComplete, "invalid GPU completion transition");
        request.gpuComplete = true;
        request.cfg->vblankCond.notify_all();
    };
    if (req.index >= 0) {
        const auto display = DescribeVideoOutBuffer(req.buffer, req.group);
        AgcDriverPresentBuffer_nid_postfix(target, display, gpuReady, &req);
    } else {
        AgcDriverPresentClear_nid_postfix(target, req.index == VIDEO_OUT_BUFFER_INDEX_BLACK, gpuReady, &req);
    }
    timing.Mark("present");
    window.UpdateTitle();
    timing.Mark("window_title");
    std::lock_guard lock(req.cfg->mutex);
    timing.Mark("completion_mutex_wait");
    checkConfig(*req.cfg);
    require(!req.terminal && req.cfg->generation == req.generation, "flip cancelled during presentation");
    require(req.gpuComplete, "flip submitted before GPU completion");
    require(req.cfg->flipStatus.count != std::numeric_limits<uint64_t>::max(), "flip counter overflow");
    triggerEvents(*req.cfg, VIDEO_OUT_EVENT_FLIP, reinterpret_cast<void*>(req.flipArg));
    ++req.cfg->flipStatus.count;
    req.cfg->lastFlipVblank = req.cfg->vblankStatus.count;
    req.cfg->flipStatus.processTime = sceKernelGetProcessTime();
    req.cfg->flipStatus.processTimeCounter = sceKernelGetProcessTimeCounter();
    req.cfg->flipStatus.flipArg = req.flipArg;
    req.cfg->flipStatus.currentBuffer = req.index;
    req.cfg->width = req.width;
    req.cfg->height = req.height;
    --req.cfg->flipStatus.flipPendingNum;
    --req.queue->reservations;
    if (req.index >= 0) --req.cfg->bufferPending[req.index];
    req.terminal = true;
    req.cfg->vblankCond.notify_all();
    timing.Mark("notify_game");
}

void VideoOutDriver::presentLoop(std::stop_token token) {
    std::shared_ptr<FlipRequest> current;
    PadInput padInput;
    try {
        while (!token.stop_requested()) {
            {
                std::unique_lock lock(flipQueue->mutex);
                flipQueue->changed.wait_for(lock, std::chrono::milliseconds(10), [&] { return token.stop_requested() || flipQueue->failure || !flipQueue->requests.empty(); });
                if (flipQueue->failure) std::rethrow_exception(flipQueue->failure);
                if (token.stop_requested()) break;
                if (!flipQueue->requests.empty()) {
                    current = std::move(flipQueue->requests.front());
                    flipQueue->requests.pop_front();
                }
            }
            SDL_Event event;
            while (SDL_PollEvent(&event)) {
                require(event.type != SDL_QUIT, "window was closed");
                padInput.HandleEvent(event, window);
            }
            padInput.Update();
            if (current) {
                require(current->timing != nullptr, "missing presentation timing");
                const auto dequeued = AgcDriver::FrameTiming::Clock::now();
                current->timing->Add(current->timing->Get("VideoOut", "queue"), dequeued - current->queuedAt);
                processFlip(*current);
                const auto finished = AgcDriver::FrameTiming::Clock::now();
                AgcDriver::FrameTiming::Clock::duration interval{};
                {
                    std::lock_guard lock(current->cfg->mutex);
                    const auto previous = current->cfg->lastTimingFlip;
                    if (previous != AgcDriver::FrameTiming::Clock::time_point{}) interval = finished - previous;
                    current->cfg->lastTimingFlip = finished;
                }
                current->timing->Print(current->outputHandle, current->index, current->flipArg, finished, interval);
            }
            current.reset();
        }
        std::list<std::shared_ptr<FlipRequest>> cancelled;
        {
            std::lock_guard lock(flipQueue->mutex);
            cancelled.swap(flipQueue->requests);
        }
        if (!cancelled.empty()) {
            auto error = std::make_exception_ptr(std::runtime_error("VideoOut: pending flip cancelled during shutdown"));
            for (auto& request : cancelled) request->Fail(error);
        }
    } catch (...) {
        auto error = std::current_exception();
        if (!error) std::terminate();
        // The driver learns first: the worker no longer waits for the presenter, so a title (or test)
        // that sees the failed flip's counters must already find the failure in the driver.
        AgcDriverReportFailure_nid_postfix(error);
        PadReportInputFailure_nid_postfix(error);
        if (current) current->Fail(error);
        std::list<std::shared_ptr<FlipRequest>> failed;
        {
            std::lock_guard lock(flipQueue->mutex);
            flipQueue->failure = error;
            failed.swap(flipQueue->requests);
        }
        for (auto& request : failed) request->Fail(error);
        {
            std::lock_guard lock(mutex);
            for (auto& cfg : contexts) {
                if (!cfg) continue;
                std::lock_guard cfgLock(cfg->mutex);
                if (!cfg->failure) cfg->failure = error;
                cfg->vblankCond.notify_all();
            }
        }
        flipQueue->changed.notify_all();
    }
}

void VideoOutDriver::vblankLoop(std::stop_token token) {
    using Frame = std::chrono::duration<int64_t, std::ratio<1001, 60000>>;
    const auto start = std::chrono::steady_clock::now();
    try {
        for (int64_t frame = 1; !token.stop_requested(); ++frame) {
            const auto next = start + std::chrono::duration_cast<std::chrono::steady_clock::duration>(Frame(frame));
            {
                std::unique_lock lock(flipQueue->mutex);
                flipQueue->changed.wait_until(lock, next, [&] { return token.stop_requested() || flipQueue->failure; });
                if (token.stop_requested() || flipQueue->failure) return;
            }
            vblankEnd();
        }
    } catch (...) {
        AgcDriverReportFailure_nid_postfix(std::current_exception());
    }
}
