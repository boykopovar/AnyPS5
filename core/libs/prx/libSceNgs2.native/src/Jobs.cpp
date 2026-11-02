#include <condition_variable>
#include <deque>
#include <exception>
#include <map>
#include <mutex>
#include <new>
#include <vector>
#include <utility>

#include "SceTypes.hpp"
#include "Ngs2Internal.hpp"
#include "prx/libkernel/include/Api.hpp"

struct Ngs2HostJobPool {
    struct Job { Ngs2JobHandler handler; void* context; std::uint32_t index; std::uint64_t stage; };
    std::mutex mutex;
    std::condition_variable changed;
    std::deque<Job> jobs;
    std::map<std::uint64_t, std::size_t> pending;
    std::vector<Pthread> workers;
    std::exception_ptr error;
    std::uint64_t enqueueStage = 0;
    std::uint64_t runningStage = 0;
    std::uint32_t capacity = 0;
    std::size_t active = 0;
    bool stopping = false;

    void Advance() {
        while (runningStage < enqueueStage && !pending.contains(runningStage)) ++runningStage;
    }
};

static void* APS5_VABI HostJobWorker(void* context) {
    auto& pool = *static_cast<Ngs2HostJobPool*>(context);
    std::unique_lock lock(pool.mutex);
    for (;;) {
        pool.changed.wait(lock, [&] { return pool.stopping || (!pool.jobs.empty() && pool.jobs.front().stage == pool.runningStage); });
        if (pool.stopping && pool.jobs.empty()) return nullptr;
        if (pool.jobs.empty() || pool.jobs.front().stage != pool.runningStage) continue;
        const auto job = pool.jobs.front();
        pool.jobs.pop_front();
        ++pool.active;
        pool.changed.notify_all();
        lock.unlock();
        std::exception_ptr error;
        try { job.handler(job.context, job.index); }
        catch (...) { error = std::current_exception(); }
        lock.lock();
        if (error && !pool.error) pool.error = error;
        --pool.active;
        if (--pool.pending.at(job.stage) == 0) pool.pending.erase(job.stage);
        pool.Advance();
        pool.changed.notify_all();
    }
}

static int CheckHostJobs(const Ngs2JobSchedulerOption* option) {
    if (option == nullptr) return static_cast<int>(0x804A8012u);
    if (option->max_jobs < 2 || option->max_jobs > 16384) return static_cast<int>(0x804A8421u);
    if (option->num_workers < 2 || option->num_workers > 28) return static_cast<int>(0x804A8422u);
    return SCE_NGS2_OK;
}

static int APS5_VABI QueryHostJobs(const Ngs2JobSchedulerOption* option, std::uint32_t, Ngs2ContextBufferInfo* buffer) {
    const int result = CheckHostJobs(option);
    if (result != SCE_NGS2_OK) return result;
    if (buffer != nullptr) {
        *buffer = {};
        buffer->host_buffer_size = sizeof(Ngs2HostJobPool) + alignof(Ngs2HostJobPool) - 1;
    }
    return SCE_NGS2_OK;
}

static void StopHostJobs(Ngs2HostJobPool& pool) {
    {
        std::lock_guard lock(pool.mutex);
        pool.stopping = true;
        pool.changed.notify_all();
    }
    for (auto worker : pool.workers) scePthreadJoin(worker, nullptr);
    pool.workers.clear();
}

