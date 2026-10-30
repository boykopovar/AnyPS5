#include "prx/libSceNgs2.native/tests/Ngs2Test.hpp"
#include <algorithm>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <iterator>
#include <map>
#include <memory>
#include <mutex>
#include <new>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "prx/libc/include/General.hpp"
#include "Ngs2Internal.hpp"

static std::vector<Ngs2System*>& Systems() {
    static auto* const systems = new std::vector<Ngs2System*>();
    return *systems;
}

struct Ngs2ReportRegistration {
    std::uint32_t type;
    Ngs2ReportHandler handler;
    std::uintptr_t userData;
};

static std::vector<Ngs2ReportRegistration*>& ReportRegistrations() {
    static auto* const registrations = new std::vector<Ngs2ReportRegistration*>();
    return *registrations;
}

std::string Ngs2Hex(std::uint32_t value) {
    char text[16];
    std::snprintf(text, sizeof(text), "0x%x", value);
    return text;
}

std::recursive_mutex& Ngs2Mutex() {
    static auto* const mutex = new std::recursive_mutex();
    return *mutex;
}

static std::uint32_t& LockDepth() {
    thread_local std::uint32_t depth = 0;
    return depth;
}

static std::condition_variable_any& Ngs2Waiters() {
    static auto* const waiters = new std::condition_variable_any();
    return *waiters;
}

Ngs2Lock::Ngs2Lock() {
    Ngs2Mutex().lock();
    ++LockDepth();
}

Ngs2Lock::~Ngs2Lock() {
    --LockDepth();
    Ngs2Mutex().unlock();
}

Ngs2Unlocked::Ngs2Unlocked() : depth(std::exchange(LockDepth(), 0u)) {
    for (std::uint32_t i = 0; i < depth; ++i) Ngs2Mutex().unlock();
}

Ngs2Unlocked::~Ngs2Unlocked() {
    for (std::uint32_t i = 0; i < depth; ++i) Ngs2Mutex().lock();
    LockDepth() = depth;
}

struct HeldNgs2Lock {
    std::uint32_t depth = 0;

    void unlock() {
        depth = std::exchange(LockDepth(), 0u);
        for (std::uint32_t i = 0; i < depth; ++i) Ngs2Mutex().unlock();
    }

    void lock() {
        for (std::uint32_t i = 0; i < depth; ++i) Ngs2Mutex().lock();
        LockDepth() = depth;
    }
};

struct ExclusiveState {
    std::thread::id owner;
    std::uint32_t depth = 0;
};

static ExclusiveState& Exclusive() {
    static ExclusiveState state;
    return state;
}

bool Ngs2RenderingHere(const Ngs2System& system) {
    return system.renderDepth != 0;
}

static thread_local bool jobWorker = false;

bool Ngs2SetJobWorker(bool active) {
    return std::exchange(jobWorker, active);
}

bool Ngs2ExclusiveElsewhere() {
    if (jobWorker) return false;
    return Exclusive().depth != 0 && Exclusive().owner != std::this_thread::get_id();
}

void Ngs2EnterExclusive() {
    if (jobWorker) throw std::runtime_error("NGS2: exclusive locking from a render job is not implemented");
    Exclusive().owner = std::this_thread::get_id();
    ++Exclusive().depth;
}

void Ngs2LeaveExclusive(std::uint32_t depth) {
    Exclusive().depth -= depth;
    if (Exclusive().depth != 0) return;
    Exclusive().owner = {};
    Ngs2NotifyWaiters();
}

void Ngs2WaitOnce() {
    HeldNgs2Lock held;
    Ngs2Waiters().wait(held);
}

void Ngs2NotifyWaiters() {
    Ngs2Waiters().notify_all();
}

struct SystemLockState {
    std::thread::id owner;
    std::uint32_t depth = 0;
};

static std::mutex& SystemLockMutex() {
    static auto* const mutex = new std::mutex();
    return *mutex;
}

static std::condition_variable& SystemLockReleased() {
    static auto* const released = new std::condition_variable();
    return *released;
}

static std::map<Ngs2Handle, SystemLockState>& SystemLocks() {
    static auto* const locks = new std::map<Ngs2Handle, SystemLockState>();
    return *locks;
}

static void ReleaseSystemLock(SystemLockState& state) {
    if (state.depth == 0) return;
    if (--state.depth == 0) {
        state.owner = {};
        SystemLockReleased().notify_all();
    }
}

