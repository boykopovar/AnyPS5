#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#include <array>
#include <atomic>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::GuestMemory::GpuMutex;

// A compute-capable device with host imports (VK_EXT_external_memory_host) when the host offers
// them, as the driver creates its own; the recorder's batches need a real queue and real fences.
class Device {
public:
    Device() {
#ifdef _WIN32
        library = SDL_LoadObject("vulkan-1.dll");
#else
        library = SDL_LoadObject("libvulkan.so.1");
#endif
        Require(library != nullptr, "cannot load Vulkan");
        try {
            instanceProc = reinterpret_cast<PFN_vkGetInstanceProcAddr>(SDL_LoadFunction(library, "vkGetInstanceProcAddr"));
            Require(instanceProc != nullptr, "missing Vulkan instance resolver");
            VkApplicationInfo application{VK_STRUCTURE_TYPE_APPLICATION_INFO};
            application.apiVersion = VK_API_VERSION_1_1;
            VkInstanceCreateInfo info{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
            info.pApplicationInfo = &application;
            Check(function<PFN_vkCreateInstance>("vkCreateInstance")(&info, nullptr, &instance), "vkCreateInstance");
            std::uint32_t count = 0;
            const auto enumerate = function<PFN_vkEnumeratePhysicalDevices>("vkEnumeratePhysicalDevices");
            Check(enumerate(instance, &count, nullptr), "vkEnumeratePhysicalDevices");
            Require(count != 0, "no Vulkan device");
            std::vector<VkPhysicalDevice> devices(count);
            Check(enumerate(instance, &count, devices.data()), "vkEnumeratePhysicalDevices");
            context.physical = devices.front();
            const auto extensions = function<PFN_vkEnumerateDeviceExtensionProperties>("vkEnumerateDeviceExtensionProperties");
            Check(extensions(context.physical, nullptr, &count, nullptr), "vkEnumerateDeviceExtensionProperties");
            std::vector<VkExtensionProperties> available(count);
            Check(extensions(context.physical, nullptr, &count, available.data()), "vkEnumerateDeviceExtensionProperties");
            const auto hasExtension = [&](const char* name) {
                for (const auto& extension : available) {
                    if (std::strcmp(extension.extensionName, name) == 0) return true;
                }
                return false;
            };
            auto bytes = AgcDriver::QueryBdaByteFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            auto address = AgcDriver::QueryBdaFeatures(context.physical, function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2"), available);
            const auto queues = function<PFN_vkGetPhysicalDeviceQueueFamilyProperties>("vkGetPhysicalDeviceQueueFamilyProperties");
            queues(context.physical, &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            queues(context.physical, &count, families.data());
            std::uint32_t family = 0;
            while (family < count && (families[family].queueFlags & VK_QUEUE_COMPUTE_BIT) == 0) ++family;
            Require(family < count, "no Vulkan compute queue");
            const float priority = 1;
            VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
            queue.queueFamilyIndex = family;
            queue.queueCount = 1;
            queue.pQueuePriorities = &priority;
            VkPhysicalDeviceFeatures enabled{};
            enabled.shaderInt64 = VK_TRUE;
            address.pNext = &bytes;
            std::vector<const char*> extensionsEnabled{VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME, VK_KHR_8BIT_STORAGE_EXTENSION_NAME};
            if (hasExtension(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME)) {
                extensionsEnabled.push_back(VK_EXT_EXTERNAL_MEMORY_HOST_EXTENSION_NAME);
                VkPhysicalDeviceExternalMemoryHostPropertiesEXT hostProperties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTERNAL_MEMORY_HOST_PROPERTIES_EXT};
                VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2, &hostProperties};
                function<PFN_vkGetPhysicalDeviceProperties2>("vkGetPhysicalDeviceProperties2")(context.physical, &properties);
                context.hostImportAlignment = hostProperties.minImportedHostPointerAlignment;
            }
            VkDeviceCreateInfo device{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO, &address};
            device.queueCreateInfoCount = 1;
            device.pQueueCreateInfos = &queue;
            device.enabledExtensionCount = static_cast<std::uint32_t>(extensionsEnabled.size());
            device.ppEnabledExtensionNames = extensionsEnabled.data();
            device.pEnabledFeatures = &enabled;
            Check(function<PFN_vkCreateDevice>("vkCreateDevice")(context.physical, &device, nullptr, &context.device), "vkCreateDevice");
            context.deviceProc = function<PFN_vkGetDeviceProcAddr>("vkGetDeviceProcAddr");
            function<PFN_vkGetPhysicalDeviceMemoryProperties>("vkGetPhysicalDeviceMemoryProperties")(context.physical, &context.memory);
            VkPhysicalDeviceProperties properties{};
            function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties")(context.physical, &properties);
            context.limits = properties.limits;
            context.bufferDeviceAddress = true;
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
            pool.queueFamilyIndex = family;
            Check(context.Function<PFN_vkCreateCommandPool>("vkCreateCommandPool")(context.device, &pool, nullptr, &context.pool), "vkCreateCommandPool");
        } catch (...) {
            release();
            throw;
        }
    }

    ~Device() { release(); }
    const Context& GetContext() const { return context; }
    void WaitQueue() const { Check(context.Function<PFN_vkQueueWaitIdle>("vkQueueWaitIdle")(context.queue), "vkQueueWaitIdle"); }

private:
    template<typename TFunction>
    TFunction function(const char* name) const {
        const auto result = reinterpret_cast<TFunction>(instanceProc(instance, name));
        Require(result != nullptr, name);
        return result;
    }

    void release() noexcept {
        if (context.pool != VK_NULL_HANDLE) context.Function<PFN_vkDestroyCommandPool>("vkDestroyCommandPool")(context.device, context.pool, nullptr);
        context.bufferPool.reset();
        if (context.device != VK_NULL_HANDLE) function<PFN_vkDestroyDevice>("vkDestroyDevice")(context.device, nullptr);
        if (instance != VK_NULL_HANDLE) function<PFN_vkDestroyInstance>("vkDestroyInstance")(instance, nullptr);
        if (library != nullptr) SDL_UnloadObject(library);
    }

    void* library = nullptr;
    PFN_vkGetInstanceProcAddr instanceProc = nullptr;
    VkInstance instance = VK_NULL_HANDLE;
    Context context{};
};

using Kind = Recorder::ReadKind;

void readTrackingTests(const Device& device, Recorder& recorder) {
    Require(Recorder::ReadTracking(), "read tracking is off (APS5_COPY_READ_TRACKING=0 set?)");
    Require(recorder.Idle() && !recorder.PendingReadOverlaps(0x10000, 16), "a fresh recorder reports a read");
    recorder.NotePendingRead(0x10000, 0x100, Kind::DispatchElement);
    Require(recorder.Recording(), "a read note did not open a batch");
    Require(recorder.PendingReadOverlaps(0x10080, 4) && recorder.PendingReadOverlaps(0xff00, 0x101) && recorder.PendingReadOverlaps(0x100ff, 1), "an open batch's read is not seen");
    Require(!recorder.PendingReadOverlaps(0x10100, 4) && !recorder.PendingReadOverlaps(0xff00, 0x100) && !recorder.PendingReadOverlaps(0x10000, 0), "a disjoint range is reported as read");
    const auto open = recorder.DescribePendingRead(0x10000, 4);
    Require(open.has_value() && open->open && !open->signaled && open->kind == Kind::DispatchElement && open->serial == recorder.Submissions() + 1, "the open batch's read is described wrongly");
    recorder.Submit();
    Require(!recorder.Recording() && !recorder.Idle(), "the batch is not in flight after Submit");
    Require(recorder.PendingReadOverlaps(0x10000, 4, false), "the raw scan does not see the in-flight batch's read");
    // An empty batch completes at once: once its fence signaled, its read is no reader any more,
    // although the batch stays in flight until it is reaped.
    device.WaitQueue();
    Require(!recorder.PendingReadOverlaps(0x10000, 4), "a signaled batch's read still refuses");
    Require(!recorder.Idle(), "an overlap query reaped the batch");
    const auto signaled = recorder.DescribePendingRead(0x10000, 4);
    Require(signaled.has_value() && !signaled->open && signaled->signaled && signaled->serial == recorder.Submissions(), "the signaled batch's read is described wrongly");
    const auto counts = Recorder::ReadCounts();
    Require(counts.noted == 1 && counts.staleIgnored >= 1 && counts.hits[static_cast<std::size_t>(Kind::DispatchElement)] >= 3 && counts.queries >= 6, "read counters are off");
    recorder.Sync();
    Require(recorder.Idle() && !recorder.PendingReadOverlaps(0x10000, 4, false) && !recorder.DescribePendingRead(0x10000, 4).has_value(), "a read outlived its batch");
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> ranges{{0x20000, 0x21000}, {0x30000, 0x30010}, {0x40000, 0x40000}};
    recorder.NotePendingReads(ranges, Kind::AddressBased);
    Require(recorder.PendingReadOverlaps(0x20fff, 1) && recorder.PendingReadOverlaps(0x30000, 16) && !recorder.PendingReadOverlaps(0x21000, 16) && !recorder.PendingReadOverlaps(0x40000, 16), "noted ranges are not seen as reads");
    const auto batch = recorder.DescribePendingRead(0x30008, 4);
    Require(batch.has_value() && batch->kind == Kind::AddressBased && batch->open, "a noted range has the wrong reader kind");
    Require(Recorder::ReadCounts().hits[static_cast<std::size_t>(Kind::AddressBased)] >= 2, "address-based hits are not counted");
    recorder.Sync();
    Require(recorder.Idle() && !recorder.PendingReadOverlaps(0x20000, 0x1000, false), "noted ranges outlived their batch");
}

void writeSettledTests(const Device& device, Recorder& recorder) {
    Require(recorder.PendingWriteSettled(0x50000, 0x100), "an unwritten range is not settled");
    recorder.NotePendingWrite(0x50000, 0x100);
    Require(recorder.PendingWriteOverlaps(0x50000, 0x100) && !recorder.PendingWriteSettled(0x50000, 0x100) && recorder.PendingWriteSettled(0x50100, 0x100), "an open batch's write counts as settled");
    recorder.Submit();
    device.WaitQueue();
    Require(recorder.PendingWriteOverlaps(0x50000, 0x100) && recorder.PendingWriteSettled(0x50000, 0x100), "a signaled batch's write is not settled");
    recorder.NotePendingWrite(0x50080, 0x10);
    Require(!recorder.PendingWriteSettled(0x50000, 0x100) && recorder.PendingWriteSettled(0x50000, 0x80), "a later open write over the range still counts as settled");
    recorder.Sync();
    Require(recorder.Idle() && !recorder.PendingWriteOverlaps(0x50000, 0x100), "writes outlived their batches");
}

// Completion counting: a write-back completion (OnComplete) is pending until its batch finished;
// a completion label the GPU also stored (AfterCompletions storedOnGpu) is pending only once a CPU
// write-back overlapped it (NoteWrittenBack, once per label), one the GPU has no view of from its
// registration; both counts return to 0 when the batch finishes.
void completionCountTests(const Device& device, Recorder& recorder) {
    alignas(64) static std::uint32_t memory[64];
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const std::array<std::byte, 4> value{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    Require(recorder.Idle() && Recorder::PendingCompletionLabels() == 0 && Recorder::PendingWriteBackCompletions() == 0, "a fresh recorder reports pending completions");
    int ran = 0;
    recorder.OnComplete([&] { ++ran; });
    Require(Recorder::PendingWriteBackCompletions() == 1 && recorder.HasCompletions(), "a write-back completion is not pending");
    // APS5_COUNT_ALL_COMPLETION_LABELS=1 (or APS5_LABEL_STORE_ALWAYS=1) counts the GPU-stored
    // labels at registration, as before; the write-back then changes nothing.
    const bool countAll = std::getenv("APS5_COUNT_ALL_COMPLETION_LABELS") != nullptr || std::getenv("APS5_LABEL_STORE_ALWAYS") != nullptr;
    const std::uint64_t registered = countAll ? 2 : 0;
    recorder.AfterCompletions(base, value, 3, 0, true);
    recorder.AfterCompletions(base + 8, value, 4, 0, true);
    Require(Recorder::PendingCompletionLabels() == registered, "a GPU-stored completion label counts before any write-back");
    recorder.AfterCompletions(base + 0x40, value, 5, 0, false);
    Require(Recorder::PendingCompletionLabels() == registered + 1, "a label the GPU has no view of is not pending");
    // A late lookup (stamp <= afterStamp) of a label whose value reaches memory only through the
    // batch's completion action is refused as BehindCompletion (a poller reaps for it); a GPU-stored
    // one keeps the ordinary reason (unclosed group, or trust off) until a write-back overlaps it.
    using Refusal = Recorder::LabelRefusal;
    const Refusal ordinary = Recorder::LateTrust() ? Refusal::Unclosed : Refusal::TrustOff;
    Refusal refusal{};
    Require(!Recorder::LookupLabel(base + 0x40, 4, 5, &refusal).has_value() && refusal == Refusal::BehindCompletion, "a not-imported completion label is not refused as behind a completion");
    Require(!Recorder::LookupLabel(base, 4, 3, &refusal).has_value() && refusal == (countAll ? Refusal::BehindCompletion : ordinary), "a GPU-stored label is refused as behind a completion before any write-back");
    Require(Recorder::LookupLabel(base, 4, 2, &refusal).has_value() && refusal == Refusal::None, "an early lookup of a completion label is refused");
    Recorder::NoteWrittenBack(base + 0x80, 0x40);
    Require(Recorder::PendingCompletionLabels() == registered + 1, "a disjoint write-back counted a label");
    Recorder::NoteWrittenBack(base, 4);
    Recorder::NoteWrittenBack(base + 2, 4);
    const std::uint64_t counted = countAll ? 3 : 2;
    Require(Recorder::PendingCompletionLabels() == counted, "a write-back over a GPU-stored label did not count it exactly once");
    Require(!Recorder::LookupLabel(base, 4, 3, &refusal).has_value() && refusal == Refusal::BehindCompletion, "a written-back GPU-stored label is not refused as behind a completion");
    Require(!Recorder::LookupLabel(base + 8, 4, 4, &refusal).has_value() && refusal == (countAll ? Refusal::BehindCompletion : ordinary), "an untouched GPU-stored label became behind a completion");
    recorder.Submit();
    device.WaitQueue();
    Require(Recorder::PendingCompletionLabels() == counted && Recorder::PendingWriteBackCompletions() == 1, "counts dropped before the batch finished");
    memory[0] = 7;
    memory[2] = 7;
    memory[16] = 7;
    recorder.Sync();
    Require(ran == 1 && Recorder::PendingCompletionLabels() == 0 && Recorder::PendingWriteBackCompletions() == 0 && recorder.Idle(), "counts did not return to 0 at finish");
    const bool storeAlways = std::getenv("APS5_LABEL_STORE_ALWAYS") != nullptr;
    Require(memory[0] == 1 && memory[2] == (storeAlways ? 1u : 7u) && memory[16] == 1, "completion stores ran for the wrong labels (overlapped and not-imported ones store, the untouched GPU-stored one skips)");
}

void labelTests(Recorder& recorder) {
    const std::array<std::byte, 4> value{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    Require(!recorder.PendingLabelIn(0x60000, 0x100), "an empty table reports a label");
    // A label this worker queued but has not recorded yet is inside the range for a CPU store decision.
    Recorder::NoteQueuedLabel(0x60010, value, 1, AgcDriver::GuestMemory::GpuLockThreadTag());
    Require(recorder.PendingLabelIn(0x60010, 4) && recorder.PendingLabelIn(0x60000, 0x100) && !recorder.PendingLabelIn(0x60014, 4) && !recorder.PendingLabelIn(0x60000, 0x10), "a queued label is not seen by PendingLabelIn");
    Recorder::ForgetQueuedLabels();
    Require(!recorder.PendingLabelIn(0x60000, 0x100), "a forgotten queued label is still pending");
    recorder.NoteLabel(0x60020, value, 2, 0);
    Require(recorder.PendingLabelIn(0x60020, 4) && recorder.PendingLabelIn(0x60000, 0x100) && !recorder.PendingLabelIn(0x60000, 0x20), "a recorded label is not seen by PendingLabelIn");
    recorder.Sync();
    Require(!recorder.PendingLabelIn(0x60000, 0x100), "a recorded label outlived its batch");
}

// The late rule (Recorder::NoteLabel): an entry with stamp <= afterStamp is served once its group
// closed, unless a non-label write covered it, it is queued, or APS5_LABEL_TRUST_LATE=0 (run the
// binary with that set for the switch-off case).
void lateLabelTests(Recorder& recorder) {
    using Refusal = Recorder::LabelRefusal;
    const std::array<std::byte, 4> one{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    const std::array<std::byte, 4> two{std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}};
    const std::array<std::byte, 8> pair{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2}, std::byte{0}, std::byte{0}, std::byte{0}};
    const auto tag = AgcDriver::GuestMemory::GpuLockThreadTag();
    constexpr std::uint64_t a = 0x70000, b = 0x70100, c = 0x70200, d = 0x70300;
    Refusal refusal{};
    const auto before = Recorder::LateCounts();
    recorder.NoteLabel(a, one, 10, 0);
    const auto early = Recorder::LookupLabel(a, 4, 5, &refusal);
    Require(early.has_value() && !early->late && early->value == 1 && early->queue == 0 && early->stamp == 10 && early->generation == 0 && refusal == Refusal::None, "an early lookup is wrong");
    auto late = Recorder::LookupLabel(a, 4, 10, &refusal);
    if (!Recorder::LateTrust()) {
        Require(!late.has_value() && refusal == Refusal::TrustOff, "(1) a late lookup is not refused with the switch off");
        Require(Recorder::LateCounts().candidates == before.candidates + 1, "(1) the late candidate is not counted with the switch off");
        Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
        Require(!Recorder::LookupLabel(a, 4, 10, &refusal).has_value() && refusal == Refusal::TrustOff, "(1) a closed group is served with the switch off");
        Require(Recorder::LookupLabel(a, 4, 5).has_value(), "(1) the early lookup broke with the switch off");
        recorder.Sync();
        Require(!Recorder::LookupLabel(a, 4, 5).has_value(), "(7) the entry outlived its batch");
        std::cout << "late label tests ran with APS5_LABEL_TRUST_LATE=0\n";
        return;
    }
    Require(!late.has_value() && refusal == Refusal::Unclosed, "(2) an unclosed group is served");
    Require(Recorder::LateCounts().candidates == before.candidates + 1 && Recorder::LateCounts().unclosed == before.unclosed + 1, "(2) the unclosed refusal is not counted");
    const auto generation = AgcDriver::GuestMemory::TrackerGeneration();
    Recorder::CloseLabelGroup(generation);
    late = Recorder::LookupLabel(a, 4, 10, &refusal);
    Require(late.has_value() && late->late && late->value == 1 && late->generation == generation && late->stamp == 10 && refusal == Refusal::None, "(2) a closed group is not served");
    Require(Recorder::LookupLabel(a, 4, 9)->late == false, "(2) an early lookup became late");
    // (3) A disjoint write leaves the entry alone; an overlapping non-label write refuses the late
    // lookup only (the early one still composes what memory will hold in order).
    recorder.NotePendingWrite(a + 4, 16);
    Require(Recorder::LookupLabel(a, 4, 10).has_value(), "(3) a disjoint write flagged the entry");
    recorder.NotePendingWrite(a - 8, 12);
    late = Recorder::LookupLabel(a, 4, 10, &refusal);
    Require(!late.has_value() && refusal == Refusal::Overwritten, "(3) an overwritten entry is served late");
    Require(Recorder::LookupLabel(a, 4, 5).has_value(), "(3) an overwrite refused the early lookup");
    // The label's own note (inside NoteLabel) is no overwrite: an 8-byte label closed as a group.
    recorder.NoteLabel(b, pair, 20, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    const auto wide = Recorder::LookupLabel(b, 8, 20, &refusal);
    Require(wide.has_value() && wide->late && wide->value == 0x200000001ull && refusal == Refusal::None, "(3) the label's own note refused it");
    // (4) A later label on the dword replaces the entry: unclosed until its own group closes.
    recorder.NoteLabel(a, two, 30, 0);
    Require(!Recorder::LookupLabel(a, 4, 40, &refusal).has_value() && refusal == Refusal::Unclosed, "(4) a replaced entry kept the old close or flag");
    const auto replaced = Recorder::LookupLabel(a, 4, 25);
    Require(replaced.has_value() && !replaced->late && replaced->value == 2 && replaced->stamp == 30, "(4) the replaced entry's early lookup is wrong");
    const auto generation2 = AgcDriver::GuestMemory::TrackerGeneration();
    Recorder::CloseLabelGroup(generation2);
    late = Recorder::LookupLabel(a, 4, 40, &refusal);
    Require(late.has_value() && late->late && late->value == 2 && late->generation == generation2, "(4) the replaced entry is not late-trustable after its close");
    // (5) Queued entries: another queue's is never seen, this queue's is early-only.
    const std::uint32_t other = tag == 7 ? 8 : 7;
    Recorder::NoteQueuedLabel(d, one, 50, other);
    Require(!Recorder::LookupLabel(d, 4, 60, &refusal).has_value() && refusal == Refusal::None && !Recorder::LookupLabel(d, 4, 40).has_value(), "(5) another queue's queued label is served");
    Recorder::NoteQueuedLabel(d, one, 51, tag);
    Recorder::ForgetQueuedLabels();
    Recorder::NoteQueuedLabel(c, one, 50, tag);
    Require(Recorder::LookupLabel(c, 4, 40).has_value() && !Recorder::LookupLabel(c, 4, 40)->late, "(5) this queue's queued label is not served early");
    Require(!Recorder::LookupLabel(c, 4, 60, &refusal).has_value() && refusal == Refusal::Queued, "(5) a queued entry is late-trusted");
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(!Recorder::LookupLabel(c, 4, 60, &refusal).has_value() && refusal == Refusal::Queued, "(5) a close closed a queued entry");
    Recorder::ForgetQueuedLabels();
    Require(!Recorder::LookupLabel(c, 4, 40).has_value(), "(5) a forgotten queued entry is still served");
    const auto after = Recorder::LateCounts();
    Require(after.queued == before.queued + 2 && after.overwritten == before.overwritten + 1 && after.unclosed == before.unclosed + 2 && after.candidates >= before.candidates + 6, "late counters are off");
    // (7) Entries leave the table with their batch.
    recorder.Sync();
    Require(!Recorder::LookupLabel(a, 4, 5).has_value() && !Recorder::LookupLabel(b, 8, 5).has_value() && !recorder.PendingLabelIn(0x70000, 0x400), "(7) entries outlived their batch");
}

// (6) GuestMemory::UnchangedSinceCollected: false outside the watched arena, and around a
// MarkWritten or a CPU write of the block inside it.
void unchangedSinceTests() {
    using namespace AgcDriver::GuestMemory;
    static std::uint32_t outside[16];
    Require(!UnchangedSinceCollected(reinterpret_cast<std::uint64_t>(outside), 4, TrackerGeneration()), "(6) a range outside the arena is unchanged");
    if (!GuestArena::GuestArenaAvailable_nid_postfix()) {
        std::cout << "guest arena unavailable: UnchangedSinceCollected not tested inside it\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes);
#ifdef _WIN32
    Require(VirtualAlloc(block, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot commit the arena block");
#endif
    auto* words = static_cast<volatile std::uint32_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    words[0] = 0;
    if (!GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        Require(!UnchangedSinceCollected(address, 4, TrackerGeneration()), "(6) an unwatched arena reports unchanged");
        std::cout << "arena not write-watched: UnchangedSinceCollected is always false\n";
        return;
    }
    CollectWritesUncached(address, 4);
    const auto generation = TrackerGeneration();
    Require(UnchangedSinceCollected(address, 4, generation), "(6) an untouched range is not unchanged");
    Require(UnchangedSinceCollected(address, 4, generation), "(6) the check itself dirtied the range");
    Require(!UnchangedSinceCollected(address, 4, 0), "(6) generation 0 is unchanged");
    MarkWritten(address, 4);
    Require(UnchangedSinceCollected(address, 4, generation), "(6) a MarkWritten of the block (a GPU label record) refuses");
    Require(!UnchangedSince(address, 4, generation), "(6) a MarkWritten of the block is not seen by UnchangedSince");
    const auto generation2 = TrackerGeneration();
    Require(UnchangedSinceCollected(address, 4, generation2), "(6) unchanged after the stamp fails");
    words[1] = 1;
    Require(!UnchangedSinceCollected(address, 4, generation2), "(6) a CPU write to the page is not seen");
    Require(UnchangedSinceCollected(address, 4, TrackerGeneration()), "(6) the collect did not reset the page");
    const auto generation3 = TrackerGeneration();
    words[1] = 2;
    CollectWritesUncached(address, 4);
    Require(!UnchangedSinceCollected(address, 4, generation3), "(6) a CPU write collected by another caller is not seen");
#ifdef _WIN32
    VirtualFree(block, bytes, MEM_DECOMMIT);
#endif
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
}

// (9) ProvedClearKeys: a surface's DCC keys are scanned once and answered from the proof after,
// until the key range is stamped (a GPU key store's MarkWritten) or the CPU writes it; a scan made
// while recorded work still writes the keys is not kept. With APS5_NO_KEY_FAST_PATH=1 every call
// scans and no proof is stored.
void keyProofTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "guest arena unavailable or not write-watched: key proofs not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, bytes);
#ifdef _WIN32
    Require(VirtualAlloc(block, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot commit the arena block");
#endif
    struct Release {
        void* block;
        ~Release() {
#ifdef _WIN32
            VirtualFree(block, bytes, MEM_DECOMMIT);
#endif
            GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
        }
    } release{block};
    constexpr std::size_t keyCount = 1024;
    constexpr std::uint64_t surfaceBytes = keyCount * 256;
    auto* keys = static_cast<std::uint8_t*>(block);
    std::memset(keys, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    GuestTextureResource resource{};
    resource.baseAddress = address + 8192;
    resource.width = 256;
    resource.height = 256;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dccAddress = address;
    DccKeyProof proof;
    auto before = KeyProofCounts();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "(9) 0x00 keys are not a 0000 clear");
    auto after = KeyProofCounts();
    Require(after.scanned == before.scanned + 1 && after.proved == before.proved, "(9) the first call did not scan");
    if (!KeyFastPath()) {
        Require(proof.generation == 0, "(9) APS5_NO_KEY_FAST_PATH stored a proof");
        Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000 && proof.generation == 0, "(9) APS5_NO_KEY_FAST_PATH proved keys");
        std::cout << "key fast path off: every call scans\n";
        return;
    }
    Require(proof.generation != 0 && proof.keys == DccKeys::Clear0000 && after.unstable == before.unstable, "(9) a stable scan left no proof");
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "(9) the proof answers other keys");
    before = KeyProofCounts();
    Require(before.proved == after.proved + 1 && before.scanned == after.scanned, "(9) the second call scanned");
    const auto generation = proof.generation;
    MarkWritten(address, keyCount);
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "(9) unchanged bytes read differently");
    after = KeyProofCounts();
    Require(after.scanned == before.scanned + 1 && after.proved == before.proved && proof.generation > generation, "(9) a stamped key range was proved");
    std::memset(keys, 0x40, keyCount);
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001, "(9) a CPU write of the keys was not seen");
    before = KeyProofCounts();
    Require(before.scanned == after.scanned + 1 && proof.keys == DccKeys::Clear0001, "(9) the CPU write did not make a scan");
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && KeyProofCounts().proved == before.proved + 1, "(9) the new keys were not proved");
    // A recorded write over the keys (noted and stamped as the driver's key stores are): the scans
    // are not kept until its batch signaled.
    recorder.NotePendingWrite(address, keyCount);
    MarkWritten(address, keyCount);
    before = KeyProofCounts();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && proof.generation == 0, "(9) a scan under a pending write was kept");
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && proof.generation == 0, "(9) a second scan under a pending write was kept");
    after = KeyProofCounts();
    Require(after.scanned == before.scanned + 2 && after.unstable == before.unstable + 2 && after.proved == before.proved, "(9) unstable scans were miscounted");
    recorder.Submit();
    device.WaitQueue();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && proof.generation != 0, "(9) a signaled write kept the scan unstable");
    recorder.Sync();
    before = KeyProofCounts();
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0001 && KeyProofCounts().proved == before.proved + 1, "(9) the proof did not hold after the batch finished");
}

