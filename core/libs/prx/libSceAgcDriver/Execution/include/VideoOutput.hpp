#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VIDEOOUTPUT_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VIDEOOUTPUT_HPP

#include <cstdint>
#include <exception>
#include <memory>
#include <stdexcept>

namespace AgcDriver {

class FrameTiming;

inline constexpr std::uint32_t FlipPacketHeader = 0xc004105cu;
inline constexpr std::uint32_t FlipPacketWords = 6;
// AnyPS5's internal encoding for the driver-generated rendering wait.
inline constexpr std::uint32_t RenderingWaitPacketHeader = 0xc0021018u;
inline constexpr std::uint32_t RenderingWaitPacketWords = 4;

class IRenderingWait {
public:
    virtual ~IRenderingWait() = default;
    virtual void Wait() = 0;
};

struct FlipInfo {
    std::uint32_t handle;
    std::int32_t index;
    std::uint32_t mode;
    std::int64_t argument;
};

class IFlipRequest {
public:
    virtual ~IFlipRequest() = default;
    virtual void GpuReady(const std::shared_ptr<FrameTiming>& timing) = 0;
    virtual void Fail(std::exception_ptr error) noexcept = 0;
};

class IVideoOutput {
public:
    virtual ~IVideoOutput() = default;
    virtual std::shared_ptr<IFlipRequest> Reserve(const FlipInfo& info) = 0;
    // Blocks a submission carrying a flip until the output's flip queue has room, as the system
    // holds back a submitter that runs ahead of the display. Called without the driver's lock.
    virtual void WaitForFlipRoom() {}
    virtual std::shared_ptr<IRenderingWait> CaptureRenderingWait(std::uint32_t index) {
        throw std::runtime_error("VideoOut: rendering waits are unsupported by this output");
    }
    virtual void Fail(std::exception_ptr error) noexcept = 0;
};

}

extern "C" void AgcDriverRegisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output);
extern "C" void AgcDriverUnregisterVideoOutput_nid_postfix(std::uint32_t handle, const std::shared_ptr<AgcDriver::IVideoOutput>& output);

#endif