Ngs2System* Ngs2FindSystem(Ngs2Handle handle) {
    auto& systems = Systems();
    const auto found = std::find(systems.begin(), systems.end(), reinterpret_cast<Ngs2System*>(handle));
    return found == systems.end() || (*found)->destroying ? nullptr : *found;
}

std::vector<Ngs2Rack*>& Ngs2DetachedRacks() {
    static auto* const racks = new std::vector<Ngs2Rack*>();
    return *racks;
}

Ngs2Rack* Ngs2FindRack(Ngs2Handle handle) {
    for (auto* rack : Ngs2DetachedRacks())
        if (!rack->destroying && reinterpret_cast<Ngs2Handle>(rack) == handle) return rack;
    for (auto* system : Systems()) {
        if (system->destroying) continue;
        for (auto* rack : system->racks) {
            if (!rack->destroying && reinterpret_cast<Ngs2Handle>(rack) == handle) return rack;
        }
    }
    return nullptr;
}

Ngs2Voice* Ngs2FindVoice(Ngs2Handle handle) {
    for (auto* rack : Ngs2DetachedRacks()) {
        if (rack->destroying) continue;
        for (auto& voice : rack->voices)
            if (reinterpret_cast<Ngs2Handle>(&voice) == handle) return &voice;
    }
    for (auto* system : Systems()) {
        if (system->destroying) continue;
        for (auto* rack : system->racks) {
            if (rack->destroying) continue;
            for (auto& voice : rack->voices) {
                if (reinterpret_cast<Ngs2Handle>(&voice) == handle) return &voice;
            }
        }
    }
    return nullptr;
}

void* Ngs2Place(const Ngs2ContextBufferInfo* bufferInfo, std::size_t size, std::size_t alignment) {
    if (bufferInfo == nullptr || bufferInfo->host_buffer == nullptr || bufferInfo->host_buffer_size < size) {
        throw std::invalid_argument("NGS2: the context buffer is missing or smaller than the queried size");
    }
    if (reinterpret_cast<std::uintptr_t>(bufferInfo->host_buffer) % alignment != 0) {
        throw std::invalid_argument("NGS2: the context buffer is not aligned to " + std::to_string(alignment) + " bytes");
    }
    return bufferInfo->host_buffer;
}

int Ngs2ReleaseBuffer(const Ngs2BufferAllocator& allocator, Ngs2ContextBufferInfo bufferInfo, Ngs2ContextBufferInfo* outBufferInfo) {
    if (outBufferInfo != nullptr) *outBufferInfo = allocator.free_handler == nullptr ? bufferInfo : Ngs2ContextBufferInfo{};
    return allocator.free_handler == nullptr ? SCE_NGS2_OK : allocator.free_handler(&bufferInfo);
}

static Ngs2ContextBufferInfo Allocate(const Ngs2BufferAllocator* allocator, std::size_t size) {
    if (allocator->free_handler == nullptr) APS5_INVALID_ARG_EX;
    Ngs2ContextBufferInfo bufferInfo{};
    bufferInfo.host_buffer_size = size;
    bufferInfo.user_data = allocator->user_data;
    const int result = allocator->alloc_handler(&bufferInfo);
    if (result != SCE_NGS2_OK) throw std::runtime_error("NGS2: the allocator handler failed with " + std::to_string(result));
    return bufferInfo;
}

static Ngs2SystemOption DefaultSystemOption() {
    Ngs2SystemOption option{};
    option.size = sizeof(Ngs2SystemOption);
    option.max_grain_samples = 512;
    option.num_grain_samples = 256;
    option.sample_rate = 48000;
    option.max_voice_channels = 8;
    return option;
}

static constexpr std::uint32_t MAX_GRAIN_SAMPLES = 2048;
static constexpr std::uint32_t MAX_VOICE_CHANNELS = 37;

static bool ValidGrainSamples(std::uint32_t grainSamples, std::uint32_t maxSamples) {
    return grainSamples >= MIN_GRAIN_SAMPLES && grainSamples <= maxSamples && grainSamples % MIN_GRAIN_SAMPLES == 0;
}

static bool ValidSampleRate(std::uint32_t sampleRate) {
    static constexpr std::uint32_t rates[] = {11025, 12000, 22050, 24000, 44100, 48000, 88200, 96000, 176400, 192000};
    return std::find(std::begin(rates), std::end(rates), sampleRate) != std::end(rates);
}

