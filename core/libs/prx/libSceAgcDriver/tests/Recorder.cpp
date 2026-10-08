#include "prx/libSceAgcDriver/Graphics/include/Recorder.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Draw.hpp"
#include "prx/libSceAgcDriver/Graphics/include/DccMetadata.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestBufferMemory.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Resources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Shaders.hpp"
#include "prx/libSceAgcDriver/Graphics/include/Texture.hpp"
#include "prx/libSceAgcDriver/Graphics/include/TextureDetiler.hpp"
#include "prx/libSceAgcDriver/Graphics/include/UnitShadow.hpp"
#include "prx/libSceAgcDriver/Execution/include/BdaFeatures.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderMemory.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/GuestWriteWatch.hpp"
#include "ResidentPresent.hpp"
#include "SampleLod_spv.h"
#include <SDL_loadso.h>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/mman.h>
#endif
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using namespace AgcDriver::Graphics;
using AgcDriver::GuestMemory::GpuMutex;

void* AllocateWatched(std::size_t bytes, std::size_t alignment) {
    if (!AgcDriver::GuestMemory::WriteWatched()) return nullptr;
#ifdef _WIN32
    void* block = GuestArena::GuestArenaAllocate_nid_postfix(bytes, alignment);
    GuestArena::GuestArenaCommit_nid_postfix(block, bytes, PAGE_READWRITE, bytes);
#else
    void* raw = mmap(nullptr, bytes + alignment, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (raw == MAP_FAILED) throw std::runtime_error("cannot map the watched block");
    const auto begin = reinterpret_cast<std::uintptr_t>(raw);
    const auto aligned = (begin + alignment - 1) & ~(static_cast<std::uintptr_t>(alignment) - 1);
    if (aligned != begin) munmap(raw, aligned - begin);
    if (aligned + bytes != begin + bytes + alignment) munmap(reinterpret_cast<void*>(aligned + bytes), begin + alignment - aligned);
    void* block = reinterpret_cast<void*>(aligned);
    GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
#endif
    if (!AgcDriver::GuestMemory::Watched(reinterpret_cast<std::uint64_t>(block), bytes)) throw std::runtime_error("the watched block is not watched");
    return block;
}

void ReleaseWatched(void* block, std::size_t bytes) {
#ifdef _WIN32
    GuestArena::GuestArenaReset_nid_postfix(block, bytes);
    GuestArena::GuestArenaRelease_nid_postfix(block, bytes);
#else
    munmap(block, bytes);
    GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
#endif
}

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
            const auto rankDeviceType = [](VkPhysicalDeviceType type) {
                switch (type) {
                    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU: return 3;
                    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU: return 2;
                    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU: return 1;
                    default: return 0;
                }
            };
            const auto physicalProperties = function<PFN_vkGetPhysicalDeviceProperties>("vkGetPhysicalDeviceProperties");
            int selectedRank = -1;
            for (const auto physical : devices) {
                VkPhysicalDeviceProperties candidate{};
                physicalProperties(physical, &candidate);
                if (candidate.apiVersion < VK_API_VERSION_1_1) continue;
                const int rank = rankDeviceType(candidate.deviceType);
                if (rank <= selectedRank) continue;
                context.physical = physical;
                selectedRank = rank;
            }
            Require(context.physical != VK_NULL_HANDLE, "no Vulkan 1.1 device");
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
            if (hasExtension(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME) && hasExtension(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME)) {
                extensionsEnabled.push_back(VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME);
                extensionsEnabled.push_back(VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME);
                context.dmaBufImport = true;
            }
            VkPhysicalDeviceImageViewMinLodFeaturesEXT minLod{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT};
            if (hasExtension(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME)) {
                VkPhysicalDeviceFeatures2 features{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, &minLod};
                function<PFN_vkGetPhysicalDeviceFeatures2>("vkGetPhysicalDeviceFeatures2")(context.physical, &features);
                context.imageViewMinLod = minLod.minLod == VK_TRUE;
            }
            minLod = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_IMAGE_VIEW_MIN_LOD_FEATURES_EXT};
            minLod.minLod = VK_TRUE;
            if (context.imageViewMinLod) {
                extensionsEnabled.push_back(VK_EXT_IMAGE_VIEW_MIN_LOD_EXTENSION_NAME);
                minLod.pNext = address.pNext;
                address.pNext = &minLod;
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
            context.formatProperties = function<PFN_vkGetPhysicalDeviceFormatProperties>("vkGetPhysicalDeviceFormatProperties");
            context.Function<PFN_vkGetDeviceQueue>("vkGetDeviceQueue")(context.device, family, 0, &context.queue);
            VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
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
        if (context.device != VK_NULL_HANDLE) {
            DestroyShadows(context.device);
            function<PFN_vkDestroyDevice>("vkDestroyDevice")(context.device, nullptr);
        }
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

void afterRecordedWorkTests(const Device& device, Recorder& recorder) {
    alignas(64) static std::uint32_t memory[16];
    const auto base = reinterpret_cast<std::uint64_t>(memory);
    const std::array<std::byte, 4> value{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    recorder.Sync();
    std::vector<int> ran;
    std::uint32_t seen = 0;
    Require(recorder.Idle() && !recorder.AfterRecordedWork([&] { ran.push_back(0); }) && ran.empty() && Recorder::PendingCompletionLabels() == 0, "an idle recorder kept an action for recorded work");
    recorder.NotePendingWrite(0x52000, 0x100);
    Require(!Recorder::PendingLabelSince().has_value(), "a write started the label flush deadline");
    Require(recorder.AfterRecordedWork([&] { ran.push_back(1); }) && Recorder::PendingCompletionLabels() == 1 && Recorder::PendingLabelSince().has_value(), "an action behind the open batch is not pending under the label flush deadline");
    recorder.AfterCompletions(base, value, 6, 0, false);
    Require(recorder.AfterRecordedWork([&] { seen = memory[0]; ran.push_back(2); }) && Recorder::PendingCompletionLabels() == 3, "an action behind a completion label is not pending");
    recorder.Submit();
    Require(recorder.AfterRecordedWork([&] { ran.push_back(3); }) && Recorder::PendingCompletionLabels() == 4, "an action behind an in-flight batch is not pending");
    device.WaitQueue();
    Require(ran.empty(), "an action ran before its batch was reaped");
    recorder.Sync();
    Require(ran == std::vector<int>{1, 2, 3} && Recorder::PendingCompletionLabels() == 0 && recorder.Idle(), "actions behind recorded work did not run once each, in order");
    Require(seen == 1, "an action ran before the completion label recorded ahead of it");
}

void batchStampTests(Recorder& recorder) {
    Require(recorder.Idle(), "batch stamps: the recorder is busy");
    double previousEnd = 0;
    for (int round = 0; round < 3; ++round) {
        recorder.NotePendingWrite(0x60000, 0x100);
        const auto serial = recorder.Submissions() + 1;
        recorder.Submit();
        recorder.Sync();
        std::size_t missing = 0;
        const auto batches = recorder.CompletedBatches(serial - 1, serial, missing);
        Require(missing == 0 && batches.size() == 1 && batches.front().serial == serial, "a finished batch has no completion record");
        const auto& batch = batches.front();
        if (Recorder::BatchStampsEnabled()) {
            Require(batch.gpuStartNs > 0 && batch.gpuEndNs >= batch.gpuStartNs && batch.gpuStartNs >= previousEnd, "batch stamps are missing or out of order");
            previousEnd = batch.gpuEndNs;
        } else {
            Require(batch.gpuStartNs == 0 && batch.gpuEndNs == 0, "batch stamps were written with profiling off");
        }
    }
    std::cout << "batch stamps " << (Recorder::BatchStampsEnabled() ? "checked" : "off") << '\n';
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

void largeLabelTests(Recorder& recorder) {
    constexpr std::uint64_t slot = 0x71000, small = slot + 0x10, outside = slot - 4, wide = 0x72000;
    const std::array<std::byte, 4> one{std::byte{1}, std::byte{0}, std::byte{0}, std::byte{0}};
    std::array<std::byte, 256> clear{};
    std::array<std::byte, Recorder::LabelTableBytes> limit{};
    limit.fill(std::byte{3});
    recorder.Sync();
    Require(recorder.PendingLabels() == 0 && !Recorder::PendingLabelSince().has_value() && !Recorder::WideLabelIn(0, ~0ull), "the table is not empty before the large label tests");
    recorder.NoteLabel(small, one, 100, 0);
    recorder.NoteLabel(outside, one, 100, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(recorder.PendingLabels() == 2 && Recorder::LookupLabel(small, 4, 50).has_value(), "4-byte labels did not enter the table");
    recorder.NoteLabel(slot, clear, 101, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(recorder.PendingLabels() == 1 && Recorder::LookupLabel(outside, 4, 50).has_value(), "a label larger than the table bound entered dwords or removed one outside its range");
    Require(!Recorder::LookupLabel(small, 4, 50).has_value() && !Recorder::LookupLabel(slot, 8, 0).has_value(), "an older label under a larger one still composes the value");
    Require(Recorder::WideLabelIn(slot, 4) && Recorder::WideLabelIn(slot + clear.size() - 4, 4) && Recorder::WideLabelIn(slot - 0x100, 0x104) && !Recorder::WideLabelIn(slot - 0x100, 0x100) && !Recorder::WideLabelIn(slot + clear.size(), 4), "a large label's range is wrong");
    Require(recorder.PendingLabelIn(slot + 0x20, 0x20) && recorder.PendingWriteOverlaps(slot, clear.size()), "a large label is not pending");
    Require(Recorder::PendingLabelSince().has_value(), "a large label did not start the label flush deadline");
    recorder.NoteLabel(small, one, 102, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    const auto later = Recorder::LookupLabel(small, 4, 101);
    Require(later.has_value() && later->value == 1 && later->stamp == 102, "a label after a large one is not served");
    recorder.NoteLabel(wide, limit, 103, 0);
    Recorder::CloseLabelGroup(AgcDriver::GuestMemory::TrackerGeneration());
    Require(recorder.PendingLabels() == 2 + Recorder::LabelTableBytes / 4 && !Recorder::WideLabelIn(wide, limit.size()), "a label at the table bound did not enter every dword");
    const auto bound = Recorder::LookupLabel(wide + Recorder::LabelTableBytes - 8, 8, 0);
    Require(bound.has_value() && bound->value == 0x0303030303030303ull && bound->stamp == 103, "a label at the table bound is not served");
    recorder.Sync();
    Require(recorder.PendingLabels() == 0 && !Recorder::WideLabelIn(0, ~0ull) && !recorder.PendingLabelIn(slot, clear.size()) && !recorder.PendingWriteOverlaps(slot, clear.size()), "large label tests left entries, ranges or writes behind");
}

// (6) GuestMemory::UnchangedSinceCollected: false outside the watched arena, and around a
// MarkWritten or a CPU write of the block inside it.
void unchangedSinceTests() {
    using namespace AgcDriver::GuestMemory;
    static std::uint32_t outside[16];
    Require(!UnchangedSinceCollected(reinterpret_cast<std::uint64_t>(outside), 4, TrackerGeneration()), "(6) a range outside the watched memory is unchanged");
    Require(!Watched(reinterpret_cast<std::uint64_t>(outside), 4) && CollectWrites(reinterpret_cast<std::uint64_t>(outside), 4) == 0, "(6) a range outside the watched memory is watched");
    constexpr std::size_t bytes = 65536;
    void* block = AllocateWatched(bytes, bytes);
    if (block == nullptr) {
        std::cout << "no write watching: UnchangedSinceCollected is always false\n";
        return;
    }
    auto* words = static_cast<volatile std::uint32_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    words[0] = 0;
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
    ReleaseWatched(block, bytes);
}

// (9) ProvedClearKeys: a surface's DCC keys are scanned once and answered from the proof after,
// until the key range is stamped (a GPU key store's MarkWritten) or the CPU writes it; a scan made
// while recorded work still writes the keys is not kept. With APS5_NO_KEY_FAST_PATH=1 every call
// scans and no proof is stored.
void keyProofTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t bytes = 65536;
    void* block = AllocateWatched(bytes, bytes);
    if (block == nullptr) {
        std::cout << "no write watching: key proofs not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
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
    constexpr std::size_t bytes = 1u << 20;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) return;
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
    ReleaseWatched(block, bytes);
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

void remappedImportTests(const Device& device) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: remapped imports not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the remap test block");
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the remap test block refused: remapped imports not tested\n";
        return;
    }
    const auto first = HostImportSerial(context, address, bytes, true);
    Require(first != 0 && HostImportSerial(context, address, bytes, true) == first, "an unchanged range lost its import");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
        mutation.Add(block, bytes, true, true);
    }
    Require(HostImportFor(context, address, bytes) != nullptr, "the remapped range was not imported again");
    const auto second = HostImportSerial(context, address, bytes, true);
    Require(second != 0 && second != first, "the import of a range mapped again was kept");
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    }
    HostImportFor(context, address, bytes);
}

void movedMetadataTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: moved DCC metadata not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the moved metadata block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* firstKeys = texels + surfaceBytes;
    auto* secondKeys = firstKeys + 4096;
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the moved metadata block refused: moved DCC metadata not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    Require(DescribeSurface(resource).guestBytes == surfaceBytes, "the moved metadata surface has an unexpected size");
    const auto first = address + surfaceBytes;
    const auto second = first + 4096;
    const auto withKeys = [&](std::uint64_t keys) {
        auto described = resource;
        described.dccAddress = keys;
        return described;
    };
    const auto holds = [&](const StorageTexture& image, std::array<std::uint8_t, 4> texel) {
        Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {side, side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        const auto pixels = readback.Bytes();
        for (std::size_t i = 0; i < pixels.size(); ++i) {
            if (std::to_integer<std::uint8_t>(pixels[i]) != texel[i % 4]) return false;
        }
        return true;
    };
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(firstKeys, 0xff, keyCount);
    std::memset(secondKeys, 0x00, keyCount);
    const auto original = CachedStorageSurface(context, withKeys(first));
    Require(original->Descriptor().dccAddress == first && holds(*original, {0x55, 0x55, 0x55, 0x55}), "the first image does not hold the stored texels");
    const auto moved = CachedStorageSurface(context, withKeys(second));
    Require(moved->Descriptor().dccAddress == second, "the storage image kept the keys the surface no longer names");
    Require(!StorageImageCached(context, original.get()), "the image of the old keys is still the surface's");
    Require(holds(*moved, {0, 0, 0, 0}), "a fast clear of the moved keys did not reach the image");
    Require(CachedStorageSurface(context, resource) == moved && moved->Descriptor().dccAddress == second, "a descriptor without metadata replaced the image");
    const auto back = CachedStorageSurface(context, withKeys(first));
    Require(back != moved && back->Descriptor().dccAddress == first, "the keys moving back did not remake the image");
    Require(holds(*back, {0x55, 0x55, 0x55, 0x55}), "the image remade under the first keys does not hold the stored texels");
}

void viewPastLastMipTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: views past the last mip not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t bytes = 0x60000;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the past-last-mip block");
    auto* texels = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the past-last-mip block refused: views past the last mip not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    const auto view = [&](std::uint32_t level) {
        const std::array<std::uint32_t, 8> words{
            static_cast<std::uint32_t>(address >> 8u),
            static_cast<std::uint32_t>((address >> 40u) & 0xffu) | (56u << 20u) | (((side - 1u) & 3u) << 30u),
            ((side - 1u) >> 2u) | ((side - 1u) << 14u),
            0xfacu | (level << 12u) | (level << 16u) | (0x1bu << 20u) | (9u << 28u),
            0u,
            2u << 4u,
            0u,
            0u,
        };
        return DecodeTextureResource(words);
    };
    const auto texel = [&](const StorageTexture& image, std::uint32_t level) {
        Buffer readback(context, 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, level, 0, 1};
        copy.imageExtent = {1, 1, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image.Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        std::uint32_t value = 0;
        std::memcpy(&value, readback.Bytes().data(), sizeof(value));
        return value;
    };
    std::memset(texels, 0x55, bytes);
    std::memset(texels + 0x8400, 0x66, 4);
    std::memset(texels + 0x4800, 0x77, 4);
    const auto allocatedView = view(2);
    const auto pastView = view(3);
    Require(allocatedView.mipCount == 3 && pastView.mipCount == 4 && DescribeSurface(pastView).guestBytes == bytes, "a 256x256 32 bpp SW_64KB_R_X surface viewed past MAX_MIP 2 decoded wrongly");
    const auto allocated = CachedStorageSurface(context, allocatedView);
    Require(allocated->Descriptor().mipCount == 3, "the allocated levels' storage image has an unexpected level count");
    const auto past = CachedStorageSurface(context, pastView);
    Require(past->Descriptor().mipCount == 4, "the storage image of a view past the last mip lacks the level it names");
    Require(!StorageImageCached(context, allocated.get()), "the allocated levels' image is still the surface's after a view past its last mip");
    Require(CachedStorageSurface(context, allocatedView) == past, "a view of the allocated levels took another image than the view past the last mip");
    Require(texel(*past, 2) == 0x66666666u, "level 2 was not read from its addrlib tail slot");
    Require(texel(*past, 3) == 0x77777777u, "the level past the last mip was not read from its addrlib tail slot");
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

void misalignedSnapshotTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    const auto alignment = context.limits.minStorageBufferOffsetAlignment;
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: misaligned draw snapshots not tested\n";
        return;
    }
    if (alignment < 8 || alignment > 256) {
        std::cout << "storage buffer offset alignment " << alignment << ": misaligned draw snapshots not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the misaligned snapshot block");
    auto* guest = static_cast<std::uint8_t*>(block);
    for (std::size_t at = 0; at < bytes; ++at) guest[at] = static_cast<std::uint8_t>(at * 7u + 3u);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the test block refused: misaligned draw snapshots not tested\n";
        return;
    }
    constexpr std::uint32_t outer = 4096;
    constexpr std::uint32_t offset = outer + 4;
    constexpr std::size_t outerBytes = 128;
    constexpr std::size_t elementBytes = 64;
    const auto words = [](std::uint64_t at, std::size_t size) { return std::array<std::uint32_t, 4>{static_cast<std::uint32_t>(at), static_cast<std::uint32_t>(at >> 32u) & 0xffffu, static_cast<std::uint32_t>(size), 0x31000000u}; };
    ShaderRecompiler::RecompileResult program;
    ShaderRecompiler::DescriptorBinding binding;
    binding.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
    binding.role = ShaderRecompiler::DescriptorRole::GuestBuffers;
    binding.descriptorSet = 0;
    binding.binding = 0;
    binding.count = 2;
    for (const auto word : words(address + outer, outerBytes)) binding.guestDescriptor.push_back(word);
    for (const auto word : words(address + offset, elementBytes)) binding.guestDescriptor.push_back(word);
    binding.bufferWritten = {false, false};
    program.bindings.push_back(binding);
    program.pushConstants.resize(16);
    program.memoryOffsetDword = 0;
    const CompiledShader compute{ShaderRecompiler::ShaderStage::Compute, &program, 0};
    {
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        std::array<std::byte, PipelinePushConstantBytes> push{};
        resources.PatchPushConstants(push);
        const auto adjustment = std::to_integer<std::uint32_t>(push[1]);
        Require(push[0] == std::byte{0} && adjustment == offset % alignment, "the inner view's shader offset is not its distance from the binding");
        const auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
        Require(bindings != nullptr && bindings->snapshots.size() == 2, "read-only draw inputs were not snapshotted");
        const auto outerContents = bindings->snapshots[0].buffer->Bytes();
        Require(outerContents.size() >= outerBytes && std::memcmp(outerContents.data(), guest + outer, outerBytes) == 0, "an aligned draw snapshot misses its view's bytes");
        const auto contents = bindings->snapshots[1].buffer->Bytes();
        Require(contents.size() >= adjustment + elementBytes, "a draw snapshot ends before the bytes the shader reads");
        Require(std::memcmp(contents.data() + adjustment, guest + offset, elementBytes) == 0, "the shader's offset into a draw snapshot misses the view's bytes");
        snapshotRecorder.Sync();
    }
    recorder.Activate();
}

