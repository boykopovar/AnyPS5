#ifndef CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_VIDEOOUTDRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOUOUT_INCLUDE_VIDEOOUTDRIVER_HPP

#include <array>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <thread>
#include <vector>

#include "SDL.h"
#include "SceTypes.hpp"

static constexpr int VIDEO_OUT_ERROR_INVALID_VALUE = -2144796671;
static constexpr int VIDEO_OUT_ERROR_INVALID_ADDRESS = -2144796670;
static constexpr int VIDEO_OUT_ERROR_INVALID_HANDLE = -2144796669;
static constexpr int VIDEO_OUT_ERROR_INVALID_EVENT_QUEUE = -2144796668;
static constexpr int VIDEO_OUT_ERROR_INVALID_INDEX = -2144796667;
static constexpr int VIDEO_OUT_ERROR_INVALID_OPTION = -2144796666;
static constexpr int VIDEO_OUT_ERROR_INVALID_CATEGORY = -2144796664;
static constexpr int VIDEO_OUT_ERROR_SLOT_OCCUPIED = -2144796663;
static constexpr int VIDEO_OUT_ERROR_RESOURCE_BUSY = -2144796656;
static constexpr int VIDEO_OUT_ERROR_FLIP_QUEUE_FULL = -2144796654;
static constexpr int VIDEO_OUT_ERROR_UNSUPPORTED_OUTPUT_MODE = -2144796634;
static constexpr int VIDEO_OUT_ERROR_UNAVAILABLE_OUTPUT_MODE = -2144796633;
static constexpr int VIDEO_OUT_ERROR_INVALID_EVENT = -2144796624;

static constexpr int VIDEO_OUT_BUS_TYPE_MAIN = 0;
static constexpr int VIDEO_OUT_BUS_TYPE_OVERLAY = 1;
static constexpr int VIDEO_OUT_BUS_TYPE_SUB = 2;

static constexpr int VIDEO_OUT_BUFFER_NUM_MAX = 16;
static constexpr int VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX = 4;
static constexpr int VIDEO_OUT_NUM_MAX = 4;
static constexpr std::size_t VIDEO_OUT_FLIP_QUEUE_CAPACITY = 16;

static constexpr int VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED = 0;
static constexpr int VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_COMPRESSED = 1;

static constexpr int VIDEO_OUT_EVENT_FLIP = 0;
static constexpr int VIDEO_OUT_EVENT_VBLANK = 1;
static constexpr int VIDEO_OUT_EVENT_PRE_VBLANK_START = 2;
static constexpr int VIDEO_OUT_EVENT_SET_MODE = 8;

static constexpr int VIDEO_OUT_FLIP_MODE_VSYNC = 1;
static constexpr int VIDEO_OUT_FLIP_MODE_VSYNC_MULTI = 4;

static constexpr int VIDEO_OUT_BUFFER_INDEX_BLACK = -2;
static constexpr int VIDEO_OUT_BUFFER_INDEX_BLANK = -1;

static constexpr uint64_t VIDEO_OUT_OUTPUT_MODE_DEFAULT = 0x0000000000000001ULL;
static constexpr uint64_t VIDEO_OUT_OUTPUT_MODE_119_88HZ = 0x000000000000000FULL;

static constexpr uint64_t VIDEO_OUT_REFRESH_RATE_59_94HZ = 3;
static constexpr uint64_t VIDEO_OUT_REFRESH_RATE_119_88HZ = 13;

struct VideoOutBuffer {
    int groupIndex = -1;
    uint64_t dataAddress = 0;
    uint64_t metadataAddress = 0;

    bool Occupied() const { return groupIndex >= 0; }
};

struct BufferAttributeGroup {
    VideoOutBufferAttribute2 attribute{};
    int category = VIDEO_OUT_BUFFER_ATTRIBUTE_CATEGORY_UNCOMPRESSED;
    bool occupied = false;
};

struct EventRegistration {
    KernelEqueue eq = 0;
    uint64_t generation = 0;
};

struct VideoOutConfig {
    std::mutex mutex;
    std::condition_variable_any vblankCond;

    std::vector<EventRegistration> flipEvents;
    std::vector<EventRegistration> vblankEvents;
    std::vector<EventRegistration> preVblankEvents;
    std::vector<EventRegistration> outputModeEvents;

    uint32_t width = 1920;
    uint32_t height = 1080;
    uint64_t generation = 0;
    bool opened = false;
    bool closing = false;
    int flipRate = 0;
    uint64_t outputMode = VIDEO_OUT_OUTPUT_MODE_DEFAULT;
    float gamma = 1.0f;

    VideoOutFlipStatus flipStatus{};
    VideoOutVblankStatus vblankStatus{};
    VideoOutVblankStatus preVblankStatus{};

    std::array<VideoOutBuffer, VIDEO_OUT_BUFFER_NUM_MAX> buffers{};
    std::array<BufferAttributeGroup, VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX> groups{};
};

struct FlipRequest {
    VideoOutConfig* cfg = nullptr;
    uint64_t generation = 0;
    int index = 0;
    int flipMode = 0;
    int64_t flipArg = 0;
};

class VideoOutDriver {
public:
    static VideoOutDriver& Get();

    VideoOutDriver();
    ~VideoOutDriver();

    VideoOutDriver(const VideoOutDriver&) = delete;
    VideoOutDriver& operator=(const VideoOutDriver&) = delete;

    int Open(int busType);
    bool Close(int handle);
    VideoOutConfig* GetConfig(int handle);
    bool IsOpen(int handle);

    void SubmitFlip(VideoOutConfig* cfg, int index, int flipMode, int64_t flipArg);

private:
    void presentLoop(std::stop_token token);
    void vblankBegin();
    void vblankEnd();
    void processFlip(const FlipRequest& req);
    void triggerEvents(VideoOutConfig& cfg, int eventKind, void* triggerData);

    std::mutex mutex;
    VideoOutConfig contexts[VIDEO_OUT_NUM_MAX];

    std::mutex flipMutex;
    std::condition_variable flipCond;
    std::list<FlipRequest> flipQueue;

    SDL_Window* window = nullptr;
    SDL_Surface* windowSurface = nullptr;

    std::jthread presentThread;
};

#endif