static int CheckSystemOption(const Ngs2SystemOption* option, Ngs2SystemOption& checked) {
    if (option == nullptr) {
        checked = DefaultSystemOption();
        return SCE_NGS2_OK;
    }
    if (option->size != sizeof(Ngs2SystemOption)) return SCE_NGS2_ERROR_INVALID_OPTION_SIZE;
    constexpr std::uint32_t knownFlags = 0x2;
    if ((option->flags & ~knownFlags) != 0) throw std::runtime_error("NGS2: system option flags " + Ngs2Hex(option->flags) + " are not implemented");
    if (!ValidGrainSamples(option->max_grain_samples, MAX_GRAIN_SAMPLES)) return SCE_NGS2_ERROR_INVALID_MAX_GRAIN_SAMPLES;
    if (!ValidGrainSamples(option->num_grain_samples, option->max_grain_samples)) return SCE_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
    if (!ValidSampleRate(option->sample_rate)) return SCE_NGS2_ERROR_INVALID_SAMPLE_RATE;
    if (option->max_voice_channels < 1 || option->max_voice_channels > MAX_VOICE_CHANNELS) return SCE_NGS2_ERROR_INVALID_NUM_CHANNELS;
    for (auto address : option->job_scheduler_options) {
        if (address == 0) continue;
        const auto& scheduler = *reinterpret_cast<const Ngs2JobSchedulerOption*>(address);
        if (scheduler.max_jobs < 2 || scheduler.max_jobs > 16384) return static_cast<int>(0x804A8421u);
    }
    checked = *option;
    return SCE_NGS2_OK;
}

struct Ngs2JobPlan {
    std::array<Ngs2ContextBufferInfo, 4> buffers{};
    std::size_t size = sizeof(Ngs2System);
};

static int PlanJobs(const Ngs2SystemOption& option, Ngs2JobPlan& plan) {
    for (std::uint32_t i = 0; i < plan.buffers.size(); ++i) {
        const auto* scheduler = reinterpret_cast<const Ngs2JobSchedulerOption*>(option.job_scheduler_options[i]);
        if (scheduler == nullptr || scheduler->query_handler == nullptr) continue;
        const int result = scheduler->query_handler(scheduler, i, &plan.buffers[i]);
        if (result < 0) return result;
        const auto bytes = plan.buffers[i].host_buffer_size;
        if (bytes > SIZE_MAX - plan.size - 7) throw std::invalid_argument("NGS2: scheduler buffer size overflows");
        plan.size = (plan.size + 7) & ~std::size_t{7};
        plan.size += bytes;
    }
    return SCE_NGS2_OK;
}

static void DestroyJobs(Ngs2System& system, bool reverse) {
    for (std::size_t n = 0; n < system.jobManagers.size(); ++n) {
        auto& manager = system.jobManagers[reverse ? system.jobManagers.size() - 1 - n : n];
        if (manager.handle != 0 && manager.option.destroy_handler != nullptr) manager.option.destroy_handler(manager.handle);
        manager.handle = 0;
    }
}