// (8) The group close against a collect in progress: a CPU store made after the close must refuse
// the entry although another thread's resetting walk of the label's page (over a large range) may
// absorb the store and stamp the block with the generation it took before the close. And an entry
// whose batch was submitted before the close stays unclosed.
void closeRaceTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    using Refusal = Recorder::LabelRefusal;
    const std::array<std::byte, 4> one{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    Refusal refusal{};
    if (!Recorder::LateTrust()) return;
    recorder.NoteLabel(0x80000, one, 70, 0);
    recorder.Submit();
    Recorder::CloseLabelGroup(TrackerGeneration());
    Require(!Recorder::LookupLabel(0x80000, 4, 70, &refusal).has_value() && refusal == Refusal::Unclosed, "(8) a submitted batch's entry was closed");
    device.WaitQueue();
    recorder.Sync();
    if (!GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) return;
    constexpr std::size_t bytes = 1u << 20;
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536);
#ifdef _WIN32
    Require(VirtualAlloc(block, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot commit the arena block");
#endif
    const auto base = reinterpret_cast<std::uint64_t>(block);
    // Late in the range, so the walker is usually still ahead of the page when the store lands.
    const auto label = base + 4096 * 250;
    auto* word = reinterpret_cast<volatile std::uint32_t*>(label);
    *word = 0;
    CollectWritesUncached(base, bytes);
    // Joined on every exit, so a failed Require reports instead of terminating on the thread.
    struct Walker {
        std::atomic<bool> stop{false};
        std::thread thread;
        ~Walker() {
            stop.store(true, std::memory_order_relaxed);
            if (thread.joinable()) thread.join();
        }
    } walker;
    walker.thread = std::thread([&] {
        while (!walker.stop.load(std::memory_order_relaxed)) CollectWritesUncached(base, bytes);
    });
    for (std::uint32_t i = 0; i < 4000; ++i) {
        const std::uint64_t stamp = 100 + i;
        recorder.NoteLabel(label, one, stamp, 0);
        Recorder::CloseLabelGroup(TrackerGeneration());
        *word = i + 2;
        const auto hit = Recorder::LookupLabel(label, 4, stamp, &refusal);
        Require(hit.has_value() && hit->late && hit->generation != 0, "(8) the closed entry is not served late");
        Require(!UnchangedSinceCollected(label, 4, hit->generation), "(8) a store after the close is hidden by a racing collect");
    }
    walker.stop.store(true, std::memory_order_relaxed);
    walker.thread.join();
    recorder.Sync();
    Require(!Recorder::LookupLabel(label, 4, 5).has_value(), "(8) the entry outlived its batch");
#ifdef _WIN32
    VirtualFree(block, bytes, MEM_DECOMMIT);
#endif
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
}

// A resource build over host-imported guest memory notes its in-place reads when its writes are
// marked, so a CPU copy into that memory is refused until the batch ran.
// Per-batch store runs (Recorder::RecordStore): the stores of a batch queue until Submit, a
// later writer or reader of a queued store's bytes has the run recorded in place first, and the
// stores land in the import's memory once the batch ran. With APS5_LABEL_RUNS_INLINE=1 or
// APS5_NO_LABEL_RUNS=1 (run the binary with either set) nothing queues and the stores land alike.
void storeRunTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: store runs not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the store test block");
    std::memset(block, 0, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr) {
        std::cout << "host import of the store test block refused: store runs not tested\n";
        return;
    }
    const bool perBatch = std::getenv("APS5_LABEL_RUNS_INLINE") == nullptr && std::getenv("APS5_NO_LABEL_RUNS") == nullptr && std::getenv("APS5_NO_LABEL_BATCHING") == nullptr;
    auto* words = static_cast<volatile std::uint32_t*>(block);
    const auto store = [&](std::size_t word, std::uint32_t value) {
        const std::array<std::byte, 4> value4{std::byte{static_cast<unsigned char>(value)}, std::byte{static_cast<unsigned char>(value >> 8u)}, std::byte{static_cast<unsigned char>(value >> 16u)}, std::byte{static_cast<unsigned char>(value >> 24u)}};
        recorder.RecordStore(import->buffer, address + word * 4 - import->base, value4, address + word * 4);
    };
    Require(recorder.Idle() && !recorder.HasQueuedStores(), "a fresh recorder has queued stores");
    store(0, 1);
    store(1, 2);
    store(4, 3);
    store(4, 4);
    Require(recorder.Recording(), "a store did not open a batch");
    Require(recorder.HasQueuedStores() == perBatch, "per-batch runs do not queue (or inline runs do)");
    Require(recorder.QueuedStoreOverlaps(address, 8) == perBatch && recorder.QueuedStoreOverlaps(address + 16, 4) == perBatch && !recorder.QueuedStoreOverlaps(address + 8, 8), "queued store ranges are wrong");
    const auto before = Recorder::StoreCounts();
    // A reader or writer of untouched bytes forces nothing; one over a queued store records the run.
    recorder.FlushStoresOverlapping(address + 8, 8);
    Require(recorder.HasQueuedStores() == perBatch, "a disjoint range flushed the run");
    recorder.FlushStoresOverlapping(address + 16, 4);
    Require(!recorder.HasQueuedStores() && Recorder::StoreCounts().runsForced == before.runsForced + (perBatch ? 1 : 0), "an overlapping writer did not record the run in place");
    store(2, 5);
    Require(recorder.HasQueuedStores() == perBatch, "a store after a forced run did not start a new run");
    recorder.Submit();
    Require(!recorder.HasQueuedStores() && Recorder::StoreCounts().runsAtSubmit == before.runsAtSubmit + (perBatch ? 1 : 0), "Submit did not record the run");
    recorder.Sync();
    Require(words[0] == 1 && words[1] == 2 && words[2] == 5 && words[4] == 4 && words[3] == 0, "the stores did not land (or the later store of a dword lost)");
    const auto after = Recorder::StoreCounts();
    Require(after.stores == before.stores + 1 && after.replaced == before.replaced + 0 && after.joined == before.joined + 0, "store counters are off");
    // The covered-access mask: reported once by the next Commands() call, then cleared.
    VkAccessFlags covered = 0xffffffffu;
    recorder.Commands(&covered);
    Require(covered == 0, "a new batch starts covered");
    recorder.MarkCovered(VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
    recorder.Commands(&covered);
    Require(covered == (VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT), "the covered mask is not reported");
    recorder.Commands(&covered);
    Require(covered == 0, "the covered mask survived a Commands() call");
    recorder.Sync();
    // The hazard tracker (counting mode only): accesses are noted without effect on the batch.
    const std::pair<std::uint64_t, std::uint64_t> range{address, address + 64};
    recorder.NoteAccess(Recorder::CommandClass::Fill, Recorder::Access{{}, std::span(&range, 1), {}, VK_PIPELINE_STAGE_TRANSFER_BIT});
    recorder.NoteAccess(Recorder::CommandClass::DispatchLeading, Recorder::Access{std::span(&range, 1), {}, {}, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT});
    Require(recorder.Recording() == Recorder::BarrierValidate(), "a noted access opened a batch with the tracker off (or none with it on)");
    recorder.Sync();
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, bytes);
}

void resourceReadTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: resource read notes not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the test block");
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the test block refused: resource read notes not tested\n";
        return;
    }
    const auto element = address + 4096;
    constexpr std::size_t elementBytes = 1024;
    {
        GuestBufferMemory memory(context);
        memory.AddReadable(element, elementBytes);
        memory.Upload(false);
        const auto reads = memory.InPlaceReads();
        Require(reads.size() == 1 && reads[0].first == element && reads[0].second == element + elementBytes, "an imported region is not an in-place read");
        Require(memory.Writes().empty(), "a readable element counts as written");
    }
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {static_cast<std::uint32_t>(element), static_cast<std::uint32_t>(element >> 32u) & 0xffffu, elementBytes, 0x31000000u};
    binding.bufferWritten = {false};
    program.bindings.push_back(binding);
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        ShaderResources resources(context, compute);
        Require(recorder.Idle() && !recorder.PendingReadOverlaps(element, 16), "the build itself noted a read");
        resources.MarkGpuWrites(recorder);
        Require(recorder.PendingReadOverlaps(element, 16) && recorder.PendingReadOverlaps(element + elementBytes - 4, 4) && !recorder.PendingReadOverlaps(element + elementBytes, 16), "the build's in-place read was not noted");
        Require(!recorder.PendingWriteOverlaps(element, elementBytes), "a read-only element was noted as written");
        const auto info = recorder.DescribePendingRead(element, 16);
        Require(info.has_value() && info->kind == Kind::DispatchElement && info->open, "the build's read has the wrong kind");
        recorder.Submit();
        device.WaitQueue();
        Require(!recorder.PendingReadOverlaps(element, 16), "the build's read refuses after its batch ran");
        recorder.Sync();
    }
    std::weak_ptr<ShaderResources::DrawBindings> snapshotLifetime;
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        auto first = resources.PrepareDrawBindings(snapshotRecorder);
        Require(first != nullptr && first->snapshots.size() == 1, "read-only draw input was not snapshotted");
        std::memset(reinterpret_cast<void*>(element), 0x22, elementBytes);
        auto second = resources.PrepareDrawBindings(snapshotRecorder);
        Require(second != nullptr && second->snapshots.size() == 1, "cached draw input was not snapshotted again");
        Require(first->allocation.set != second->allocation.set, "in-flight draws share mutable descriptor bindings");
        std::memset(reinterpret_cast<void*>(element), 0x33, elementBytes);
        auto downloaded = std::make_shared<Buffer>(snapshotContext, elementBytes * 2, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = snapshotRecorder.Commands();
        RecordMemoryBarrier(snapshotContext, commands, VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        const auto copy = snapshotContext.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
        VkBufferCopy region{0, 0, elementBytes};
        copy(commands, first->snapshots[0].buffer->Handle(), downloaded->Handle(), 1, &region);
        region.dstOffset = elementBytes;
        copy(commands, second->snapshots[0].buffer->Handle(), downloaded->Handle(), 1, &region);
        RecordMemoryBarrier(snapshotContext, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        snapshotLifetime = first;
        first.reset();
        second.reset();
        Require(!snapshotLifetime.expired(), "draw snapshot was released before its GPU batch");
        snapshotRecorder.Sync();
        const auto contents = downloaded->Bytes();
        Require(std::all_of(contents.begin(), contents.begin() + elementBytes, [](std::byte value) { return value == std::byte{0x11}; }), "first draw observed overwritten input");
        Require(std::all_of(contents.begin() + elementBytes, contents.end(), [](std::byte value) { return value == std::byte{0x22}; }), "second draw observed overwritten input");
        snapshotRecorder.NotePendingWrite(element, elementBytes);
        Require(resources.PrepareDrawBindings(snapshotRecorder) == nullptr, "GPU-produced input was replaced with stale CPU memory");
        snapshotRecorder.Sync();
    }
    Require(snapshotLifetime.expired(), "draw snapshots outlived their recorder");
    recorder.Activate();
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    // The import is retired by the next reconcile; the block itself is left to the process.
    HostImportFor(context, address, bytes);
}

// Unit shadows (UnitShadow.hpp) over a host import of write-watched arena memory: a retile piece's
// slab destination and its seeds, freshness from the tracker (a publish never stamps, a CPU write
// makes the unit stale), the scopes, the slab boundary, and the retire publish. With
// APS5_UNIT_SHADOW_MIB=8 (one slab) the second slab's allocation evicts the first with a publish;
// with APS5_NO_UNIT_SHADOW=1 every primitive is inert.
void unitShadowTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
    if (!UnitShadowEnabled()) {
        Require(!AnyShadowedOverlaps(0x10000, 16) && PublishShadow(0x10000, 16, PublishScope::Whole, PublishReason::Hook) == 0, "unit shadows are off but not inert");
        std::cout << "unit shadows off (APS5_NO_UNIT_SHADOW): primitives inert\n";
        return;
    }
    if (context.hostImportAlignment == 0 || !GuestArena::GuestArenaAvailable_nid_postfix() || !GuestArena::GuestArenaWriteWatched_nid_postfix()) {
        std::cout << "host imports or the write-watched arena unavailable: unit shadows not tested\n";
        return;
    }
    constexpr std::uint64_t unit = 65536;
    // Three slabs at the default 8 MiB: units 0..127, 128..255, 256..271.
    constexpr std::size_t bytes = (17u << 20u);
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, 65536);
    Require(block != nullptr, "cannot allocate the shadow test block");
