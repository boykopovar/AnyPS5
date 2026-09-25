#ifndef CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_VIDEOOUTDRIVER_HPP
#define CORE_LIBS_PRX_LIBSCEVIDEOOUT_INCLUDE_VIDEOOUTDRIVER_HPP

#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <list>
#include <mutex>
#include <thread>
#include <vector>
#include <atomic>
#include <exception>
#include <memory>
#include <stdexcept>
#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceVideoOut/include/DisplayWindow.hpp"

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

AgcDriver::DisplayBuffer DescribeVideoOutBuffer(const VideoOutBuffer& buffer, const BufferAttributeGroup& group);

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
    std::exception_ptr failure;
    int flipRate = 0;
    uint64_t lastFlipVblank = 0;
    std::chrono::steady_clock::time_point lastTimingFlip{};
    uint64_t outputMode = VIDEO_OUT_OUTPUT_MODE_DEFAULT;
    float gamma = 1.0f;

    VideoOutFlipStatus flipStatus{};
    VideoOutVblankStatus vblankStatus{};
    VideoOutVblankStatus preVblankStatus{};

    std::array<VideoOutBuffer, VIDEO_OUT_BUFFER_NUM_MAX> buffers{};
    std::array<uint32_t, VIDEO_OUT_BUFFER_NUM_MAX> bufferPending{};
    std::array<BufferAttributeGroup, VIDEO_OUT_BUFFER_ATTRIBUTE_NUM_MAX> groups{};

    void Check() const {
        if (failure) std::rethrow_exception(failure);
        if (!opened || closing) throw std::runtime_error("VideoOut: port is closed");
    }
};

struct FlipQueue;

struct FlipRequest final : AgcDriver::IFlipRequest, std::enable_shared_from_this<FlipRequest> {
    std::shared_ptr<VideoOutConfig> cfg;
    std::shared_ptr<FlipQueue> queue;
    uint64_t generation = 0;
    std::uint32_t outputHandle = 0;
    int index = 0;
    int flipMode = 0;
    int flipRate = 0;
    int64_t flipArg = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    VideoOutBuffer buffer;
    BufferAttributeGroup group;
    bool reserved = false;
    bool ready = false;
    bool gpuComplete = false;
    bool terminal = false;

    std::shared_ptr<AgcDriver::FrameTiming> timing;
    std::chrono::steady_clock::time_point queuedAt;

    ~FlipRequest() override;
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>& frameTiming) override;
    void Fail(std::exception_ptr error) noexcept override;
};

struct FlipQueue {
    std::mutex mutex;
    std::condition_variable changed;
    std::list<std::shared_ptr<FlipRequest>> requests;
    std::atomic<std::size_t> reservations = 0;
    std::exception_ptr failure;
    bool stopping = false;
};

class VideoOutDriver {
public:
    static VideoOutDriver& Get();

    VideoOutDriver();
    ~VideoOutDriver();
    void Shutdown();

    VideoOutDriver(const VideoOutDriver&) = delete;
    VideoOutDriver& operator=(const VideoOutDriver&) = delete;

    int Open(int busType);
    bool Close(int handle);
    std::shared_ptr<VideoOutConfig> GetConfig(int handle);
    bool IsOpen(int handle);

    // 0, or VIDEO_OUT_ERROR_FLIP_QUEUE_FULL when the title has VIDEO_OUT_FLIP_QUEUE_CAPACITY flips pending.
    int SubmitFlip(int handle, int index, int flipMode, int64_t flipArg);

private:
    bool close(int handle);
    void presentLoop(std::stop_token token);
    void vblankLoop(std::stop_token token);
    void vblankEnd();
    void processFlip(FlipRequest& req);
    void triggerEvents(VideoOutConfig& cfg, int eventKind, void* triggerData);

    std::mutex mutex;
    std::mutex shutdownMutex;
    bool stopped = false;
    std::array<std::shared_ptr<VideoOutConfig>, VIDEO_OUT_NUM_MAX> contexts;
    std::array<std::shared_ptr<AgcDriver::IVideoOutput>, VIDEO_OUT_NUM_MAX> outputs;
    std::shared_ptr<FlipQueue> flipQueue = std::make_shared<FlipQueue>();

    DisplayWindow window;

    std::jthread presentThread;
    std::jthread vblankThread;
};

#endif