static int CreateSystem(const Ngs2SystemOption& option, const Ngs2ContextBufferInfo& bufferInfo, const Ngs2BufferAllocator& allocator, Ngs2Handle* handle) {
    static std::uint32_t nextUid = 1;
    auto* system = new (Ngs2Place(&bufferInfo, sizeof(Ngs2System), alignof(Ngs2System))) Ngs2System{};
    struct Cleanup {
        Ngs2System* system;
        ~Cleanup() { if (system != nullptr) { DestroyJobs(*system, true); std::destroy_at(system); } }
    } cleanup{system};
    std::size_t offset = sizeof(Ngs2System);
    for (std::uint32_t i = 0; i < system->jobManagers.size(); ++i) {
        const auto* scheduler = reinterpret_cast<const Ngs2JobSchedulerOption*>(option.job_scheduler_options[i]);
        if (scheduler == nullptr || scheduler->query_handler == nullptr) continue;
        Ngs2ContextBufferInfo buffer{};
        const int queried = scheduler->query_handler(scheduler, i, &buffer);
        if (queried < 0) return queried;
        if (offset > SIZE_MAX - 7) throw std::invalid_argument("NGS2: scheduler buffer size overflows");
        offset = (offset + 7) & ~std::size_t{7};
        const auto lead = reinterpret_cast<std::uintptr_t>(system) - reinterpret_cast<std::uintptr_t>(bufferInfo.host_buffer);
        if (lead > bufferInfo.host_buffer_size || offset > bufferInfo.host_buffer_size - lead || buffer.host_buffer_size > bufferInfo.host_buffer_size - lead - offset) {
            throw std::invalid_argument("NGS2: scheduler buffer does not fit the system buffer");
        }
        buffer.host_buffer = reinterpret_cast<std::uint8_t*>(system) + offset;
        offset += buffer.host_buffer_size;
        if (scheduler->init_handler == nullptr) continue;
        std::uintptr_t managerHandle = 0;
        const int result = scheduler->init_handler(scheduler, i, &buffer, &managerHandle);
        if (result < 0) return result;
        auto& manager = system->jobManagers[i];
        std::memcpy(&manager.option, scheduler, offsetof(Ngs2JobSchedulerOption, reserved_tail));
        manager.handle = managerHandle;
    }
    system->option = option;
    system->bufferInfo = bufferInfo;
    system->allocator = allocator;
    system->uid = nextUid++;
    Systems().push_back(system);
    *handle = reinterpret_cast<Ngs2Handle>(system);
    cleanup.system = nullptr;
    std::lock_guard lock(SystemLockMutex());
    SystemLocks()[*handle] = {};
    return SCE_NGS2_OK;
}

#pragma GCC visibility push(default)