class DescriptorUpdates {
public:
    struct Contents {
        std::map<std::pair<std::uint32_t, std::uint32_t>, VkDescriptorBufferInfo> buffers;
        std::map<std::pair<std::uint32_t, std::uint32_t>, VkDescriptorImageInfo> images;
    };
    explicit DescriptorUpdates(Context& context) : context(context), previous(context.functions),
        update(context.Resolved(&DeviceFunctions::updateDescriptorSets, "vkUpdateDescriptorSets")) {
        Require(active == nullptr, "nested descriptor update capture");
        if (previous != nullptr) functions = *previous;
        functions.updateDescriptorSets = captureUpdate;
        context.functions = &functions;
        active = this;
    }
    ~DescriptorUpdates() { context.functions = previous; active = nullptr; }
    std::vector<Contents> calls;
    std::uint32_t copies = 0;

private:
    static VKAPI_ATTR void VKAPI_CALL captureUpdate(VkDevice device, std::uint32_t count, const VkWriteDescriptorSet* writes, std::uint32_t copyCount, const VkCopyDescriptorSet* copies) {
        Contents contents;
        for (std::uint32_t i = 0; i < count; ++i) {
            const auto& write = writes[i];
            for (std::uint32_t element = 0; element < write.descriptorCount; ++element) {
                const auto key = std::pair{write.dstBinding, write.dstArrayElement + element};
                if (write.descriptorType == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) contents.buffers[key] = write.pBufferInfo[element];
                else contents.images[key] = write.pImageInfo[element];
            }
        }
        active->calls.push_back(std::move(contents));
        active->copies += copyCount;
        active->update(device, count, writes, copyCount, copies);
    }
    Context& context;
    const DeviceFunctions* previous;
    DeviceFunctions functions{};
    PFN_vkUpdateDescriptorSets update;
    static inline DescriptorUpdates* active = nullptr;
};

