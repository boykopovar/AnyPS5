#include "prx/libSceAgcDriver/Execution/include/VideoOutput.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libSceVideoOut/include/BufferReuseTracker.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include <array>
#include <chrono>
#include <condition_variable>
#include <cstdlib>
#include <mutex>
extern "C" std::uint32_t APS5_VABI sceAgcDriverGetWaitRenderingPacketSizeInDwords();
extern "C" std::uint32_t APS5_VABI sceAgcDriverGetSetFlipPacketSizeInDwords();
extern "C" std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadsActivePacketSize();
extern "C" std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadCompletePacketSize();
extern "C" std::uint32_t APS5_VABI sceAgcDriverGetSetWorkloadStreamInactivePacketSize();
extern "C" std::uint32_t APS5_VABI sceAgcDriverNopDataGetPacketSize(std::uint32_t);
extern "C" std::uint32_t APS5_VABI sceAgcDriverUserDataGetPacketSize(std::uint32_t);
extern "C" std::uint32_t APS5_VABI sceAgcDriverWaitUntilSafeForRendering(std::uint32_t**, std::uint32_t, std::uint32_t, std::uint32_t, int);
static void Require(bool value) { if (!value) std::abort(); }
class Output final : public AgcDriver::IVideoOutput, public AgcDriver::IRenderingWait,
    public AgcDriver::IFlipRequest, public std::enable_shared_from_this<Output> {
public:
    std::mutex mutex;
    std::condition_variable changed;
    BufferReuseTracker reuse;
    const std::uint64_t earlier = reuse.Reserve();
    std::uint64_t captured = 0, later = 0;
    bool entered = false, flipped = false;
    std::shared_ptr<AgcDriver::IRenderingWait> CaptureRenderingWait(std::uint32_t index) override {
        Require(index == 0);
        std::lock_guard lock(mutex);
        captured = reuse.Capture();
        return shared_from_this();
    }
    std::shared_ptr<AgcDriver::IFlipRequest> Reserve(const AgcDriver::FlipInfo& info) override {
        Require(info.index == 0);
        std::lock_guard lock(mutex);
        later = reuse.Reserve();
        return shared_from_this();
    }
    void Wait() override {
        std::unique_lock lock(mutex);
        entered = true;
        changed.notify_all();
        Require(changed.wait_for(lock, std::chrono::seconds(5), [&] { return reuse.IsComplete(captured); }));
    }
    void GpuReady(const std::shared_ptr<AgcDriver::FrameTiming>&) override {
        std::lock_guard lock(mutex);
        Require(reuse.IsComplete(captured));
        reuse.Complete(later);
        flipped = true;
    }
    void Fail(std::exception_ptr) noexcept override { std::abort(); }
};
int main() {
    auto output = std::make_shared<Output>();
    AgcDriverRegisterVideoOutput_nid_postfix(7, output);
    std::array<std::uint32_t, 11> words{};
    words.back() = 0x12345678;
    auto* cursor = words.data();
    const auto size = sceAgcDriverGetWaitRenderingPacketSizeInDwords();
    Require(size == AgcDriver::RenderingWaitPacketWords);
    const auto flipPacketSize = sceAgcDriverGetSetFlipPacketSizeInDwords();
    Require(flipPacketSize == AgcDriver::FlipPacketWords);
    Require(flipPacketSize == 6);
    Require(sceAgcDriverGetSetWorkloadsActivePacketSize() == 18);
    Require(sceAgcDriverGetSetWorkloadCompletePacketSize() == 12);
    Require(sceAgcDriverGetSetWorkloadStreamInactivePacketSize() == 9);
    Require(sceAgcDriverNopDataGetPacketSize(0) == 2 && sceAgcDriverNopDataGetPacketSize(1) == 3 && sceAgcDriverNopDataGetPacketSize(4) == 3);
    Require(sceAgcDriverNopDataGetPacketSize(5) == 4 && sceAgcDriverNopDataGetPacketSize(0xffffffffu) == 0x40000002u);
    Require(sceAgcDriverUserDataGetPacketSize(0) == 3 && sceAgcDriverUserDataGetPacketSize(1) == 4 && sceAgcDriverUserDataGetPacketSize(4) == 4);
    Require(sceAgcDriverUserDataGetPacketSize(5) == 9 && sceAgcDriverUserDataGetPacketSize(0xffffffffu) == 0x40000007u);
    bool rejected = false;
    try { sceAgcDriverWaitUntilSafeForRendering(&cursor, size - 1, 0, 7, 0); }
    catch (const std::invalid_argument&) { rejected = true; }
    Require(rejected && cursor == words.data() && words[0] == 0);
    Require(sceAgcDriverWaitUntilSafeForRendering(&cursor, size, 0, 7, 0) == 0);
    Require(cursor == words.data() + size);
    Require(words[0] == AgcDriver::RenderingWaitPacketHeader && words[1] == 7 && words[2] == 0 && words[3] == 0);
    const std::uint32_t flip[] = {AgcDriver::FlipPacketHeader, 7, 0, 1, 0, 0};
    Require(sizeof(flip) / sizeof(flip[0]) == flipPacketSize);
    for (auto word : flip) *cursor++ = word;
    Require(words.back() == 0x12345678);
    Packet packet{words.data(), 10, 0, {}};
    sceAgcDriverSubmitDcb(&packet);
    {
        std::unique_lock lock(output->mutex);
        Require(output->changed.wait_for(lock, std::chrono::seconds(5), [&] { return output->entered; }));
        Require(!output->flipped && output->later > output->captured);
        output->reuse.Complete(output->earlier);
        output->changed.notify_all();
    }
    AgcDriverWaitIdle_nid_postfix();
    Require(output->flipped);
    AgcDriverUnregisterVideoOutput_nid_postfix(7, output);
    LibcRunShutdown_nid_postfix();
}
