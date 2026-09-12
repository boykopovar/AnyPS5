#include <chrono>
#include <stdexcept>

#include "SDL.h"
#include "prx/libkernel/Equeue/Equeue.hpp"
#include "prx/libkernel/Time/include/Time.hpp"
#include "prx/libSceVideoOut/include/VideoOutDriver.hpp"

static constexpr uint32_t VBLANK_FREQ_DEFAULT = 60;
static constexpr uint32_t VBLANK_FREQ_119 = 120;

VideoOutDriver& VideoOutDriver::Get() {
    static VideoOutDriver instance;
    return instance;
}

VideoOutDriver::VideoOutDriver() {
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0) {
        throw std::runtime_error(std::string("SDL_InitSubSystem(VIDEO) failed: ") + SDL_GetError());
    }
    presentThread = std::jthread([this](std::stop_token token) { presentLoop(token); });
}

VideoOutDriver::~VideoOutDriver() {
    if (presentThread.joinable()) {
        presentThread.request_stop();
        flipCond.notify_all();
        presentThread.join();
    }
    if (window != nullptr) {
        SDL_DestroyWindow(window);
        window = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
}

int VideoOutDriver::Open(int busType) {
    std::unique_lock lock(mutex);
    const int handle = busType + 1;
    if (handle <= 0 || handle >= VIDEO_OUT_NUM_MAX) {
        return -1;
    }
    auto& cfg = contexts[handle];
    std::unique_lock cfgLock(cfg.mutex);
    if (cfg.opened) {
        return -1;
    }
    cfg.closing = false;
    cfg.opened = true;
    cfg.flipRate = 0;
    cfg.outputMode = VIDEO_OUT_OUTPUT_MODE_DEFAULT;
    cfg.gamma = 1.0f;
    cfg.flipStatus = VideoOutFlipStatus{};
    cfg.flipStatus.flipArg = -1;
    cfg.flipStatus.currentBuffer = -1;
    cfg.vblankStatus = VideoOutVblankStatus{};
    cfg.preVblankStatus = VideoOutVblankStatus{};
    for (auto& buf : cfg.buffers) {
        buf = VideoOutBuffer{};
    }
    for (auto& grp : cfg.groups) {
        grp = BufferAttributeGroup{};
    }
    cfg.flipEvents.clear();
    cfg.vblankEvents.clear();
    cfg.preVblankEvents.clear();
    cfg.outputModeEvents.clear();
    if (++cfg.generation == 0) {
        throw std::runtime_error("VideoOut port generation wrapped");
    }
    return handle;
}

bool VideoOutDriver::Close(int handle) {
    std::unique_lock lock(mutex);
    if (handle <= 0 || handle >= VIDEO_OUT_NUM_MAX) {
        return false;
    }
    auto& cfg = contexts[handle];
    std::unique_lock cfgLock(cfg.mutex);
    if (!cfg.opened || cfg.closing) {
        return false;
    }
    cfg.opened = false;
    cfg.closing = true;
    if (++cfg.generation == 0) {
        throw std::runtime_error("VideoOut port generation wrapped");
    }
    for (auto& buf : cfg.buffers) {
        buf = VideoOutBuffer{};
    }
    for (auto& grp : cfg.groups) {
        grp = BufferAttributeGroup{};
    }
    cfg.flipRate = 0;
    cfg.flipEvents.clear();
    cfg.vblankEvents.clear();
    cfg.preVblankEvents.clear();
    cfg.outputModeEvents.clear();
    cfg.vblankCond.notify_all();
    return true;
}

VideoOutConfig* VideoOutDriver::GetConfig(int handle) {
    std::unique_lock lock(mutex);
    if (handle <= 0 || handle >= VIDEO_OUT_NUM_MAX || !contexts[handle].opened) {
        return nullptr;
    }
    return &contexts[handle];
}

bool VideoOutDriver::IsOpen(int handle) {
    std::unique_lock lock(mutex);
    return handle > 0 && handle < VIDEO_OUT_NUM_MAX && contexts[handle].opened;
}

void VideoOutDriver::SubmitFlip(VideoOutConfig* cfg, int index, int flipMode, int64_t flipArg) {
    std::unique_lock lock(flipMutex);
    if (flipQueue.size() >= VIDEO_OUT_FLIP_QUEUE_CAPACITY) {
        throw std::runtime_error("VideoOut flip queue full");
    }
    FlipRequest req;
    req.cfg = cfg;
    req.generation = cfg->generation;
    req.index = index;
    req.flipMode = flipMode;
    req.flipArg = flipArg;
    {
        std::unique_lock cfgLock(cfg->mutex);
        cfg->flipStatus.flipPendingNum++;
    }
    flipQueue.push_back(req);
    flipCond.notify_one();
}

void VideoOutDriver::triggerEvents(VideoOutConfig& cfg, int eventKind, void* triggerData) {
    std::vector<EventRegistration>* events = nullptr;
    if (eventKind == VIDEO_OUT_EVENT_FLIP) {
        events = &cfg.flipEvents;
    } else if (eventKind == VIDEO_OUT_EVENT_VBLANK) {
        events = &cfg.vblankEvents;
    } else if (eventKind == VIDEO_OUT_EVENT_PRE_VBLANK_START) {
        events = &cfg.preVblankEvents;
    } else if (eventKind == VIDEO_OUT_EVENT_SET_MODE) {
        events = &cfg.outputModeEvents;
    } else {
        throw std::runtime_error("triggerEvents: unknown event kind");
    }
    for (auto& reg : *events) {
        if (reg.generation != cfg.generation) {
            continue;
        }
        EqueueTriggerEvent_nid_postfix(reg.eq, static_cast<uintptr_t>(eventKind), EVFILT_VIDEO_OUT, triggerData);
    }
}

void VideoOutDriver::vblankBegin() {
    std::unique_lock lock(mutex);
    for (int i = 1; i < VIDEO_OUT_NUM_MAX; i++) {
        auto& cfg = contexts[i];
        if (!cfg.opened) {
            continue;
        }
        std::unique_lock cfgLock(cfg.mutex);
        cfg.preVblankStatus.count++;
        cfg.preVblankStatus.processTime = sceKernelGetProcessTime();
        cfg.preVblankStatus.processTimeCounter = sceKernelGetProcessTimeCounter();
        triggerEvents(cfg, VIDEO_OUT_EVENT_PRE_VBLANK_START, reinterpret_cast<void*>(cfg.preVblankStatus.count));
    }
}

void VideoOutDriver::vblankEnd() {
    std::unique_lock lock(mutex);
    for (int i = 1; i < VIDEO_OUT_NUM_MAX; i++) {
        auto& cfg = contexts[i];
        if (!cfg.opened) {
            continue;
        }
        std::unique_lock cfgLock(cfg.mutex);
        cfg.vblankStatus.count++;
        cfg.vblankStatus.processTime = sceKernelGetProcessTime();
        cfg.vblankStatus.processTimeCounter = sceKernelGetProcessTimeCounter();
        triggerEvents(cfg, VIDEO_OUT_EVENT_VBLANK, reinterpret_cast<void*>(cfg.vblankStatus.count));
        cfg.vblankCond.notify_all();
    }
}

void VideoOutDriver::processFlip(const FlipRequest& req) {
    VideoOutConfig* cfg = req.cfg;
    {
        std::unique_lock cfgLock(cfg->mutex);
        if (!cfg->opened || cfg->closing || cfg->generation != req.generation) {
            cfg->flipStatus.flipPendingNum--;
            return;
        }
    }
    const bool isBlank = req.index == VIDEO_OUT_BUFFER_INDEX_BLANK;
    const bool isBlack = req.index == VIDEO_OUT_BUFFER_INDEX_BLACK;
    if (isBlank || isBlack) {
        if (window != nullptr) {
            SDL_FillRect(windowSurface, nullptr, isBlack ? SDL_MapRGB(windowSurface->format, 0, 0, 0) : 0);
            SDL_UpdateWindowSurface(window);
        }
    } else {
        throw std::runtime_error("VideoOut Vulkan rendering not implemented");
    }
    std::unique_lock cfgLock(cfg->mutex);
    if (!cfg->opened || cfg->closing || cfg->generation != req.generation) {
        cfg->flipStatus.flipPendingNum--;
        return;
    }
    cfg->flipStatus.count++;
    cfg->flipStatus.processTime = sceKernelGetProcessTime();
    cfg->flipStatus.processTimeCounter = sceKernelGetProcessTimeCounter();
    cfg->flipStatus.flipArg = req.flipArg;
    cfg->flipStatus.currentBuffer = req.index;
    cfg->flipStatus.flipPendingNum--;
    triggerEvents(*cfg, VIDEO_OUT_EVENT_FLIP, reinterpret_cast<void*>(req.flipArg));
}

void VideoOutDriver::presentLoop(std::stop_token token) {
    while (!token.stop_requested()) {
        uint64_t outputMode = VIDEO_OUT_OUTPUT_MODE_DEFAULT;
        {
            std::unique_lock lock(mutex);
            for (int i = 1; i < VIDEO_OUT_NUM_MAX; i++) {
                if (contexts[i].opened) {
                    outputMode = contexts[i].outputMode;
                    break;
                }
            }
        }
        const uint32_t freq = (outputMode == VIDEO_OUT_OUTPUT_MODE_119_88HZ) ? VBLANK_FREQ_119 : VBLANK_FREQ_DEFAULT;
        const auto period = std::chrono::nanoseconds(1'000'000'000ULL / freq);
        const auto frameStart = std::chrono::steady_clock::now();

        vblankBegin();

        FlipRequest req{};
        bool hasFlip = false;
        {
            std::unique_lock lock(flipMutex);
            if (!flipQueue.empty()) {
                req = flipQueue.front();
                flipQueue.pop_front();
                hasFlip = true;
            }
        }
        if (hasFlip) {
            if (window == nullptr) {
                uint32_t w = 1920;
                uint32_t h = 1080;
                {
                    std::unique_lock lock(mutex);
                    if (req.cfg != nullptr) {
                        std::unique_lock cfgLock(req.cfg->mutex);
                        w = req.cfg->width;
                        h = req.cfg->height;
                    }
                }
                window = SDL_CreateWindow("PS5", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, static_cast<int>(w), static_cast<int>(h), SDL_WINDOW_SHOWN);
                if (window == nullptr) {
                    throw std::runtime_error(std::string("SDL_CreateWindow failed: ") + SDL_GetError());
                }
                windowSurface = SDL_GetWindowSurface(window);
                if (windowSurface == nullptr) {
                    throw std::runtime_error(std::string("SDL_GetWindowSurface failed: ") + SDL_GetError());
                }
            }
            processFlip(req);
        }

        vblankEnd();

        SDL_Event sdlEvent;
        while (SDL_PollEvent(&sdlEvent)) {
            if (sdlEvent.type == SDL_QUIT) {
                return;
            }
        }

        const auto elapsed = std::chrono::steady_clock::now() - frameStart;
        if (elapsed < period) {
            const auto sleepUs = std::chrono::duration_cast<std::chrono::microseconds>(period - elapsed).count();
            if (sleepUs > 0) {
                sceKernelUsleep_nid_postfix(static_cast<KernelUseconds>(sleepUs));
            }
        }
    }
}