void drawDescriptorWriteTests(const Device& device, Recorder& recorder) {
    auto context = device.GetContext();
    DescriptorUpdates updates(context);
    {
        DescriptorCache cache(context);
        context.descriptorCache = &cache;
        Recorder snapshots(context);
        snapshots.Activate();
        ShaderRecompiler::RecompileResult program;
        ShaderRecompiler::DescriptorBinding data{};
        data.kind = ShaderRecompiler::DescriptorKind::StorageBuffer;
        data.role = ShaderRecompiler::DescriptorRole::FlattenedSrt;
        data.binding = 2;
        data.count = 1;
        data.guestDescriptor = {1, 2, 3, 4};
        program.bindings.push_back(data);
        data.binding = 7;
        program.bindings.push_back(data);
        ShaderRecompiler::DescriptorBinding sampler{};
        sampler.kind = ShaderRecompiler::DescriptorKind::Sampler;
        sampler.role = ShaderRecompiler::DescriptorRole::GuestSamplers;
        sampler.binding = 11;
        sampler.count = 2;
        sampler.guestDescriptor.resize(8);
        sampler.samplerDepthCompare = {false, false};
        program.bindings.push_back(sampler);
        const CompiledShader shader{ShaderRecompiler::ShaderStage::Compute, &program, 0};
        ShaderResources resources(context, shader);
        Require(updates.calls.size() == 1, "building resources did not write one descriptor set");
        const auto original = updates.calls.front();
        Require(original.buffers.size() == 2 && original.images.size() == 2, "descriptor test lacks mixed bindings and an image array");
        for (std::size_t allocation = 0; allocation < 2; ++allocation) {
            const ShaderResources::MovedBuffer moved{allocation, 0, 16, {5, 6, 7, 8}};
            const auto draw = resources.PrepareDrawBindings(snapshots, std::span(&moved, 1));
            Require(draw != nullptr && draw->snapshots.size() == 1, "moved shader data did not create a draw snapshot");
            Require(updates.calls.size() == allocation + 2 && updates.copies == 0, "draw descriptors copied GPU memory or needed multiple updates");
            const auto& actual = updates.calls.back();
            Require(actual.buffers.size() == original.buffers.size() && actual.images.size() == original.images.size(), "snapshot update omitted unchanged descriptors");
            for (const auto& [key, before] : original.buffers) {
                const auto after = actual.buffers.at(key);
                const auto expected = key.first == (allocation == 0 ? 2u : 7u) ? draw->snapshots.front().buffer->Handle() : before.buffer;
                Require(after.buffer == expected && after.offset == before.offset && after.range == before.range, "snapshot update replaced the wrong buffer or changed its range");
            }
            for (const auto& [key, before] : original.images) {
                const auto after = actual.images.at(key);
                Require(after.sampler == before.sampler && after.imageView == before.imageView && after.imageLayout == before.imageLayout, "snapshot update changed an image descriptor");
            }
        }
        snapshots.Sync();
    }
    recorder.Activate();
}

void drawSnapshotReuseTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    constexpr std::size_t bytes = 65536;
    void* block = context.hostImportAlignment != 0 ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "host imports or write watching unavailable: draw snapshot reuse not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    std::memset(block, 0x11, bytes);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, reinterpret_cast<std::uint64_t>(block), bytes);
        }
    } unregister{context, block};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the watched block refused: draw snapshot reuse not tested\n";
        return;
    }
    const auto element = address + 4096;
    constexpr std::size_t elementBytes = 1024;
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
        auto snapshotContext = context;
        DescriptorCache cache(snapshotContext);
        snapshotContext.descriptorCache = &cache;
        Recorder snapshotRecorder(snapshotContext);
        snapshotRecorder.Activate();
        ShaderResources resources(snapshotContext, compute);
        const auto snapshot = [&](std::byte expected) {
            const auto bindings = resources.PrepareDrawBindings(snapshotRecorder);
            Require(bindings != nullptr && bindings->snapshots.size() == 1, "read-only draw input was not snapshotted");
            const auto buffer = bindings->snapshots[0].buffer;
            const auto contents = buffer->Bytes();
            Require(contents.size() == elementBytes && std::all_of(contents.begin(), contents.end(), [&](std::byte value) { return value == expected; }), "a draw snapshot does not hold the guest bytes of its draw");
            return buffer;
        };
        const auto first = snapshot(std::byte{0x11});
        Require(snapshot(std::byte{0x11}) == first, "an unchanged draw input was copied again");
        std::memset(reinterpret_cast<void*>(element), 0x22, elementBytes);
        const auto afterCpu = snapshot(std::byte{0x22});
        Require(afterCpu != first, "a draw snapshot outlived a CPU store to its range");
        Require(snapshot(std::byte{0x22}) == afterCpu, "the recopied draw input was not kept");
        std::memset(reinterpret_cast<void*>(element), 0x33, elementBytes);
        AgcDriver::GuestMemory::MarkWritten(element, 4);
        const auto afterStore = snapshot(std::byte{0x33});
        Require(afterStore != afterCpu, "a draw snapshot outlived a driver store to its range");
        {
            GuestAllocations::Mutation mutation;
        }
        Require(snapshot(std::byte{0x33}) != afterStore, "a draw snapshot outlived a registry mutation");
        snapshotRecorder.Sync();
    }
    recorder.Activate();
}

void drawSnapshotEvictionTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    constexpr std::size_t bytes = 65536;
    void* block = WriteWatched() ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "write watching unavailable: draw snapshot eviction not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto address = reinterpret_cast<std::uint64_t>(block);
    Recorder cache(device.GetContext());
    const auto buffer = std::make_shared<Buffer>(device.GetContext(), 16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
    const auto registry = GuestAllocations::GuestAllocationsGeneration_nid_postfix();
    const auto generation = CollectWrites(address, 4096);
    Require(generation != 0, "the watched block has no generation");
    constexpr auto cap = Recorder::DrawSnapshotEntries;
    cache.KeepDrawSnapshot(address, 16, generation, registry, buffer, Recorder::SnapshotUse::Vertex);
    for (std::size_t size = 1; size <= cap; ++size) cache.KeepDrawSnapshot(address, size, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 1) == buffer, "a kept snapshot is not reusable");
    cache.KeepDrawSnapshot(address, cap + 1, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 2) == nullptr, "the least recently used snapshot survived the cap");
    Require(cache.ReusableDrawSnapshot(address, 1) == buffer && cache.ReusableDrawSnapshot(address, 3) == buffer && cache.ReusableDrawSnapshot(address, cap + 1) == buffer, "eviction dropped a more recently used snapshot");
    cache.KeepDrawSnapshot(address, cap + 2, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 4) == nullptr && cache.ReusableDrawSnapshot(address, 1) == buffer, "the second eviction did not take the next oldest");
    Require(cache.ReusableDrawSnapshot(address, 16, Recorder::SnapshotUse::Vertex) == buffer, "storage snapshots evicted a vertex snapshot");
    cache.KeepDrawSnapshot(address, 1, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 1) == buffer && cache.ReusableDrawSnapshot(address, 5) == buffer, "replacing a kept snapshot evicted another");
    cache.KeepDrawSnapshot(address + 8192, Recorder::DrawSnapshotBudget, generation, registry, buffer);
    Require(cache.ReusableDrawSnapshot(address, 1) == nullptr && cache.ReusableDrawSnapshot(address, 5) == nullptr, "the byte budget did not evict the older snapshots");
    cache.KeepDrawSnapshot(address, 16, generation, registry, buffer);
    std::memset(block, 0x5a, 16);
    CollectWrites(address, 16);
    Require(cache.ReusableDrawSnapshot(address, 16) == nullptr && cache.ReusableDrawSnapshot(address, 16) == nullptr, "a snapshot outlived a CPU store");
}

void drawInputReuseTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    using Use = Recorder::SnapshotUse;
    constexpr std::size_t bytes = 65536;
    void* block = WriteWatched() ? AllocateWatched(bytes, 65536) : nullptr;
    if (block == nullptr) {
        std::cout << "write watching unavailable: draw input reuse not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto& context = device.GetContext();
    auto* words = static_cast<std::uint32_t*>(block);
    for (std::uint32_t i = 0; i < bytes / sizeof(std::uint32_t); ++i) words[i] = i * 3;
    const auto address = reinterpret_cast<std::uint64_t>(block);
    constexpr std::size_t size = 256;
    const auto equalsGuest = [&](const DrawInputCopy& copy) {
        const auto contents = copy.buffer->Bytes();
        return contents.size() == size && std::memcmp(contents.data(), block, size) == 0;
    };
    const auto first = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(!first.reused && first.generation != 0 && equalsGuest(first), "the first draw input copy is wrong");
    KeepDrawInput(&recorder, address, first, Use::Index32, 189);
    const auto second = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(second.reused && second.buffer == first.buffer && second.derived == 189, "an unchanged draw input was copied again");
    const auto vertex = CopyDrawInput(context, &recorder, address, size, 1, Use::Vertex);
    const auto narrow = CopyDrawInput(context, &recorder, address, size, 2, Use::Index16);
    Require(!vertex.reused && !narrow.reused && equalsGuest(vertex) && equalsGuest(narrow), "a draw input reused another use's copy");
    KeepDrawInput(&recorder, address, vertex, Use::Vertex, 0);
    Require(CopyDrawInput(context, &recorder, address, size, 1, Use::Vertex).buffer == vertex.buffer && CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).buffer == first.buffer, "the uses' copies displaced each other");
    Require(!CopyDrawInput(context, nullptr, address, size, 4, Use::Index32).reused, "a draw input was reused without a recorder");
    words[5] = 0xdead;
    const auto stored = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(!stored.reused && stored.buffer != first.buffer && equalsGuest(stored), "a draw input outlived a CPU store");
    KeepDrawInput(&recorder, address, stored, Use::Index32, 0xdead);
    Require(CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).derived == 0xdead, "the new copy was not kept");
    MarkWritten(address + 128, 4);
    const auto marked = CopyDrawInput(context, &recorder, address, size, 4, Use::Index32);
    Require(!marked.reused && equalsGuest(marked), "a draw input outlived a stamped GPU store");
    KeepDrawInput(&recorder, address, marked, Use::Index32, 1);
    Require(CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).reused, "the copy after the GPU store was not kept");
    alignas(64) static std::byte other[64];
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(other, sizeof(other), true, true);
    }
    {
        GuestAllocations::Mutation mutation;
        mutation.Remove(other);
    }
    Require(!CopyDrawInput(context, &recorder, address, size, 4, Use::Index32).reused, "a draw input outlived a registry mutation");
    const auto pool = address + 8192;
    const auto whole = CopyDrawInput(context, &recorder, pool, 12288, 1, Use::Vertex);
    Require(!whole.reused && std::memcmp(whole.buffer->Bytes().data(), reinterpret_cast<const void*>(pool), 12288) == 0, "the vertex pool copy is wrong");
    KeepDrawInput(&recorder, pool, whole, Use::Vertex, 0);
    const auto prefix = CopyDrawInput(context, &recorder, pool, 4096, 1, Use::Vertex);
    Require(prefix.reused && prefix.buffer == whole.buffer, "a shorter vertex read did not reuse the longer snapshot");
    Require(!CopyDrawInput(context, &recorder, pool, 16384, 1, Use::Vertex).reused, "a longer vertex read reused a shorter snapshot");
    Require(!CopyDrawInput(context, &recorder, pool + 4, 4096, 1, Use::Vertex).reused, "a vertex read at another address reused the snapshot");
    Require(!CopyDrawInput(context, &recorder, pool, 4096, 4, Use::Index32).reused, "an index read reused a longer vertex snapshot");
    const auto shorter = CopyDrawInput(context, &recorder, pool, 8192, 4, Use::Index32);
    KeepDrawInput(&recorder, pool, shorter, Use::Index32, 3);
    Require(!CopyDrawInput(context, &recorder, pool, 4096, 4, Use::Index32).reused, "an index read reused a longer index snapshot");
    words[(8192 + 8192 + 16) / 4] = 0xbeef;
    const auto prefixAfter = CopyDrawInput(context, &recorder, pool, 4096, 1, Use::Vertex);
    Require(std::memcmp(prefixAfter.buffer->Bytes().data(), reinterpret_cast<const void*>(pool), 4096) == 0, "a shorter vertex read after a store got other bytes");
    const auto after = CopyDrawInput(context, &recorder, pool, 12288, 1, Use::Vertex);
    Require(!after.reused && std::memcmp(after.buffer->Bytes().data(), reinterpret_cast<const void*>(pool), 12288) == 0, "a vertex read over the store reused the old bytes");
    KeepDrawInput(&recorder, pool, after, Use::Vertex, 0);
    const auto small = CopyDrawInput(context, &recorder, pool, 4096, 1, Use::Vertex);
    Require(small.reused && small.buffer == after.buffer, "the new vertex snapshot does not serve shorter reads");
    recorder.Sync();
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
        std::cout << "unit shadows off (APS5_NO_UNIT_SHADOW, or no write watching): primitives inert\n";
        return;
    }
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: unit shadows not tested\n";
        return;
    }
    constexpr std::uint64_t unit = 65536;
    // Three slabs at the default 8 MiB: units 0..127, 128..255, 256..271.
    constexpr std::size_t bytes = (17u << 20u);
    void* block = AllocateWatched(bytes, 65536);
    Require(block != nullptr, "unit shadows are on without write watching");
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
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
    if (!Watched(address, bytes)) {
        std::cout << "host imports are compared, not watched: unit shadows not tested\n";
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

void storageRefreshTests(const Device& device, Recorder& recorder, bool watched) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: storage refresh not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
    void* block = nullptr;
    if (watched) {
        block = AllocateWatched(bytes, 65536);
        if (block == nullptr) {
            std::cout << "no write watching: storage refresh in watched memory not tested\n";
            return;
        }
    } else {
#ifdef _WIN32
        block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
        block = std::aligned_alloc(65536, bytes);
#endif
    }
    Require(block != nullptr, "cannot allocate the storage refresh block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* keys = texels + surfaceBytes;
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(keys, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    const auto* import = HostImportFor(base, address, bytes);
    if (import == nullptr) {
        std::cout << "host import of the storage refresh block refused: storage refresh not tested\n";
        return;
    }
    if (watched && !AgcDriver::GuestMemory::Watched(address, bytes)) std::cout << "host imports are compared, not watched: storage refresh in watched memory runs as unwatched\n";
    watched = watched && AgcDriver::GuestMemory::Watched(address, bytes);
    Require(watched || !ShadowDestinationFor(base, *import, address, address + 65536).has_value(), "a unit shadow was offered for memory the write tracker does not watch");
    if (!AgcDriver::GuestMemory::WriteWatched()) Require(!UnitShadowEnabled(), "unit shadows are on without write watching");
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = address + surfaceBytes;
    Require(DescribeSurface(resource).guestBytes == surfaceBytes, "the test surface has an unexpected size");
    DccKeyProof proof;
    Require(ProvedClearKeys(resource, surfaceBytes, proof) == DccKeys::Clear0000, "0x00 keys are not a 0000 clear");
    {
        auto image = std::make_shared<StorageTexture>(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto draw = [&](VkClearColorValue value) {
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
        };
        const auto holds = [&](std::array<std::uint8_t, 4> texel) {
            Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {side, side, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            const auto pixels = readback.Bytes();
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                if (std::to_integer<std::uint8_t>(pixels[i]) != texel[i % 4]) return false;
            }
            return true;
        };
        const auto memoryHolds = [&](std::array<std::uint8_t, 4> texel, std::size_t first = 0, std::uint8_t other = 0) {
            std::vector<std::byte> read(surfaceBytes);
            if (watched) AgcDriver::GuestMemory::Read(address, read);
            else std::memcpy(read.data(), texels, surfaceBytes);
            for (std::size_t i = 0; i < surfaceBytes; ++i) {
                if (std::to_integer<std::uint8_t>(read[i]) != (i < first ? other : texel[i % 4])) return false;
            }
            return true;
        };
        const auto holdsMixed = [&](std::array<std::uint8_t, 4> texel, std::size_t count, std::uint8_t other) {
            Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {side, side, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            const auto pixels = readback.Bytes();
            std::size_t others = 0;
            for (std::size_t i = 0; i < pixels.size(); i += 4) {
                bool isOther = true, isTexel = true;
                for (std::size_t c = 0; c < 4; ++c) {
                    const auto value = std::to_integer<std::uint8_t>(pixels[i + c]);
                    isOther = isOther && value == other;
                    isTexel = isTexel && value == texel[c];
                }
                if (isOther) ++others;
                else if (!isTexel) return false;
            }
            return others == count;
        };
        Require(holds({0, 0, 0, 0}), "a surface under 0000 clear keys was not cleared");
        draw({{1.0f, 0.0f, 0.0f, 1.0f}});
        image->Refresh();
        Require(holds({255, 0, 0, 255}), "a refresh under the image's own clear keys dropped the draw's results");
        Require(memoryHolds({255, 0, 0, 255}), "the refresh's store of the results did not reach guest memory");
        if (watched) {
            draw({{0.0f, 0.0f, 1.0f, 1.0f}});
            std::memset(texels, 0x33, 65536);
            image->Refresh();
            Require(holdsMixed({0, 0, 255, 255}, 65536 / 4, 0x33), "a CPU store into a unit with results pending was not seen by the refresh");
            Require(memoryHolds({0, 0, 255, 255}, 65536, 0x33), "guest memory lost the CPU store or the other units' results");
            image->Refresh();
            Require(holdsMixed({0, 0, 255, 255}, 65536 / 4, 0x33), "an unchanged surface changed at a refresh");
        }
        draw({{0.0f, 1.0f, 0.0f, 1.0f}});
        std::memset(keys, 0x40, keyCount);
        image->Refresh();
        Require(holds({0, 0, 0, 255}), "new 0001 clear keys did not clear the image");
        Require(watched ? memoryHolds({0, 0, 255, 255}, 65536, 0x33) : memoryHolds({255, 0, 0, 255}), "results dead under new clear keys were stored");
    }
    recorder.Sync();
}

void importWatchTests(const Device& device) {
    using namespace AgcDriver::GuestMemory;
    const auto& context = device.GetContext();
#ifdef _WIN32
    static_cast<void>(context);
    std::cout << "import watch decisions: Linux write watch only\n";
#else
    if (context.hostImportAlignment == 0 || !WriteWatched()) {
        std::cout << "host imports or write watching unavailable: import watch decisions not tested\n";
        return;
    }
    const auto probe = ProbeImportWriteProtection(context);
    Require(probe.failure == nullptr, std::string("(u) the import probe failed at ") + (probe.failure != nullptr ? probe.failure : "") + " (" + std::to_string(static_cast<int>(probe.result)) + ")");
    std::cout << "import probe: " << probe.writtenAfterSubmit << " of " << probe.pages << " scratch pages written after a GPU read, " << probe.writtenAtImport << " after the import\n";
    Require(probe.writtenByCpu != 0, "(u) a CPU store after the import probe's GPU read was not collected");
    if (context.dmaBufImport) {
        const auto dmaBuf = ProbeDmaBufImportWriteProtection(context);
        Require(dmaBuf.failure == nullptr, std::string("(u) the dma-buf import probe failed at ") + (dmaBuf.failure != nullptr ? dmaBuf.failure : "") + " (" + std::to_string(static_cast<int>(dmaBuf.result)) + ")");
        Require(dmaBuf.writtenByCpu != 0, "(u) a CPU store into a dma-buf imported scratch range was not collected");
        std::cout << "dma-buf import probe: " << dmaBuf.writtenAfterSubmit << " of " << dmaBuf.pages << " scratch pages written after a GPU read, " << dmaBuf.writtenAtImport << " after the import, " << dmaBuf.writtenByCpu << " after a CPU store\n";
    } else {
        std::cout << "dma-buf imports unavailable: the dma-buf import probe not tested\n";
    }
    const auto decided = PrepareImportWatch(context);
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{context, decided};
    constexpr std::size_t unit = 65536;
    const auto remap = [](void* block, std::size_t bytes) {
        munmap(block, bytes);
        GuestWriteWatch::GuestWriteWatchUnregister_nid_postfix(block, bytes);
        Require(mmap(block, bytes, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0) == block, "(u) cannot map the test block again");
        GuestWriteWatch::GuestWriteWatchRegister_nid_postfix(block, bytes);
    };
    const auto registerRange = [](void* block, std::size_t bytes) {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    };
    const auto unregisterRange = [](void* block) {
        GuestAllocations::Mutation mutation;
        mutation.Remove(block);
    };
    SetImportWatch(context, ImportWatch::Unwatch);
    {
        constexpr std::size_t bytes = 3 * unit;
        void* block = AllocateWatched(bytes, unit);
        const auto address = reinterpret_cast<std::uint64_t>(block);
        std::memset(block, 0x11, bytes);
        const auto before = CollectWrites(address, bytes);
        Require(before != 0, "(u) a watched block is not collected");
        BumpCollectEpoch();
        Require(CollectWrites(address, 2 * unit) != 0, "(u) the memoized collect of a watched block failed");
        registerRange(block, 2 * unit);
        const auto* import = HostImportFor(context, address, 2 * unit);
        Require(import != nullptr && import->unwatched, "(u) an import made under the unwatch decision is not marked unwatched");
        Require(!Watched(address, 2 * unit) && !Watched(address + unit, 4096), "(u) an imported range stays watched");
        Require(CollectWrites(address, 2 * unit) == 0 && CollectWrites(address + 4096, 4096) == 0, "(u) a collect memoized before the import answers for the unwatched range");
        Require(CollectWrites(address + 2 * unit - 4096, 8192) == 0, "(u) a range partly imported is collected");
        Require(Watched(address + 2 * unit, unit), "(u) the unimported neighbour left the watch");
        const auto neighbour = CollectWrites(address + 2 * unit, unit);
        Require(neighbour != 0, "(u) the unimported neighbour is not collected");
        static_cast<volatile std::uint8_t*>(block)[4096] = 0x22;
        Require(!UnchangedSince(address, 2 * unit, before) && !UnchangedSince(address, 4096, TrackerGeneration()) && !UnchangedSinceCollected(address, 4096, TrackerGeneration()), "(u) an unwatched range reports unchanged");
        const std::array<std::uint64_t, 2> generations{before, TrackerGeneration()};
        std::array<std::uint8_t, 2> changed{};
        std::array<std::uint8_t, 2> cpu{};
        Require(!ChangedBlocks(address, 2 * unit, generations, changed, cpu) && changed[0] != 0 && changed[1] != 0, "(u) the stamps of an unwatched range are offered as tracked");
        Require(MarkWritten(address, unit) == 0, "(u) a GPU write into an unwatched range claims a generation");
        Require(MarkWritten(address + 2 * unit, 4096) != 0 && !UnchangedSince(address + 2 * unit, unit, neighbour), "(u) a GPU write into the watched neighbour is not stamped");
        unregisterRange(block);
        remap(block, 2 * unit);
        Require(Watched(address, 2 * unit), "(u) a remapped range is not watched");
        registerRange(block, 2 * unit);
        const auto* kept = HostImportFor(context, address, 2 * unit);
        Require(kept != nullptr && kept->unwatched && !Watched(address, 2 * unit) && CollectWrites(address, 2 * unit) == 0, "(u) an import kept over a remap left the range watched");
        unregisterRange(block);
        remap(block, 2 * unit);
        registerRange(block, unit);
        const auto* again = HostImportFor(context, address, unit);
        Require(again != nullptr && again->unwatched && again->bytes == unit && !Watched(address, unit), "(u) a re-import after an unmap did not follow the decision");
        Require(Watched(address + unit, unit) && CollectWrites(address + unit, unit) != 0, "(u) the unregistered rest of a remapped range is not watched");
        unregisterRange(block);
        Require(HostImportFor(context, address, unit) == nullptr, "(u) an unregistered range still imports");
        ReleaseWatched(block, bytes);
    }
    SetImportWatch(context, ImportWatch::Watch);
    {
        constexpr std::size_t bytes = unit;
        void* block = AllocateWatched(bytes, unit);
        const auto address = reinterpret_cast<std::uint64_t>(block);
        registerRange(block, bytes);
        const auto* import = HostImportFor(context, address, bytes);
        Require(import != nullptr && !import->unwatched && Watched(address, bytes), "(u) an import made under the watch decision left the watch");
        const auto generation = CollectWrites(address, bytes);
        Require(generation != 0 && UnchangedSince(address, bytes, generation), "(u) a watched import is not collected");
        Require(MarkWritten(address, 4096) != 0 && !UnchangedSince(address, bytes, generation), "(u) a GPU write into a watched import is not stamped");
        unregisterRange(block);
        HostImportFor(context, address, bytes);
        ReleaseWatched(block, bytes);
    }
#endif
}

void staleGenerationTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
#ifdef _WIN32
    static_cast<void>(recorder);
    static_cast<void>(base);
    std::cout << "a range leaving the watch: Linux write watch only\n";
#else
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: a range leaving the watch not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) {
        std::cout << "no write watching: a range leaving the watch not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto decided = PrepareImportWatch(base);
    SetImportWatch(base, ImportWatch::Unwatch);
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{base, decided};
    auto* texels = static_cast<std::uint8_t*>(block);
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(texels + surfaceBytes, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = address + surfaceBytes;
    {
        auto image = std::make_shared<StorageTexture>(context, detiler, resource, 0);
        Require(AgcDriver::GuestMemory::Watched(address, bytes), "(s) the storage image imported its memory before a generation was taken");
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto commands = recorder.Commands();
        recorder.Keep(image);
        const VkClearColorValue red{{1.0f, 0.0f, 0.0f, 1.0f}};
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &red, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        image->MarkDirty();
        const auto* import = HostImportFor(base, address, bytes);
        if (import == nullptr) {
            std::cout << "host import of the stale generation block refused: a range leaving the watch not tested\n";
            recorder.Sync();
            return;
        }
        Require(!AgcDriver::GuestMemory::Watched(address, bytes), "(s) the imported range stayed watched");
        image->Refresh();
        Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto copyCommands = recorder.Commands();
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {side, side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(copyCommands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        const auto pixels = readback.Bytes();
        constexpr std::array<std::uint8_t, 4> expected{255, 0, 0, 255};
        for (std::size_t i = 0; i < pixels.size(); ++i) Require(std::to_integer<std::uint8_t>(pixels[i]) == expected[i % 4], "(s) results pending when their memory left the watch were dropped");
        for (std::size_t i = 0; i < surfaceBytes; ++i) Require(texels[i] == expected[i % 4], "(s) results pending when their memory left the watch did not reach it");
    }
    recorder.Sync();
#endif
}

// A write-back stores the texels of an image and leaves every other byte of the surface's memory
// as it was: the padding of the tile blocks the surface covers only partly (its last block column
// and row), the tail block of a mip chain, the bytes between the mips and the pitch padding of a
// linear surface. Another surface over the memory, or the CPU, reads those bytes. Each case clears
// every mip of an image over memory holding a pattern no texel equals, stores it and reads the
// memory back (watched memory through the flush hook, after a store into the import's unit shadow):
// each 4-byte word is the clear texel or the pattern, and the texels are exactly the surface's.
// The 64 KiB-aligned tiled cases store through block windows, the others whole layers.
void writeBackPaddingTests(const Device& device, Recorder& recorder, bool watched) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: write-back padding not tested\n";
        return;
    }
    struct Case {
        const char* name;
        TextureTileMode tileMode;
        std::uint32_t width;
        std::uint32_t height;
        std::uint32_t mipCount;
        std::uint64_t offset;
    };
    constexpr Case cases[] = {
        {"64 KiB R_X 200x150", TextureTileMode::kR64KBX, 200, 150, 1, 0},
        {"64 KiB R_X 200x150, 3 mips", TextureTileMode::kR64KBX, 200, 150, 3, 0},
        {"64 KiB R_X 520x260, 6 mips", TextureTileMode::kR64KBX, 520, 260, 6, 0},
        {"64 KiB R_X 200x150 off a 64 KiB boundary, 3 mips", TextureTileMode::kR64KBX, 200, 150, 3, 4096},
        {"4 KiB S 100x70, 2 mips", TextureTileMode::kStandard4KB, 100, 70, 2, 0},
        {"linear 200x150", TextureTileMode::kLinear, 200, 150, 1, 0},
    };
    const auto pattern = [](std::size_t i) { return static_cast<std::uint8_t>(i * 131u + 7u); };
    constexpr std::array<std::uint8_t, 4> red{255, 0, 0, 255};
    std::string failures;
    for (const auto& test : cases) {
        const std::string what = std::string(watched ? "(watched) " : "") + test.name;
        GuestTextureResource resource{};
        resource.width = test.width;
        resource.height = test.height;
        resource.mipCount = test.mipCount;
        resource.tileMode = test.tileMode;
        resource.dimension = TextureDimension::k2D;
        resource.format = 56;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto geometry = DescribeSurface(resource);
        const auto surfaceBytes = static_cast<std::size_t>(geometry.guestBytes);
        std::size_t texelCount = 0;
        for (const auto& mip : geometry.mips) texelCount += static_cast<std::size_t>(mip.width) * mip.height;
        // The surface, then a 64 KiB unit past it that no store may reach.
        const auto bytes = (static_cast<std::size_t>(test.offset) + surfaceBytes + 65535) / 65536 * 65536 + 65536;
        void* block = nullptr;
        if (watched) {
            block = AllocateWatched(bytes, 65536);
            if (block == nullptr) {
                std::cout << "no write watching: write-back padding in watched memory not tested\n";
                return;
            }
        } else {
#ifdef _WIN32
            block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
            block = std::aligned_alloc(65536, bytes);
#endif
        }
        Require(block != nullptr, "cannot allocate the write-back padding block");
        auto* memory = static_cast<std::uint8_t*>(block);
        for (std::size_t i = 0; i < bytes; ++i) memory[i] = pattern(i);
        const auto blockAddress = reinterpret_cast<std::uint64_t>(block);
        const auto address = blockAddress + test.offset;
        {
            GuestAllocations::Mutation mutation;
            mutation.Add(block, bytes, true, true);
        }
        struct Unregister {
            const Context& context;
            void* block;
            std::uint64_t address;
            std::size_t bytes;
            bool watched;
            ~Unregister() {
                {
                    GuestAllocations::Mutation mutation;
                    mutation.Remove(block);
                }
                HostImportFor(context, address, bytes);
                if (watched) ReleaseWatched(block, bytes);
            }
        } unregister{base, block, blockAddress, bytes, watched};
        if (HostImportFor(base, blockAddress, bytes) == nullptr) {
            std::cout << "host import of the write-back padding block refused: write-back padding not tested\n";
            return;
        }
        TextureDetiler detiler(base);
        auto context = base;
        context.detiler = &detiler;
        resource.baseAddress = address;
        {
            auto image = std::make_shared<StorageTexture>(context, detiler, resource, 0);
            const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, test.mipCount, 0, 1};
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            const VkClearColorValue value{{1.0f, 0.0f, 0.0f, 1.0f}};
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
            image->WriteBack();
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            std::vector<std::byte> read(bytes);
            if (watched) AgcDriver::GuestMemory::Read(blockAddress, read);
            else std::memcpy(read.data(), memory, bytes);
            std::size_t texels = 0, garbage = 0, outside = 0;
            for (std::size_t i = 0; i < bytes; i += 4) {
                const bool inside = i >= test.offset && i < test.offset + surfaceBytes;
                bool isTexel = inside, isPattern = true;
                for (std::size_t c = 0; c < 4; ++c) {
                    const auto byte = std::to_integer<std::uint8_t>(read[i + c]);
                    isTexel = isTexel && byte == red[c];
                    isPattern = isPattern && byte == pattern(i + c);
                }
                if (isTexel) ++texels;
                else if (!isPattern) ++(inside ? garbage : outside);
            }
            if (garbage != 0 || outside != 0 || texels != texelCount) {
                failures += what + ": the write-back stored " + std::to_string(texels) + " texels of " + std::to_string(texelCount) + " and changed " + std::to_string(garbage) + " words no texel holds (" + std::to_string(outside) + " past the surface)\n";
            }
        }
        recorder.Sync();
    }
    if (!failures.empty()) throw std::runtime_error("write-back padding:\n" + failures);
    std::cout << "write-back padding" << (watched ? " (watched)" : "") << ": ok\n";
}

void importWindowTests(const Device& device, Recorder& recorder) {
    using namespace AgcDriver::GuestMemory;
    const auto& base = device.GetContext();
#ifdef _WIN32
    static_cast<void>(recorder);
    static_cast<void>(base);
    std::cout << "the import window: Linux write watch only\n";
#else
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: the import window not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
    void* block = AllocateWatched(bytes, 65536);
    if (block == nullptr) {
        std::cout << "no write watching: the import window not tested\n";
        return;
    }
    struct Release {
        void* block;
        ~Release() { ReleaseWatched(block, bytes); }
    } release{block};
    const auto probe = ProbeImportWriteProtection(base);
    Require(probe.failure == nullptr, "(w) the import probe failed");
    const bool importWrites = probe.writtenAtImport != 0;
    const auto decided = PrepareImportWatch(base);
    if (decided == ImportWatch::Unwatch) {
        std::cout << "host imports are compared, not watched: the import window not tested\n";
        return;
    }
    SetImportWatch(base, ImportWatch::Watch);
    struct Restore {
        const Context& context;
        ImportWatch decided;
        ~Restore() { SetImportWatch(context, decided); }
    } restore{base, decided};
    auto* texels = static_cast<std::uint8_t*>(block);
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(texels + surfaceBytes, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = address + surfaceBytes;
    {
        auto image = std::make_shared<StorageTexture>(context, detiler, resource, 0);
        Require(!HostImportCovers(base, address, bytes), "(w) the storage image imported its memory before a generation was taken");
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto commands = recorder.Commands();
        recorder.Keep(image);
        const VkClearColorValue red{{1.0f, 0.0f, 0.0f, 1.0f}};
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
        context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &red, 1, &range);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
        image->MarkDirty();
        const auto cached = CollectWritesUncached(address, surfaceBytes);
        Require(cached != 0 && UnchangedSince(address, surfaceBytes, cached), "(w) the cache's generation is not current before the import");
        const auto* import = HostImportFor(base, address, bytes);
        if (import == nullptr) {
            std::cout << "host import of the import window block refused: the import window not tested\n";
            recorder.Sync();
            return;
        }
        Require(!import->unwatched && Watched(address, bytes), "(w) an import under the watch decision left the watch");
        CollectWritesUncached(address, surfaceBytes);
        if (importWrites) {
            Require(!UnchangedSince(address, surfaceBytes, cached), "(w) a cache over the pages the import reported sees no change");
            std::array<std::uint8_t, surfaceBytes / 65536> changed{};
            const std::array<std::uint64_t, surfaceBytes / 65536> generations{cached, cached, cached, cached};
            Require(ChangedBlocks(address, surfaceBytes, generations, changed) && std::all_of(changed.begin(), changed.end(), [](std::uint8_t value) { return value == BlockMaybeWritten; }), "(w) the pages the import reported read as written by the CPU");
            Require(!WrittenSince(address, surfaceBytes, cached), "(w) the import window reads as a CPU store");
        } else {
            std::cout << "the import reports no pages here: only the pending results of the import window checked\n";
        }
        const auto after = CollectWritesUncached(address, surfaceBytes);
        Require(after != 0 && UnchangedSince(address, surfaceBytes, after), "(w) the imported range is not watched again after the import window");
        image->Refresh();
        Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
        const auto copyCommands = recorder.Commands();
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
        VkBufferImageCopy copy{};
        copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
        copy.imageExtent = {side, side, 1};
        context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(copyCommands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
        RecordMemoryBarrier(context, copyCommands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        const auto pixels = readback.Bytes();
        constexpr std::array<std::uint8_t, 4> expected{255, 0, 0, 255};
        for (std::size_t i = 0; i < pixels.size(); ++i) Require(std::to_integer<std::uint8_t>(pixels[i]) == expected[i % 4], "(w) results pending over the import window were dropped");
        std::vector<std::byte> memory(surfaceBytes);
        Read(address, memory);
        for (std::size_t i = 0; i < surfaceBytes; ++i) Require(std::to_integer<std::uint8_t>(memory[i]) == expected[i % 4], "(w) results pending over the import window did not reach guest memory");
        const auto stored = CollectWritesUncached(address, surfaceBytes);
        texels[65536 + 8] = 0x77;
        CollectWritesUncached(address, surfaceBytes);
        Require(WrittenSince(address + 65536, 65536, stored) && !WrittenSince(address, 65536, stored), "(w) a CPU store after the import window is not reported as one");
    }
    recorder.Sync();
#endif
}

void pendingKeyStoreTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: pending key stores not tested\n";
        return;
    }
    constexpr std::size_t surfaceBytes = 65536;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the pending key store block");
    auto* keys = static_cast<std::uint8_t*>(block);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{context, block, address};
    if (HostImportFor(context, address, bytes) == nullptr) {
        std::cout << "host import of the pending key store block refused: pending key stores not tested\n";
        return;
    }
    recorder.Sync();
    std::memset(keys, 0x00, keyCount);
    MarkDccUncompressed(context, address, surfaceBytes);
    Require(recorder.PendingWriteOverlaps(address, keyCount) && keys[0] == 0x00, "the uncompressed key store did not stay pending");
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Uncompressed, "the keys after a pending uncompressed store do not read as uncompressed");
    Require(recorder.PendingWriteOverlaps(address, keyCount), "reading keys the driver's own pending store wrote waited for the GPU");
    Require(CurrentDccKeys(address + 16, surfaceBytes / 2) == DccKeys::Uncompressed, "a key range inside the pending store does not read as uncompressed");
    recorder.NotePendingWrite(address + 16, 16);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Uncompressed && !recorder.PendingWriteOverlaps(address, keyCount), "a key read with a later writer over the pending store did not wait for it");
    Require(std::all_of(keys, keys + keyCount, [](std::uint8_t key) { return key == 0xff; }), "the uncompressed key store did not land");
    recorder.NotePendingWrite(address, keyCount);
    NoteKeysFillOnGpu(address, keyCount, DccKeys::Clear0001);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Clear0001 && recorder.PendingWriteOverlaps(address, keyCount), "the keys of a pending fill did not read as its keys without a wait");
    recorder.NotePendingWrite(address, 2 * keyCount);
    Require(CurrentDccKeys(address, surfaceBytes) == DccKeys::Uncompressed && !recorder.PendingWriteOverlaps(address, keyCount), "a later wider writer over a pending fill was not waited for");
    recorder.Sync();
}

void sampleDumpTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (context.hostImportAlignment == 0 || !context.bufferDeviceAddress) {
        std::cout << "host imports or buffer device addresses unavailable: occlusion counter dumps on the GPU not tested\n";
        return;
    }
    constexpr std::size_t bytes = 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the occlusion counter block");
    auto* words = static_cast<std::uint64_t*>(block);
    constexpr std::uint64_t untouched = 0xaaaaaaaaaaaaaaaaull;
    std::fill(words, words + 64, untouched);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{context, block, address};
    const auto* import = HostImportFor(context, address, bytes);
    if (import == nullptr || import->address == 0) {
        std::cout << "host import of the occlusion counter block refused: occlusion counter dumps on the GPU not tested\n";
        return;
    }
    const auto target = import->address + (address - import->base);
    recorder.Sync();
    constexpr std::uint64_t ready = 1ull << 63u;
    Require(recorder.DumpSamples(target), "the occlusion counters were not dumped on the GPU");
    Require(words[0] == untouched && !recorder.Idle(), "the occlusion counter dump waited for the GPU or landed before its batch ran");
    recorder.Sync();
    const auto begin = recorder.SamplesTotal();
    for (std::size_t db = 0; db < 16; ++db) {
        Require(words[db * 2] == ((db == 0 ? begin : 0) | ready), "an occlusion counter dump stored the wrong value");
        Require(words[db * 2 + 1] == untouched, "an occlusion counter dump stored over the next counter");
    }
    Require(recorder.DumpSamples(target + 8), "the second occlusion counter dump was not made on the GPU");
    recorder.Submit();
    recorder.Sync();
    for (std::size_t db = 0; db < 16; ++db) {
        Require(words[db * 2 + 1] == ((db == 0 ? begin : 0) | ready), "the counters changed with nothing drawn between two dumps");
        Require(words[db * 2] == ((db == 0 ? begin : 0) | ready), "the second dump stored over the first");
    }
    Require(recorder.SamplesTotal() == begin, "the sample total moved with nothing drawn");
    for (int i = 0; i < 40; ++i) Require(recorder.DumpSamples(target), "a dump past one batch's query slots was not made on the GPU");
    recorder.Sync();
    Require(words[0] == (begin | ready) && recorder.SamplesTotal() == begin, "dumps past one batch's query slots moved the total");
}

void metadataPassTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: resident metadata passes not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the metadata pass block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* keys = texels + surfaceBytes;
    const auto address = reinterpret_cast<std::uint64_t>(block);
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            ClearCachedTextures(context.device);
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the metadata pass block refused: resident metadata passes not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    ColorTarget color{};
    color.address = address;
    color.extent = {side, side};
    color.format = VK_FORMAT_R8G8B8A8_UNORM;
    color.bytes = surfaceBytes;
    color.componentMapping = 0xe4u;
    color.tileMode = ColorTileMode::RenderTarget;
    color.elementBytes = 4;
    color.dccAddress = address + surfaceBytes;
    color.clearWords = {0x80402010u, 0};
    const ColorMetadataPass pass{ColorMetadataPass::Mode::EliminateFastClear, {color}};
    const auto memoryHolds = [&](std::array<std::uint8_t, 4> texel) {
        StorageTexture::FlushPending(address, surfaceBytes, nullptr, "test");
        recorder.Submit();
        device.WaitQueue();
        recorder.Sync();
        std::vector<std::uint8_t> stored(surfaceBytes);
        AgcDriver::GuestMemory::Read(address, std::as_writable_bytes(std::span(stored)), 1);
        for (std::size_t i = 0; i < surfaceBytes; ++i) {
            if (stored[i] != texel[i % 4]) return false;
        }
        return true;
    };
    const auto keysUncompressed = [&] { return ReadDccKeys(color.dccAddress, surfaceBytes) == DccKeys::Uncompressed; };
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(keys, 0x20, keyCount);
    RunColorMetadataPass(context, pass);
    Require(StorageTexture::FindPending(address, surfaceBytes) != nullptr && texels[0] == 0x55, "the register fast clear eliminate did not stay in the resident image");
    Require(keysUncompressed(), "the register fast clear eliminate left the keys compressed");
    Require(memoryHolds({0x10, 0x20, 0x40, 0x80}), "the register fast clear eliminate did not store CB_COLOR_CLEAR_WORD");
    Require(std::all_of(keys, keys + keyCount, [](std::uint8_t key) { return key == 0xff; }), "the stored keys are not uncompressed");
    std::memset(keys, 0xc0, keyCount);
    RunColorMetadataPass(context, {ColorMetadataPass::Mode::DccDecompress, {color}});
    Require(StorageTexture::FindPending(address, surfaceBytes) != nullptr, "the DCC decompress did not stay in the resident image");
    Require(keysUncompressed(), "the DCC decompress left the keys compressed");
    Require(memoryHolds({0xff, 0xff, 0xff, 0xff}), "the DCC decompress did not store the 1111 value");
    {
        const auto* import = HostImportFor(base, address, bytes);
        Require(import != nullptr, "the metadata pass block lost its import");
        const auto commands = recorder.Commands();
        context.Function<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(commands, import->buffer, color.dccAddress - import->base, keyCount, 0x20202020u);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_HOST_READ_BIT);
        recorder.NotePendingWrite(color.dccAddress, keyCount);
        Require(keys[0] == 0xff, "the recorded key fill landed before its batch ran");
        Require(CurrentDccKeys(color.dccAddress, surfaceBytes) == DccKeys::ClearRegister, "the keys read past a pending fast-clear fill");
    }
    RunColorMetadataPass(context, pass);
    Require(keysUncompressed() && memoryHolds({0x10, 0x20, 0x40, 0x80}), "a fast clear recorded before the pass was not eliminated");
    std::memset(texels, 0x66, surfaceBytes);
    RunColorMetadataPass(context, pass);
    Require(memoryHolds({0x66, 0x66, 0x66, 0x66}), "a pass over uncompressed keys changed the texels");
    ClearCachedTextures(context.device);
    GuestTextureResource plain{};
    plain.baseAddress = address;
    plain.width = side;
    plain.height = side;
    plain.mipCount = 1;
    plain.tileMode = TextureTileMode::kR64KBX;
    plain.dimension = TextureDimension::k2D;
    plain.format = 56;
    plain.dstSelX = 4;
    plain.dstSelY = 5;
    plain.dstSelZ = 6;
    plain.dstSelW = 7;
    const auto unkeyed = CachedStorageSurface(context, plain);
    std::memset(keys, 0xc0, keyCount);
    RunColorMetadataPass(context, pass);
    const auto keyed = StorageTexture::FindPending(address, surfaceBytes);
    Require(keyed != nullptr && keyed != unkeyed && keyed->Descriptor().dccAddress == color.dccAddress, "a pass over a resident image under other keys did not remake it under the target's keys");
    Require(keysUncompressed() && memoryHolds({0xff, 0xff, 0xff, 0xff}), "a pass over a remade resident image did not store the 1111 value");
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