static int APS5_VABI InitHostJobs(const Ngs2JobSchedulerOption* option, std::uint32_t, Ngs2ContextBufferInfo* buffer, std::uintptr_t* handle) {
    const int checked = CheckHostJobs(option);
    if (checked != SCE_NGS2_OK) return checked;
    if (handle == nullptr) throw std::invalid_argument("NGS2: a job manager requires an output handle");
    auto* pool = new (Ngs2Place(buffer, sizeof(Ngs2HostJobPool), alignof(Ngs2HostJobPool))) Ngs2HostJobPool{};
    pool->capacity = option->max_jobs;
    PthreadAttr attr = nullptr;
    struct Cleanup {
        Ngs2HostJobPool* pool;
        PthreadAttr& attr;
        ~Cleanup() {
            if (attr != nullptr) scePthreadAttrDestroy(&attr);
            if (pool != nullptr) { StopHostJobs(*pool); std::destroy_at(pool); }
        }
    } cleanup{pool, attr};
    const KernelSchedParam priority{static_cast<int>(option->reserved[2])};
    if (scePthreadAttrInit(&attr) != 0) return -1;
    scePthreadAttrSetstacksize(&attr, option->reserved[1] != 0 ? option->reserved[1] : 0x4000);
    scePthreadAttrSetinheritsched(&attr, 0);
    scePthreadAttrSetschedpolicy(&attr, 1);
    scePthreadAttrSetschedparam(&attr, &priority);
    pool->workers.reserve(option->num_workers);
    for (std::uint32_t i = 0; i < option->num_workers; ++i) {
        Pthread worker = nullptr;
        if (scePthreadCreate(&worker, &attr, HostJobWorker, pool, "sce-ngs2-lo-jobw") != 0) return -1;
        pool->workers.push_back(worker);
    }
    *handle = reinterpret_cast<std::uintptr_t>(pool);
    cleanup.pool = nullptr;
    return SCE_NGS2_OK;
}

static int APS5_VABI SubmitHostJobs(std::uintptr_t handle, Ngs2JobHandler handler, void* context, std::uint32_t count) {
    if (static_cast<std::int32_t>(count) <= 0) return SCE_NGS2_OK;
    auto& pool = *reinterpret_cast<Ngs2HostJobPool*>(handle);
    std::unique_lock lock(pool.mutex);
    for (std::uint32_t i = 0; i < count; ++i) {
        pool.changed.wait(lock, [&] { return pool.jobs.size() < pool.capacity - 1; });
        pool.jobs.push_back({handler, context, i, pool.enqueueStage});
        ++pool.pending[pool.enqueueStage];
        pool.changed.notify_all();
    }
    return SCE_NGS2_OK;
}

static int APS5_VABI FlushHostJobs(std::uintptr_t handle) {
    auto& pool = *reinterpret_cast<Ngs2HostJobPool*>(handle);
    std::lock_guard lock(pool.mutex);
    ++pool.enqueueStage;
    pool.Advance();
    pool.changed.notify_all();
    return SCE_NGS2_OK;
}

static int APS5_VABI JoinHostJobs(std::uintptr_t handle) {
    auto& pool = *reinterpret_cast<Ngs2HostJobPool*>(handle);
    std::unique_lock lock(pool.mutex);
    pool.changed.wait(lock, [&] { return pool.pending.empty() && pool.active == 0; });
    const auto error = std::exchange(pool.error, {});
    lock.unlock();
    if (error) std::rethrow_exception(error);
    return SCE_NGS2_OK;
}

static int APS5_VABI DestroyHostJobs(std::uintptr_t handle) {
    if (handle == 0) return static_cast<int>(0x804A8420u);
    auto& pool = *reinterpret_cast<Ngs2HostJobPool*>(handle);
    std::exception_ptr error;
    try { JoinHostJobs(handle); } catch (...) { error = std::current_exception(); }
    StopHostJobs(pool);
    std::destroy_at(&pool);
    if (error) std::rethrow_exception(error);
    return SCE_NGS2_OK;
}

void Ngs2ResetJobSchedulerOption(Ngs2JobSchedulerOption& value) {
    auto* option = &value;
    *option = {};
    option->size = sizeof(*option);
    option->query_handler = QueryHostJobs;
    option->init_handler = InitHostJobs;
    option->destroy_handler = DestroyHostJobs;
    option->submit_handler = SubmitHostJobs;
    option->flush_handler = FlushHostJobs;
    option->join_handler = JoinHostJobs;
    option->reserved[0] = 32;
    option->reserved[1] = 0x20000;
    option->reserved[2] = 700;
    option->num_workers = 4;
    option->max_jobs = 1024;
}
