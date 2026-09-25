#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_VULKANDEVICE_HPP

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#include "Recompiler.hpp"
#include "prx/libSceAgcDriver/Execution/include/Presentation.hpp"
#include "prx/libSceAgcDriver/Execution/include/DisplayBuffer.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include <memory>
#include <optional>

namespace AgcDriver {

namespace Graphics {
class StorageTexture;
}

// Stage A of a dispatch's resource build (see VulkanDevice::PrepareDispatch); opaque to callers.
struct PreparedDispatch;

class VulkanDevice {
public:
    explicit VulkanDevice(const PresentationWindow* window = nullptr);
    ~VulkanDevice();
    VulkanDevice(const VulkanDevice&) = delete;
    VulkanDevice& operator=(const VulkanDevice&) = delete;
    ShaderRecompiler::SpirvTarget Target() const;
    // Drains everything under the caller's GpuMutex: recorder Sync plus vkDeviceWaitIdle. For suspend,
    // resize, device replacement, CPU fill fallback and APS5_DRAIN_ALL; the packet-loop drains use the
    // three-step form below so the GPU wait happens without the mutex.
    void WaitIdle();
    // Sends recorded work to the GPU without waiting for it. With `reapFirst` it first retires batches
    // that already finished, so the in-flight list stays short (APS5_NO_OPPORTUNISTIC_REAP=1 skips
    // that). A reap runs completion actions, and a write-back can wait for a later batch under the
    // mutex through the flush hook, so a thread submitting on another queue's behalf (a WAIT_REG_MEM
    // poller, a compute worker's between-packet flush) passes false: it must only submit, never wait.
    void SubmitRecorded(bool reapFirst = true);
    // Unlocked drain: SubmitAndEpoch (under the mutex) returns the serial covering every recorded
    // batch (0: nothing in flight); WaitRecorded(serial) waits for it WITHOUT the mutex (the caller
    // holds a shared_ptr to this device); ReapRecorded(serial) (under the mutex) runs the completions
    // of the batches up to it. CanWaitUnlocked is false without timeline semaphores: use WaitIdle.
    bool CanWaitUnlocked() const;
    std::uint64_t SubmitAndEpoch();
    void WaitRecorded(std::uint64_t serial);
    void ReapRecorded(std::uint64_t serial);
    // Completes the batches that already finished (non-blocking); under the mutex.
    void ReapRecorded();
    // Records a store of `bytes` at a guest address behind the recorded work, so the value appears
    // once that work completed; the batch is submitted by the queue worker's flush rules (or at once
    // with APS5_LABEL_SUBMIT_NOW=1). `stamp` is the record-order stamp and `queue` the recording
    // queue for the recorder's pending-label table. Returns 0 when recorded on the GPU, 5 when kept
    // as a completion action behind pending write-backs and 6 when kept as one because the memory is
    // not host-imported so the GPU cannot store it (both land when their batch is reaped), else why
    // the CPU must write it:
    // 1 nothing recorded (the write is already ordered), 2 write-backs pending and 3 memory not
    // imported (both only with APS5_DRAIN_COMPLETION_LABELS=1), 4 unsuitable size or alignment.
    // `reapFirst` retires finished batches before the checks; a caller recording a group of labels
    // under one lock passes it for the first label only.
    int WriteLabelOnGpu(std::uint64_t address, std::span<const std::byte> bytes, std::uint64_t stamp, std::uint32_t queue, bool reapFirst = true);
    // Pending-label table lookup and open-batch overlap test for WAIT_REG_MEM (see Recorder).
    std::optional<std::uint64_t> PendingLabel(std::uint64_t address, std::size_t bytes, std::uint64_t afterStamp, std::uint32_t& queue) const;
    bool OpenWriteOverlaps(std::uint64_t address, std::size_t bytes) const;
    // Fills [address, address + bytes) of host-imported guest memory with a repeating 16-byte pattern,
    // recorded behind the open batch; false when the range is not imported (the caller stores it).
    // Recorded GPU stores over the range are ordered before the fill by its barrier; finished
    // batches are retired first, and it waits only for stores a batch's completion makes on the
    // CPU (a copied buffer's write-back, a deferred label). Debug aid: APS5_FILL_SYNC=1 waits for
    // every recorded store over the range.
    bool FillBuffer(std::uint64_t address, std::size_t bytes, std::span<const std::uint32_t, 4> pattern);
    void ResolveMemory(std::uint64_t address, std::size_t bytes, bool writable);
    void* Window() const;
    void Resize(std::uint32_t width, std::uint32_t height);
    bool Presentable() const;
    // A presentation is four steps so the presenter holds GuestMemory::GpuMutex only while it touches
    // the queue: AcquireImage takes the next swapchain image (no mutex needed: the swapchain and its
    // fences are the presenter's own, and in FIFO mode this is where a frame waits for the vblank);
    // PresentClear/PresentDisplayBuffer record and submit the frame (under the mutex) and return
    // whether one is in flight; FinishPresent waits for its render fence (no mutex); QueuePresent
    // hands the image to the swapchain (under the mutex again). AcquireImage returns false when the
    // swapchain is out of date (the frame is dropped; the next Resize recreates it); a present without
    // a prior AcquireImage acquires itself. PresentPixels does all steps itself.
    bool AcquireImage();
    bool PresentClear(std::uint32_t width, std::uint32_t height, bool opaque);
    void PresentPixels(std::uint32_t width, std::uint32_t height, std::span<const std::byte> pixels);
    bool PresentDisplayBuffer(const DisplayBuffer& buffer);
    void FinishPresent();
    void QueuePresent();
    // Stage A of a dispatch's resource build, run WITHOUT GuestMemory::GpuMutex before Dispatch or
    // DispatchIndirect: the binding plan, data buffers, imports already serving the guest buffers,
    // read-only copies and the descriptor set (see ShaderResources). The dispatch completes it
    // under the mutex (stage B: texture lookups, the rest of the upload, descriptor writes) and
    // records. Null when nothing is prepared: the resource cache may serve the dispatch (its
    // Revalidate stays under the mutex), or APS5_LOCKED_BUILD=1 keeps the whole build under it as
    // before. `shader` and `snapshots` must outlive the dispatch.
    std::shared_ptr<PreparedDispatch> PrepareDispatch(const ShaderRecompiler::RecompileResult& shader, std::span<const Graphics::GuestMemorySnapshot> snapshots);
    void Dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}, std::uint64_t programAddress = 0, std::shared_ptr<PreparedDispatch> prepared = nullptr);
    // A dispatch whose group counts are the three dwords at `arguments` in guest memory
    // (DISPATCH_INDIRECT): the GPU reads them in place from the host import, ordered after everything
    // recorded before, so the CPU never waits for the shader that wrote them. When the GPU could not
    // see the current bytes the counts are read on the CPU instead (through the flush hook, as the
    // driver resolved every indirect dispatch before) and the dispatch is recorded as a direct one:
    // cpuReason 1 storage-image results were pending over them, 2 a recorded dispatch writes them
    // through a copied buffer (its CPU write-back lands only when the batch is reaped) or a label
    // over those dwords is pending, 3 the memory is not host-imported; 0 when recorded GPU-side. argumentReadMs is
    // what that CPU read (its sync) took. The device limit on group counts is not checked GPU-side.
    // Debug aid: APS5_NO_GPU_INDIRECT=1 (in the driver) keeps every indirect dispatch on the CPU path.
    struct IndirectOutcome {
        int cpuReason;
        double argumentReadMs;
    };
    IndirectOutcome DispatchIndirect(const ShaderRecompiler::RecompileResult& shader, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots = {}, std::uint64_t programAddress = 0, std::shared_ptr<PreparedDispatch> prepared = nullptr);
    void Draw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});
    void EnqueueDraw(const Graphics::State& graphics, const Pm4::DrawParameters& draw, std::span<const Graphics::CompiledShader> shaders, std::span<const Graphics::GuestMemorySnapshot> snapshots = {});

private:
    Graphics::Context graphicsContext() const;
    // Body of Dispatch and DispatchIndirect: `arguments` 0 dispatches x, y, z groups.
    IndirectOutcome dispatch(const ShaderRecompiler::RecompileResult& shader, std::uint32_t x, std::uint32_t y, std::uint32_t z, std::uint64_t arguments, std::span<const Graphics::GuestMemorySnapshot> snapshots, std::uint64_t programAddress, std::shared_ptr<PreparedDispatch> prepared);
    bool present(std::uint32_t width, std::uint32_t height, bool opaque, std::span<const std::byte> pixels, const DisplayBuffer* display = nullptr, const std::shared_ptr<Graphics::StorageTexture>& resident = nullptr, VkFilter residentFilter = VK_FILTER_LINEAR, bool dumpFrame = false);
    void writeFrameDump();
    struct State;
    std::unique_ptr<State> state;
};

}

#endif