class SampleProgram {
public:
    SampleProgram(const Context& context, Recorder& recorder) : context(context), recorder(recorder), result(context, sizeof(float) * 4, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT) {
        VkDescriptorSetLayoutBinding bindings[2]{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        bindings[1] = {1, VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1, VK_SHADER_STAGE_COMPUTE_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo layoutInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
        layoutInfo.bindingCount = 2;
        layoutInfo.pBindings = bindings;
        Check(context.Function<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(context.device, &layoutInfo, nullptr, &setLayout), "vkCreateDescriptorSetLayout");
        const VkPushConstantRange push{VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)};
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &setLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &push;
        Check(context.Function<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(context.device, &pipelineLayoutInfo, nullptr, &pipelineLayout), "vkCreatePipelineLayout");
        VkShaderModuleCreateInfo moduleInfo{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
        moduleInfo.codeSize = sizeof(SAMPLE_LOD_SPV);
        moduleInfo.pCode = SAMPLE_LOD_SPV;
        Check(context.Function<PFN_vkCreateShaderModule>("vkCreateShaderModule")(context.device, &moduleInfo, nullptr, &module), "vkCreateShaderModule");
        VkComputePipelineCreateInfo pipelineInfo{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};
        pipelineInfo.stage = {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, nullptr, 0, VK_SHADER_STAGE_COMPUTE_BIT, module, "main", nullptr};
        pipelineInfo.layout = pipelineLayout;
        Check(context.Function<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(context.device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline), "vkCreateComputePipelines");
        VkSamplerCreateInfo samplerInfo{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.addressModeU = samplerInfo.addressModeV = samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE;
        Check(context.Function<PFN_vkCreateSampler>("vkCreateSampler")(context.device, &samplerInfo, nullptr, &sampler), "vkCreateSampler");
        const VkDescriptorPoolSize sizes[2]{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1}};
        VkDescriptorPoolCreateInfo poolInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};
        poolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        poolInfo.maxSets = 1;
        poolInfo.poolSizeCount = 2;
        poolInfo.pPoolSizes = sizes;
        Check(context.Function<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(context.device, &poolInfo, nullptr, &pool), "vkCreateDescriptorPool");
    }
    ~SampleProgram() {
        context.Function<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(context.device, pool, nullptr);
        context.Function<PFN_vkDestroySampler>("vkDestroySampler")(context.device, sampler, nullptr);
        context.Function<PFN_vkDestroyPipeline>("vkDestroyPipeline")(context.device, pipeline, nullptr);
        context.Function<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(context.device, module, nullptr);
        context.Function<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(context.device, pipelineLayout, nullptr);
        context.Function<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(context.device, setLayout, nullptr);
    }
    SampleProgram(const SampleProgram&) = delete;
    SampleProgram& operator=(const SampleProgram&) = delete;

    float Red(VkImageView view, VkImageLayout layout, float lod) {
        VkDescriptorSetAllocateInfo allocateInfo{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};
        allocateInfo.descriptorPool = pool;
        allocateInfo.descriptorSetCount = 1;
        allocateInfo.pSetLayouts = &setLayout;
        VkDescriptorSet set = VK_NULL_HANDLE;
        Check(context.Function<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(context.device, &allocateInfo, &set), "vkAllocateDescriptorSets");
        const VkDescriptorImageInfo image{sampler, view, layout};
        const VkDescriptorBufferInfo buffer{result.Handle(), 0, VK_WHOLE_SIZE};
        VkWriteDescriptorSet writes[2]{{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
        writes[0].dstSet = set;
        writes[0].dstBinding = 0;
        writes[0].descriptorCount = 1;
        writes[0].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[0].pImageInfo = &image;
        writes[1].dstSet = set;
        writes[1].dstBinding = 1;
        writes[1].descriptorCount = 1;
        writes[1].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        writes[1].pBufferInfo = &buffer;
        context.Function<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(context.device, 2, writes, 0, nullptr);
        const auto commands = recorder.Commands();
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT);
        context.Function<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline);
        context.Function<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(commands, VK_PIPELINE_BIND_POINT_COMPUTE, pipelineLayout, 0, 1, &set, 0, nullptr);
        context.Function<PFN_vkCmdPushConstants>("vkCmdPushConstants")(commands, pipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(lod), &lod);
        context.Function<PFN_vkCmdDispatch>("vkCmdDispatch")(commands, 1, 1, 1);
        RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
        recorder.Submit();
        recorder.Sync();
        Check(context.Function<PFN_vkFreeDescriptorSets>("vkFreeDescriptorSets")(context.device, pool, 1, &set), "vkFreeDescriptorSets");
        float texel[4];
        std::memcpy(texel, result.Bytes().data(), sizeof(texel));
        return texel[0];
    }

private:
    const Context& context;
    Recorder& recorder;
    Buffer result;
    VkDescriptorSetLayout setLayout = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkShaderModule module = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    VkDescriptorPool pool = VK_NULL_HANDLE;
};

void expectRed(float got, float want, const char* what) {
    if (std::abs(got - want) > 1.5f / 255.0f) throw std::runtime_error(std::string(what) + ": read " + std::to_string(got) + ", expected " + std::to_string(want));
}

void minLodTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    if (!context.imageViewMinLod) {
        std::cout << "VK_EXT_image_view_min_lod unavailable: minimum LOD clamp not tested\n";
        return;
    }
    TextureDetiler detiler(context);
    GuestTextureResource resource{};
    resource.baseAddress = 0x100000;
    resource.width = 8;
    resource.height = 8;
    resource.mipCount = 4;
    resource.baseLevel = 0;
    resource.lastLevel = 3;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    Require(geometry.mips.size() == 4, "the min LOD test surface has an unexpected mip count");
    const std::array<std::uint8_t, 4> levels{0x00, 0x40, 0x80, 0xff};
    std::vector<std::byte> snapshot(static_cast<std::size_t>(geometry.guestBytes));
    for (std::size_t level = 0; level < levels.size(); ++level) {
        const auto& mip = geometry.mips[level];
        std::fill_n(snapshot.begin() + static_cast<std::ptrdiff_t>(mip.tiledOffset), static_cast<std::size_t>(mip.tiledSize), std::byte{levels[level]});
    }
    SampleProgram program(context, recorder);
    const VkComponentMapping identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    const auto sample = [&](std::uint32_t minLod, float lod, std::uint32_t baseLevel = 0) {
        auto described = resource;
        described.minLod = minLod;
        described.baseLevel = baseLevel;
        Texture texture(context, detiler, described, identity, snapshot);
        return program.Red(texture.View(), texture.Layout(), lod);
    };
    const auto unorm = [&](std::size_t level) { return levels[level] / 255.0f; };
    expectRed(sample(0, 0.0f), unorm(0), "minimum LOD clamp: no clamp reads level 0");
    expectRed(sample(0x100, 0.0f), unorm(1), "minimum LOD clamp: MIN_LOD 1 reads level 1 at LOD 0");
    const auto fractional = sample(0x180, 0.0f);
    if (std::abs(fractional - unorm(1)) <= 1.5f / 255.0f) {
        std::cout << "minimum LOD clamp: the device takes the integer part of a fractional view minimum LOD\n";
    } else {
        expectRed(fractional, (unorm(1) + unorm(2)) / 2.0f, "minimum LOD clamp: MIN_LOD 1.5 blends levels 1 and 2");
    }
    expectRed(sample(0x100, 2.0f), unorm(2), "minimum LOD clamp: MIN_LOD 1 lowered LOD 2");
    expectRed(sample(0xfff, 0.0f), unorm(3), "minimum LOD clamp: MIN_LOD past the last level reads the last level");
    expectRed(sample(0x200, 0.0f, 1), unorm(2), "minimum LOD clamp: MIN_LOD 2 over a view from level 1 reads level 2");
    expectRed(sample(0x100, 0.0f, 1), unorm(1), "minimum LOD clamp: MIN_LOD at the view's base level reads its base level");
}

void firstLayerViewTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    TextureDetiler detiler(context);
    auto withDetiler = context;
    withDetiler.detiler = &detiler;
    GuestTextureResource resource{};
    resource.width = 64;
    resource.height = 4;
    resource.depthOrLastArray = 2;
    resource.baseArray = 1;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kLinear;
    resource.dimension = TextureDimension::k2DArray;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    const auto geometry = DescribeSurface(resource);
    std::vector<std::uint8_t> memory(static_cast<std::size_t>(geometry.guestBytes) + 256);
    auto* surface = reinterpret_cast<std::uint8_t*>((reinterpret_cast<std::uintptr_t>(memory.data()) + 255) & ~std::uintptr_t{255});
    for (std::uint32_t layer = 0; layer < 3; ++layer) std::memset(surface + geometry.GuestLayerOffset(layer), 0x20 * (layer + 1), static_cast<std::size_t>(geometry.layerBytes));
    resource.baseAddress = reinterpret_cast<std::uint64_t>(surface);
    auto image = std::make_shared<StorageTexture>(withDetiler, detiler, resource, 0);
    recorder.Keep(image);
    SampleProgram program(context, recorder);
    expectRed(program.Red(image->FirstLayerView(0), VK_IMAGE_LAYOUT_GENERAL, 0.0f), 0x40 / 255.0f, "a first-layer view does not read the BASE_ARRAY layer");
    Require(image->FirstLayerView(0) == image->FirstLayerView(0), "first-layer views are not reused");
    const VkComponentMapping identity{VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY};
    Texture snapshotTexture(context, detiler, resource, identity, std::span<const std::byte>(reinterpret_cast<const std::byte*>(surface), static_cast<std::size_t>(geometry.guestBytes)));
    Require(snapshotTexture.FirstLayerView() != VK_NULL_HANDLE, "a sampled 2D array texture has no first-layer view");
    expectRed(program.Red(snapshotTexture.FirstLayerView(), snapshotTexture.Layout(), 0.0f), 0x40 / 255.0f, "a sampled first-layer view does not read the BASE_ARRAY layer");
    Texture storageView(context, image, resource, identity);
    Require(storageView.FirstLayerView() != VK_NULL_HANDLE, "a sampled view of a 2D array storage image has no first-layer view");
    expectRed(program.Red(storageView.FirstLayerView(), storageView.Layout(), 0.0f), 0x40 / 255.0f, "a sampled first-layer view of a storage image does not read the BASE_ARRAY layer");
    auto flat = resource;
    flat.dimension = TextureDimension::k2D;
    flat.depthOrLastArray = 0;
    flat.baseArray = 0;
    Texture flatTexture(context, detiler, flat, identity, std::span<const std::byte>(reinterpret_cast<const std::byte*>(surface), static_cast<std::size_t>(DescribeSurface(flat).guestBytes)));
    Require(flatTexture.FirstLayerView() == VK_NULL_HANDLE, "a 2D texture has a first-layer view");
    expectRed(program.Red(flatTexture.View(), flatTexture.Layout(), 0.0f), 0x20 / 255.0f, "a 2D texture over the surface does not read its first layer");
}

void atomicViewTests(const Device& device, Recorder& recorder) {
    const auto& context = device.GetContext();
    TextureDetiler detiler(context);
    auto withDetiler = context;
    withDetiler.detiler = &detiler;
    for (const std::uint32_t format : {20u, 21u, 22u, 56u}) {
        GuestTextureResource resource{};
        resource.width = 64;
        resource.height = 4;
        resource.mipCount = 1;
        resource.tileMode = TextureTileMode::kLinear;
        resource.dimension = TextureDimension::k2D;
        resource.format = format;
        resource.dstSelX = 4;
        resource.dstSelY = 5;
        resource.dstSelZ = 6;
        resource.dstSelW = 7;
        const auto geometry = DescribeSurface(resource);
        std::vector<std::uint8_t> memory(static_cast<std::size_t>(geometry.guestBytes) + 256);
        resource.baseAddress = (reinterpret_cast<std::uintptr_t>(memory.data()) + 255) & ~std::uintptr_t{255};
        auto image = std::make_shared<StorageTexture>(withDetiler, detiler, resource, 0);
        recorder.Keep(image);
        if (format == 56u) {
            bool refused = false;
            try {
                static_cast<void>(image->AtomicView(0, false));
            } catch (const std::exception&) {
                refused = true;
            }
            Require(refused, "an 8_8_8_8 storage image has an atomic view");
            continue;
        }
        const auto atomic = image->AtomicView(0, false);
        Require(atomic != VK_NULL_HANDLE && atomic == image->AtomicView(0, false), "atomic storage views are not reused");
        Require((atomic == image->View(0)) == (format == 20u), "only a 32_UINT storage image serves atomics through its own view");
    }
}

void keysFillTests(const Device& device, Recorder& recorder) {
    const auto& base = device.GetContext();
    if (base.hostImportAlignment == 0) {
        std::cout << "host imports unavailable: DCC key fills not tested\n";
        return;
    }
    constexpr std::uint32_t side = 256;
    constexpr std::size_t surfaceBytes = side * side * 4;
    constexpr std::size_t keyCount = surfaceBytes / 256;
    constexpr std::size_t bytes = surfaceBytes + 65536;
#ifdef _WIN32
    void* block = VirtualAlloc(nullptr, bytes, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
#else
    void* block = std::aligned_alloc(65536, bytes);
#endif
    Require(block != nullptr, "cannot allocate the key fill block");
    auto* texels = static_cast<std::uint8_t*>(block);
    auto* keys = texels + surfaceBytes;
    std::memset(texels, 0x55, surfaceBytes);
    std::memset(keys, 0x00, keyCount);
    const auto address = reinterpret_cast<std::uint64_t>(block);
    const auto keysAddress = address + surfaceBytes;
    {
        GuestAllocations::Mutation mutation;
        mutation.Add(block, bytes, true, true);
    }
    struct Unregister {
        const Context& context;
        void* block;
        std::uint64_t address;
        ~Unregister() {
            {
                GuestAllocations::Mutation mutation;
                mutation.Remove(block);
            }
            HostImportFor(context, address, bytes);
        }
    } unregister{base, block, address};
    if (HostImportFor(base, address, bytes) == nullptr) {
        std::cout << "host import of the key fill block refused: DCC key fills not tested\n";
        return;
    }
    TextureDetiler detiler(base);
    auto context = base;
    context.detiler = &detiler;
    GuestTextureResource resource{};
    resource.baseAddress = address;
    resource.width = side;
    resource.height = side;
    resource.mipCount = 1;
    resource.tileMode = TextureTileMode::kR64KBX;
    resource.dimension = TextureDimension::k2D;
    resource.format = 56;
    resource.dstSelX = 4;
    resource.dstSelY = 5;
    resource.dstSelZ = 6;
    resource.dstSelW = 7;
    resource.dccAddress = keysAddress;
    Require(DescribeSurface(resource).guestBytes == surfaceBytes, "the key fill surface has an unexpected size");
    {
        auto image = std::make_shared<StorageTexture>(context, detiler, resource, 0);
        const VkImageSubresourceRange range{VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
        const auto draw = [&](VkClearColorValue value) {
            const auto commands = recorder.Commands();
            recorder.Keep(image);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_WRITE_BIT);
            context.Function<PFN_vkCmdClearColorImage>("vkCmdClearColorImage")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_READ_BIT | VK_ACCESS_TRANSFER_WRITE_BIT);
            image->MarkDirty();
        };
        const auto holds = [&](std::array<std::uint8_t, 4> texel) {
            Buffer readback(context, surfaceBytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT);
            const auto commands = recorder.Commands();
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT);
            VkBufferImageCopy copy{};
            copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
            copy.imageExtent = {side, side, 1};
            context.Function<PFN_vkCmdCopyImageToBuffer>("vkCmdCopyImageToBuffer")(commands, image->Image(), VK_IMAGE_LAYOUT_GENERAL, readback.Handle(), 1, &copy);
            RecordMemoryBarrier(context, commands, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_HOST_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_HOST_READ_BIT);
            recorder.Submit();
            device.WaitQueue();
            recorder.Sync();
            const auto pixels = readback.Bytes();
            for (std::size_t i = 0; i < pixels.size(); ++i) {
                if (std::to_integer<std::uint8_t>(pixels[i]) != texel[i % 4]) return false;
            }
            return true;
        };
        const auto fill = [&](std::uint8_t key) {
            recorder.Sync();
            std::memset(keys, key, keyCount);
            return StorageTexture::NoteKeysFill(keysAddress, keyCount, key);
        };
        Require(holds({0, 0, 0, 0}), "a surface under 0000 keys was not cleared");
        draw({{1.0f, 0.0f, 0.0f, 1.0f}});
        Require(fill(0x00) == 1, "a 0000 key fill did not cover the surface");
        Require(image->FilledKeys() == DccKeys::Clear0000, "a key fill over pending results was not recorded");
        image->Refresh();
        Require(holds({0, 0, 0, 0}), "a key fill did not clear the results made before it at the next refresh");
        Require(image->FilledKeys() == DccKeys::Uncompressed, "the image cleared by a refresh still holds the fill");
        draw({{0.0f, 0.0f, 1.0f, 1.0f}});
        Require(fill(0x00) == 1, "a second 0000 key fill did not cover the surface");
#ifdef _WIN32
        _putenv_s("APS5_KEYS_FILL_CLEAR", "1");
#else
        setenv("APS5_KEYS_FILL_CLEAR", "1", 1);
#endif
        Require(StorageTexture::ClearByKeysFill(keysAddress, keyCount, 0x00) == 1, "a 0000 key fill did not clear the surface at once");
        Require(holds({0, 0, 0, 0}), "a key fill cleared at once left results made before it");
        draw({{0.0f, 1.0f, 0.0f, 1.0f}});
        Require(image->FilledKeys() == DccKeys::Uncompressed, "results drawn after a key fill cleared at once are held under the fill's code");
        Require(holds({0, 255, 0, 255}), "results drawn after a key fill cleared at once were lost");
    }
    recorder.Sync();
}

int main(int argc, char** argv) {
    try {
        Device device;
        std::lock_guard gpu(GpuMutex());
        std::cout << "host imports " << (PrepareImportWatch(device.GetContext()) == ImportWatch::Unwatch ? "are compared" : "stay watched") << '\n';
        Recorder recorder(device.GetContext());
        recorder.Activate();
        if (argc == 2 && std::string_view(argv[1]) == "--draw-descriptors-only") {
            drawDescriptorWriteTests(device, recorder);
            std::cout << "Draw descriptor writes and snapshot overrides passed\n";
            return 0;
        }

        readTrackingTests(device, recorder);
        writeSettledTests(device, recorder);
        completionCountTests(device, recorder);
        afterRecordedWorkTests(device, recorder);
        batchStampTests(recorder);
        labelTests(recorder);
        lateLabelTests(recorder);
        largeLabelTests(recorder);
        unchangedSinceTests();
        closeRaceTests(device, recorder);
        keyProofTests(device, recorder);
        resourceReadTests(device, recorder);
        misalignedSnapshotTests(device, recorder);
        drawSnapshotReuseTests(device, recorder);
        drawDescriptorWriteTests(device, recorder);
        drawSnapshotEvictionTests(device);
        drawInputReuseTests(device, recorder);
        RunResidentPresentTests(device.GetContext());
        storeRunTests(device, recorder);
        remappedImportTests(device);
        movedMetadataTests(device, recorder);
        viewPastLastMipTests(device, recorder);
        keysFillTests(device, recorder);
        unitShadowTests(device, recorder);
        storageRefreshTests(device, recorder, false);
        storageRefreshTests(device, recorder, true);
        writeBackPaddingTests(device, recorder, false);
        writeBackPaddingTests(device, recorder, true);
        importWatchTests(device);
        staleGenerationTests(device, recorder);
        importWindowTests(device, recorder);
        dataWordPositionsTests();
        dataRefreshTests(device, recorder);
        minLodTests(device, recorder);
        firstLayerViewTests(device, recorder);
        atomicViewTests(device, recorder);
        metadataPassTests(device, recorder);
        pendingKeyStoreTests(device, recorder);
        sampleDumpTests(device, recorder);
        std::cout << "Recorder read tracking and label tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