extern "C" {

int APS5_VABI sceNgs2JobSchedulerResetOption(Ngs2JobSchedulerOption* option) {
    if (option == nullptr) return static_cast<int>(0x804A8012u);
    Ngs2ResetJobSchedulerOption(*option);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemQueryBufferSize(const Ngs2SystemOption* option, Ngs2ContextBufferInfo* buffer_info) {
    if (buffer_info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    std::memset(buffer_info, 0, offsetof(Ngs2ContextBufferInfo, user_data));
    Ngs2SystemOption checked{};
    const int result = CheckSystemOption(option, checked);
    if (result != SCE_NGS2_OK) return result;
    Ngs2JobPlan plan;
    const int planned = PlanJobs(checked, plan);
    if (planned < 0) return planned;
    buffer_info->host_buffer_size = plan.size;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemCreate(const Ngs2SystemOption* option, const Ngs2ContextBufferInfo* buffer_info, uintptr_t* handle) {
    if (buffer_info == nullptr || buffer_info->host_buffer == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_INFO;
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    Ngs2SystemOption checked{};
    const int result = CheckSystemOption(option, checked);
    if (result != SCE_NGS2_OK) return result;
    const Ngs2Lock lock;
    return CreateSystem(checked, *buffer_info, {}, handle);
}

int APS5_VABI sceNgs2SystemCreateWithAllocator(const Ngs2SystemOption* option, const Ngs2BufferAllocator* allocator, uintptr_t* handle) {
    if (allocator == nullptr || allocator->alloc_handler == nullptr) return SCE_NGS2_ERROR_INVALID_BUFFER_ALLOCATOR;
    if (handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    Ngs2SystemOption checked{};
    const int result = CheckSystemOption(option, checked);
    if (result != SCE_NGS2_OK) return result;
    Ngs2JobPlan plan;
    const int planned = PlanJobs(checked, plan);
    if (planned < 0) return planned;
    const auto bufferInfo = Allocate(allocator, plan.size);
    const Ngs2Lock lock;
    try {
        const int created = CreateSystem(checked, bufferInfo, *allocator, handle);
        if (created < 0) Ngs2ReleaseBuffer(*allocator, bufferInfo, nullptr);
        return created;
    } catch (...) {
        Ngs2ReleaseBuffer(*allocator, bufferInfo, nullptr);
        throw;
    }
}

int APS5_VABI sceNgs2SystemDestroy(uintptr_t system_handle, Ngs2ContextBufferInfo* buffer_info) {
    const Ngs2Lock lock;
    if (buffer_info != nullptr) *buffer_info = {};
    auto* system = Ngs2FindSystem(system_handle);
    while (system != nullptr && Ngs2ExclusiveElsewhere()) {
        Ngs2WaitOnce();
        system = Ngs2FindSystem(system_handle);
    }
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (Ngs2ControlActive(*system)) throw std::runtime_error("NGS2: destroying a system from its UserFx2 control handler is not implemented");
    if (Ngs2RenderingHere(*system)) throw std::runtime_error("NGS2: destroying a system from its own render (a voice callback) is not implemented");
    while (system != nullptr && system->voiceAccesses != 0) {
        Ngs2WaitOnce();
        system = Ngs2FindSystem(system_handle);
    }
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    auto& detached = Ngs2DetachedRacks();
    detached.reserve(detached.size() + system->racks.size());
    system->destroying = true;
    for (auto* rack : system->racks) {
        for (auto& voice : rack->voices) {
            for (auto& port : voice.ports) {
                if (port.dest != nullptr) port.detachedDest = reinterpret_cast<Ngs2Handle>(port.dest);
                port.dest = nullptr;
            }
        }
        rack->system = nullptr;
        detached.push_back(rack);
    }
    system->racks.clear();
    Ngs2NotifyWaiters();
    std::erase(Systems(), system);
    {
        std::lock_guard locks(SystemLockMutex());
        SystemLocks().erase(system_handle);
        SystemLockReleased().notify_all();
    }
    DestroyJobs(*system, false);
    const auto bufferInfo = system->bufferInfo;
    const auto allocator = system->allocator;
    std::destroy_at(system);
    return Ngs2ReleaseBuffer(allocator, bufferInfo, buffer_info);
}

int APS5_VABI sceNgs2SystemGetInfo(uintptr_t system_handle, Ngs2SystemInfo* info, size_t info_size) {
    if (info == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (info_size != sizeof(Ngs2SystemInfo)) return SCE_NGS2_ERROR_INVALID_OUT_SIZE;
    const Ngs2Lock lock;
    const auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    *info = {};
    std::memcpy(info->name, system->option.name, sizeof(info->name));
    info->system_handle = system_handle;
    info->buffer_info = system->bufferInfo;
    info->uid = system->uid;
    info->min_grain_samples = MIN_GRAIN_SAMPLES;
    info->max_grain_samples = system->option.max_grain_samples;
    info->state_flags = 1;
    info->rack_count = static_cast<std::uint32_t>(system->racks.size());
    info->render_count = system->renderCount;
    info->sample_rate = system->option.sample_rate;
    info->num_grain_samples = system->option.num_grain_samples;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemSetGrainSamples(uintptr_t system_handle, uint32_t num_samples) {
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (system->jobsExecuting) throw std::runtime_error("NGS2: changing grain size from an active job handler is not implemented");
    if (!ValidGrainSamples(num_samples, system->option.max_grain_samples)) return SCE_NGS2_ERROR_INVALID_NUM_GRAIN_SAMPLES;
    system->option.num_grain_samples = num_samples;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemSetSampleRate(uintptr_t system_handle, uint32_t sample_rate) {
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (system->jobsExecuting) throw std::runtime_error("NGS2: changing sample rate from an active job handler is not implemented");
    if (!ValidSampleRate(sample_rate)) return SCE_NGS2_ERROR_INVALID_SAMPLE_RATE;
    for (const auto* rack : system->racks) {
        for (const auto& voice : rack->voices) {
            for (const auto& filter : voice.filters) {
                if (filter.enabled && sample_rate != system->option.sample_rate) {
                    throw std::runtime_error("NGS2: changing the sample rate under an enabled sampler filter is not implemented");
                }
            }
        }
    }
    system->option.sample_rate = sample_rate;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemSetUserData(uintptr_t system_handle, uintptr_t user_data) {
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    system->userData = user_data;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemGetUserData(uintptr_t system_handle, uintptr_t* user_data) {
    const Ngs2Lock lock;
    const auto* system = Ngs2FindSystem(system_handle);
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (user_data == nullptr) APS5_INVALID_ARG_EX;
    *user_data = system->userData;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemLock(uintptr_t system_handle) {
    if (jobWorker) throw std::runtime_error("NGS2: system locking from a render job is not implemented");
    const auto self = std::this_thread::get_id();
    std::unique_lock lock(SystemLockMutex());
    auto& locks = SystemLocks();
    SystemLockReleased().wait(lock, [&] {
        const auto found = locks.find(system_handle);
        return found == locks.end() || found->second.depth == 0 || found->second.owner == self;
    });
    const auto found = locks.find(system_handle);
    if (found == locks.end()) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    found->second.owner = self;
    found->second.depth++;
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemUnlock(uintptr_t system_handle) {
    std::lock_guard lock(SystemLockMutex());
    const auto found = SystemLocks().find(system_handle);
    if (found == SystemLocks().end()) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    ReleaseSystemLock(found->second);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemRender(uintptr_t system_handle, const Ngs2RenderBufferInfo* buffer_info, uint32_t num_buffer_info) {
    if (jobWorker) throw std::runtime_error("NGS2: recursive system rendering from a render job is not implemented");
    const Ngs2Lock lock;
    auto* system = Ngs2FindSystem(system_handle);
    while (system != nullptr && Ngs2ExclusiveElsewhere()) {
        Ngs2WaitOnce();
        system = Ngs2FindSystem(system_handle);
    }
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (Ngs2ControlActive(*system)) throw std::runtime_error("NGS2: rendering a system from its UserFx2 control handler is not implemented");
    while (system != nullptr && system->controlCallbacks != 0) {
        Ngs2WaitOnce();
        system = Ngs2FindSystem(system_handle);
    }
    if (system == nullptr) return SCE_NGS2_ERROR_INVALID_SYSTEM_HANDLE;
    if (num_buffer_info > 31) return SCE_NGS2_ERROR_INVALID_NUM_RENDER_BUFFERS;
    if (buffer_info == nullptr || num_buffer_info == 0) APS5_INVALID_ARG_EX;
    for (std::uint32_t i = 0; i < num_buffer_info; i++) {
        const auto waveform_type = buffer_info[i].waveform_type;
        if (waveform_type != SCE_NGS2_WAVEFORM_TYPE_PCM_I16L && waveform_type != SCE_NGS2_WAVEFORM_TYPE_PCM_I16B &&
            waveform_type != SCE_NGS2_WAVEFORM_TYPE_PCM_F32L && waveform_type != SCE_NGS2_WAVEFORM_TYPE_PCM_F32B) {
            return SCE_NGS2_ERROR_INVALID_WAVEFORM_TYPE;
        }
        if (buffer_info[i].buffer == nullptr) return SCE_NGS2_ERROR_INVALID_RENDER_BUFFER;
    }
    SystemLockState* state = nullptr;
    {
        const auto self = std::this_thread::get_id();
        std::lock_guard locks(SystemLockMutex());
        auto& found = SystemLocks().at(system_handle);
        if (found.depth != 0 && found.owner != self) {
            Ngs2RenderSilence(*system, buffer_info, num_buffer_info);
            return SCE_NGS2_OK;
        }
        found.owner = self;
        found.depth++;
        state = &found;
    }
    Ngs2EnterExclusive();
    system->renderDepth++;
    struct Release {
        SystemLockState* state;
        Ngs2System* system;
        ~Release() {
            system->renderDepth--;
            Ngs2LeaveExclusive();
            Ngs2NotifyWaiters();
            std::lock_guard locks(SystemLockMutex());
            ReleaseSystemLock(*state);
        }
    } release{state, system};
    Ngs2RenderSystem(*system, buffer_info, num_buffer_info);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2SystemResetOption(Ngs2SystemOption* option) {
    if (option == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    *option = DefaultSystemOption();
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2ReportRegisterHandler(uint32_t report_type, Ngs2ReportHandler handler, uintptr_t user_data, Ngs2Handle* out_handle) {
    if (out_handle == nullptr) return SCE_NGS2_ERROR_INVALID_OUT_ADDRESS;
    if (handler == nullptr) APS5_INVALID_ARG_EX;
    const Ngs2Lock lock;
    auto* registration = new Ngs2ReportRegistration{report_type, handler, user_data};
    ReportRegistrations().push_back(registration);
    *out_handle = reinterpret_cast<Ngs2Handle>(registration);
    return SCE_NGS2_OK;
}

int APS5_VABI sceNgs2ReportUnregisterHandler(Ngs2Handle report_handle) {
    const Ngs2Lock lock;
    auto& registrations = ReportRegistrations();
    const auto found = std::find(registrations.begin(), registrations.end(), reinterpret_cast<Ngs2ReportRegistration*>(report_handle));
    if (found == registrations.end()) throw std::runtime_error("NGS2: unregistering a report handler that is not registered");
    Ngs2ReportRegistration* registration = *found;
    registrations.erase(found);
    delete registration;
    return SCE_NGS2_OK;
}

}

#pragma GCC visibility pop