#ifdef _WIN32
    Require(VirtualAlloc(block, bytes, MEM_COMMIT, PAGE_READWRITE) != nullptr, "cannot commit the shadow test block");
#endif
    struct Release {
        void* block;
        ~Release() {
#ifdef _WIN32
            VirtualFree(block, bytes, MEM_DECOMMIT);
#endif
            GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
        }
    } release{block};
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        void* block;
        bool armed = true;
        ~Unregister() {
            if (!armed) return;
            GuestAllocations::Mutation mutation;
            mutation.Remove(block);
        }
    } unregister{block};
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr) {
        std::cout << "host import of the shadow test block refused: unit shadows not tested\n";
        return;
    }
    Require(import->base == address && import->bytes == bytes, "the import does not cover the block");
    CollectWritesUncached(address, bytes);
    const auto* words = static_cast<const std::uint8_t*>(block);
    const auto unit3 = address + 3 * unit;
    const auto unit4 = unit3 + unit;
    const auto unit5 = unit4 + unit;
    // A half-unit piece seeds its unit from the import; a whole-unit piece does not; both land in
    // one slab at one offset.
    auto half = ShadowDestinationFor(context, *import, unit3, unit3 + unit / 2);
    Require(half.has_value() && half->seedUnits.size() == 1 && half->seedUnits[0].first == unit3 && half->seedUnits[0].second == unit4, "a half-unit retile piece does not seed its unit");
    auto whole = ShadowDestinationFor(context, *import, unit3, unit4);
    Require(whole.has_value() && whole->seedUnits.empty() && whole->slab == half->slab && whole->offset == half->offset && whole->buffer == half->buffer, "a whole-unit retile piece seeds, or lands elsewhere");
    Require(!AnyShadowedOverlaps(unit3, unit), "a destination alone counts as shadowed");
    const auto firstSlab = half->slab;
    Require(firstSlab->pins.load() == 2 && half->pin != nullptr, "destinations do not pin their slab");
    // The retile as writeBackWindows records it: the seed, then the piece's bytes over its half.
    const auto copyBuffer = context.Function<PFN_vkCmdCopyBuffer>("vkCmdCopyBuffer");
    const auto retile = [&](const ShadowDestination& destination, std::uint64_t begin, std::uint64_t length, std::uint8_t value, bool seed) {
        auto pattern = std::make_shared<Buffer>(context, static_cast<std::size_t>(length), VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
        std::memset(pattern->Bytes().data(), value, pattern->Bytes().size());
        const auto commands = recorder.Commands();
        recorder.Keep(pattern);
        recorder.Keep(destination.slab);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_HOST_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        if (seed) {
            const VkBufferCopy seedCopy{begin - import->base, SlabOffset(*import, *destination.slab, begin), unit};
            copyBuffer(commands, import->buffer, destination.buffer, 1, &seedCopy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        }
        const VkBufferCopy piece{0, SlabOffset(*import, *destination.slab, begin), length};
        copyBuffer(commands, pattern->Handle(), destination.buffer, 1, &piece);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        recorder.MarkCovered(VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        const ShadowedRange range{begin & ~(unit - 1), (begin & ~(unit - 1)) + unit, destination.slab};
        MarkShadowed(*import, std::span(&range, 1), TrackerGeneration());
    };
    retile(*half, unit3, unit / 2, 0x22, true);
    Require(AnyShadowedOverlaps(unit3, 1) && AnyShadowedOverlaps(unit4 - 1, 1) && AnyShadowedOverlaps(unit3 + 1000, 64), "a fresh unit is not seen as shadowed");
    Require(!AnyShadowedOverlaps(unit3 - 1, 1) && !AnyShadowedOverlaps(unit4, unit) && !AnyShadowedOverlaps(address, 3 * unit), "a range outside the unit is seen as shadowed");
    {
        // An upload's runs over units 2..4 split at unit 3's freshness; the fresh piece reads the slab.
        const std::pair<std::uint64_t, std::uint64_t> run{2 * unit, 5 * unit};
        const auto sources = ShadowSources(context, *import, address, std::span(&run, 1), {});
        Require(sources.size() == 3, "an upload run is not split at the fresh unit");
        Require(!sources[0].shadow && sources[0].begin == 2 * unit && sources[0].end == 3 * unit && sources[0].buffer == import->buffer && sources[0].offset == 2 * unit, "the import piece before the fresh unit is wrong");
        Require(sources[1].shadow && sources[1].begin == 3 * unit && sources[1].end == 4 * unit && sources[1].buffer == half->buffer && sources[1].offset == half->offset && sources[1].slab == half->slab, "the fresh unit's piece does not read the slab");
        Require(!sources[2].shadow && sources[2].begin == 4 * unit && sources[2].end == 5 * unit, "the import piece after the fresh unit is wrong");
    }
    // A whole publish: the untouched half keeps the seed (the import's bytes), the other the
    // retile's; the copy stamps nothing, and a second publish copies nothing.
    const auto pre = CollectWritesUncached(address, bytes);
    Require(PublishShadow(unit3, unit, PublishScope::Whole, PublishReason::Hook) == 1, "the whole publish did not copy the unit");
    Require(recorder.PendingWriteOverlaps(unit3, unit), "the publish noted no pending write");
    recorder.Sync();
    Require(words[3 * unit] == 0x22 && words[3 * unit + unit / 2 - 1] == 0x22 && words[3 * unit + unit / 2] == 0x11 && words[4 * unit - 1] == 0x11 && words[3 * unit - 1] == 0x11 && words[4 * unit] == 0x11, "the published bytes are wrong");
    Require(!AnyShadowedOverlaps(unit3, unit), "a published unit still counts as shadowed");
    Require(PublishShadow(unit3, unit, PublishScope::Whole, PublishReason::Hook) == 0, "a second publish copied the unit again");
    Require(UnchangedSince(unit3, unit, pre), "the publish stamped the unit");
    {
        // The published unit still reads from the slab (both hold the bytes).
        const std::pair<std::uint64_t, std::uint64_t> run{3 * unit, 4 * unit};
        const auto sources = ShadowSources(context, *import, address, std::span(&run, 1), {});
        Require(sources.size() == 1 && sources[0].shadow, "a published unit does not read from the slab");
    }
    // Partial-unit scope over unit 3 (partly, fresh again) and unit 4 (wholly, fresh): unit 3 only.
    retile(*whole, unit3, unit, 0x33, false);
    auto four = ShadowDestinationFor(context, *import, unit4, unit5);
    Require(four.has_value() && four->seedUnits.empty() && four->slab == half->slab, "unit 4's destination is not in the same slab");
    retile(*four, unit4, unit, 0x44, false);
    Require(AnyShadowedOverlaps(unit3, unit) && AnyShadowedOverlaps(unit4, unit), "re-marked units are not shadowed");
    Require(PublishShadow(unit3 + 100, static_cast<std::size_t>(2 * unit - 100), PublishScope::PartialUnits, PublishReason::Fill) == 1, "the partial publish did not copy exactly the partly covered unit");
    Require(!AnyShadowedOverlaps(unit3, unit) && AnyShadowedOverlaps(unit4, unit), "the partial publish copied the wrong unit");
    recorder.Sync();
    Require(words[3 * unit] == 0x33 && words[4 * unit - 1] == 0x33 && words[4 * unit] == 0x11, "the partial publish's bytes are wrong");
    // A CPU write into a page of unit 4 makes it stale: the publish collects the unit itself (no
    // walk saw the store before it), copies nothing and drops it.
    static_cast<void>(CollectWritesUncached(unit4, unit));
    *static_cast<volatile std::uint8_t*>(static_cast<void*>(static_cast<std::uint8_t*>(block) + 4 * unit + 4096)) = 0x55;
    Require(PublishShadow(unit4, unit, PublishScope::Whole, PublishReason::Hook) == 0, "a stale unit was published");
    Require(!AnyShadowedOverlaps(unit4, unit), "a stale unit still counts as shadowed after its publish");
    recorder.Sync();
    Require(words[4 * unit + 4096] == 0x55 && words[4 * unit] == 0x11, "a stale unit's publish overwrote the CPU's bytes");
    // The slab boundary: a piece over units 127 and 128 is refused, each alone lands in its slab.
    const auto unit127 = address + 127 * unit;
    const auto unit128 = unit127 + unit;
    Require(SlabBoundary(*import, unit127) == unit128 && SlabBoundary(*import, address) == unit128 && SlabBoundary(*import, unit128) == unit128 + 128 * unit, "the slab boundary is wrong");
    Require(!ShadowDestinationFor(context, *import, unit127, unit128 + unit).has_value(), "a piece across a slab boundary was accepted");
    auto low = ShadowDestinationFor(context, *import, unit127, unit128);
    Require(low.has_value() && low->slab == firstSlab && low->offset == 127 * unit, "unit 127 does not land in the first slab");
    retile(*low, unit127, unit, 0x66, false);
    auto high = ShadowDestinationFor(context, *import, unit128, unit128 + unit);
    const char* budgetText = std::getenv("APS5_UNIT_SHADOW_MIB");
    const auto budgetMiB = budgetText != nullptr ? std::strtoull(budgetText, nullptr, 10) : 1024ull;
    if (budgetMiB < 16) {
        // One slab fits: while destinations pin the first slab the second is refused (a
        // write-back in progress must keep its slab); with the pins released, the second slab's
        // allocation evicts the first after publishing its fresh unit 127 (the eviction publish is
        // recorded, so the bytes land at the sync).
        Require(!high.has_value(), "a pinned slab was evicted for a second slab");
        half.reset();
        whole.reset();
        four.reset();
        low.reset();
        Require(firstSlab->pins.load() == 0, "a released destination left its pin");
        high = ShadowDestinationFor(context, *import, unit128, unit128 + unit);
        Require(high.has_value() && high->slab != firstSlab && high->offset == 0, "unit 128 does not land in a second slab under the one-slab budget");
        recorder.Sync();
        Require(words[127 * unit] == 0x66 && !AnyShadowedOverlaps(unit127, unit), "the evicted slab's fresh unit was not published");
        std::cout << "unit shadow eviction under APS5_UNIT_SHADOW_MIB=" << budgetMiB << " verified\n";
    } else {
        Require(high.has_value() && high->slab != firstSlab && high->offset == 0 && AnyShadowedOverlaps(unit127, unit), "unit 128 does not land in a second slab beside the first");
        half.reset();
        whole.reset();
        four.reset();
        low.reset();
    }
    retile(*high, unit128, unit, 0x77, false);
    Require(AnyShadowedOverlaps(unit128, unit), "the second slab's unit is not shadowed");
    high.reset();
    // Retire through the registry: the block is re-registered as its first five units, so the next
    // lookup reconciles and retires the import; the fresh units still inside a registered range
    // are published into its (kept) buffer, the rest (memory the title took back) are dropped.
    // Under the one-slab budget unit 5's slab evicts the second one first (unit 128 published).
    retile(*ShadowDestinationFor(context, *import, unit5, unit5 + unit), unit5, unit, 0x88, false);
    retile(*ShadowDestinationFor(context, *import, address + 2 * unit, unit3), address + 2 * unit, unit, 0x99, false);
    Require(AnyShadowedOverlaps(unit5, unit) && AnyShadowedOverlaps(address + 2 * unit, unit), "units 2 and 5 are not shadowed before the retire");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        mutation.Add(block, static_cast<std::size_t>(5 * unit), true, true);
    }
    Require(HostImportFor(context, address, bytes) == nullptr, "a shrunk registration still imports the old range");
    Require(!AnyShadowedOverlaps(address, bytes), "the retired import's shadow survived");
    recorder.Sync();
    Require(words[2 * unit] == 0x99 && words[3 * unit - 1] == 0x99, "the retire did not publish the unit still registered");
    Require(words[5 * unit] == 0x11 && words[5 * unit + unit - 1] == 0x11, "the retire published a unit whose memory is no longer registered");
    if (budgetMiB >= 16) Require(words[127 * unit] == 0x11 && words[128 * unit] == 0x11, "the retire published the second slab's units outside the registration");
    else Require(words[128 * unit] == 0x77, "the eviction before the retire did not publish unit 128");
}

// The data word positions of a dispatch-cache variant (Driver.cpp's data-only hits): leaves
// located among the runs' words, aliased, unaligned, out-of-run and mismatched ones skipped.
void dataWordPositionsTests() {
    using AgcDriver::DataWordPositions;
    const std::vector<std::pair<std::uint64_t, std::uint64_t>> runs{{0x1000, 0x1010}, {0x2000, 0x2008}};
    const std::vector<std::uint32_t> words{1, 2, 3, 4, 5, 6};
    const std::vector<std::uint32_t> flattened{2, 6, 3};
    std::vector<std::uint32_t> positions, slots;
    {
        const std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves{{1, 0x2004}, {0, 0x1004}, {2, 0x1008}};
        const std::vector<std::uint64_t> otherReads{0x1008};
        const auto counts = DataWordPositions(runs, leaves, otherReads, words, flattened, positions, slots);
        Require(positions == std::vector<std::uint32_t>{1, 5} && slots == std::vector<std::uint32_t>{0, 1}, "data word positions are wrong");
        Require(counts.aliased == 1 && counts.unmapped == 0 && counts.mismatched == 0, "an aliased leaf was not counted");
    }
    {
        const std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves{{0, 0x1006}, {1, 0x3000}, {1, 0x2008}, {2, 0x1000}};
        const auto counts = DataWordPositions(runs, leaves, {}, words, flattened, positions, slots);
        Require(positions.empty() && slots.empty(), "a skipped leaf produced a position");
        Require(counts.unmapped == 3 && counts.mismatched == 1 && counts.aliased == 0, "skipped leaves were counted wrongly");
    }
    {
        // Two pure slots over one dword: both positions kept, in position order.
        const std::vector<std::pair<std::uint32_t, std::uint64_t>> leaves{{2, 0x1008}, {1, 0x2004}, {0, 0x1004}};
        static_cast<void>(DataWordPositions(runs, leaves, {}, words, flattened, positions, slots));
        Require(positions == std::vector<std::uint32_t>{1, 2, 5} && slots == std::vector<std::uint32_t>{0, 2, 1}, "positions are not sorted");
    }
}

// A template's data buffers refreshed by words from a patched compiled result (a data-only hit)
// and back: DataWordsHash() follows the buffers exactly, so a later recipe hit's hash compare
// (RecordedDispatch::DataRefresh::Hash) decides correctly in both directions.
void dataRefreshTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::FlattenedSrt;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 1;
    binding.guestDescriptor = {1, 2, 3, 4};
    program.bindings.push_back(binding);
    auto patched = program;
    patched.bindings[0].guestDescriptor[2] = 0x33;
    const CompiledShader original{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    const CompiledShader live{ShaderRecompiler::ShaderStage::Compute, &patched, 0};
    ShaderResources resources(context, original);
    Require(resources.DataWordsHash() == ShaderResources::DataWordsHash(original) && resources.DataWordsHash() != ShaderResources::DataWordsHash(live), "a fresh template's data hash is not its words'");
    Require(!resources.DataWordsDiffer(original) && resources.DataWordsDiffer(live), "the per-word compare disagrees with the words");
    Require(resources.RefreshData(recorder.Commands(), live, &recorder), "a refresh with different words recorded nothing");
    Require(resources.DataWordsHash() == ShaderResources::DataWordsHash(live) && !resources.DataWordsDiffer(live), "the refreshed template's hash is not the patched words'");
    Require(resources.RefreshData(recorder.Commands(), original, &recorder), "the refresh back recorded nothing");
    Require(resources.DataWordsHash() == ShaderResources::DataWordsHash(original), "the refreshed template's hash is not the original words'");
    Require(!resources.RefreshData(recorder.Commands(), original, &recorder), "a refresh with equal words recorded");
    recorder.Submit();
    device.WaitQueue();
    recorder.Sync();
}

}

int main() {
    try {
        Device device;
        std::lock_guard gpu(GpuMutex());
        Recorder recorder(device.GetContext());
        recorder.Activate();
        readTrackingTests(device, recorder);
        writeSettledTests(device, recorder);
        completionCountTests(device, recorder);
        labelTests(recorder);
        lateLabelTests(recorder);
        unchangedSinceTests();
        closeRaceTests(device, recorder);
        keyProofTests(device, recorder);
        resourceReadTests(device, recorder);
        storeRunTests(device, recorder);
        unitShadowTests(device, recorder);
        dataWordPositionsTests();
        dataRefreshTests(device, recorder);
        std::cout << "Recorder read tracking and label tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
