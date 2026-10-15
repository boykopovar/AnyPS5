#ifndef CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWAHEAD_HPP
#define CORE_LIBS_PRX_LIBSCEAGCDRIVER_EXECUTION_INCLUDE_DRIVER_DRAW_DRAWAHEAD_HPP

#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Queues/Submission.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgcDriver/Execution/include/Pm4.hpp"
#include "prx/libSceAgcDriver/Execution/include/QueueState.hpp"
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <vector>

namespace AgcDriver::DriverDetail {

class Driver;

enum class DrawPrecheckOutcome : std::uint8_t { Proceed, Nothing, Rejected, MetadataPass };

DrawPrecheckOutcome PrecheckDrawState(const QueueState& queue, const Pm4::DrawParameters& drawParameters, bool primitiveListRestart, std::string& rejected);
bool TraceIndirectDraws();

struct PublishedDraw {
    struct RegisterCheck {
        Graphics::RegisterBank bank = Graphics::RegisterBank::Context;
        std::uint32_t offset = 0;
        bool present = false;
        std::uint32_t value = 0;
    };
    std::size_t offset = 0;
    bool certain = false;
    DrawPrecheckOutcome precheck = DrawPrecheckOutcome::Rejected;
    std::uint64_t key = 0;
    std::uint64_t shape = 0;
    std::uint64_t deviceSerial = 0;
    Pm4::DrawParameters parameters{};
    std::uint64_t indexBase = 0;
    std::uint32_t indexType = 0;
    std::uint32_t instanceCount = 0;
    std::vector<RegisterCheck> checks;
};

class DrawScan {
public:
    static constexpr std::size_t MaxRegisterChecks = 64;
    struct Draw {
        std::size_t offset;
        std::span<const std::uint32_t> packet;
        const QueueState& state;
        bool certain;
    };
    DrawScan(std::span<const std::uint32_t> commands, const std::map<std::size_t, std::vector<std::uint32_t>>& registerLists, const std::map<std::size_t, std::size_t>& conditionalEnds, QueueState& state);
    bool Next(const std::function<bool(const Draw&)>& draw);
    void Capture(std::vector<PublishedDraw::RegisterCheck>& checks) const;
    static bool Matches(const PublishedDraw& record, const QueueState& queue);

private:
    void write(Graphics::RegisterBank bank, std::uint32_t offset, bool maybe);
    void clearContext();
    void track(std::span<const std::uint32_t> packet, std::uint32_t opcode, bool maybe, const std::vector<std::uint32_t>* pairs);

    std::span<const std::uint32_t> commands;
    const std::map<std::size_t, std::vector<std::uint32_t>>& registerLists;
    const std::map<std::size_t, std::size_t>& conditionalEnds;
    QueueState& state;
    std::size_t cursor = 0;
    std::size_t maybeUntil = 0;
    std::set<std::pair<Graphics::RegisterBank, std::uint32_t>> divergent;
    std::set<std::pair<Graphics::RegisterBank, std::uint32_t>> saved;
    bool lost = false;
};

struct DrawAheadCounters {
    std::uint64_t published = 0;
    std::uint64_t used = 0;
    std::uint64_t compiled = 0;
    std::uint64_t dropped = 0;
};

class DrawAhead {
public:
    class Speculation final : public GuestMemory::ISpeculativeReadGate {
    public:
        Speculation(DrawAhead& owner, std::uint64_t serial, std::size_t offset, bool compile);
        void Enter() override;
        void Leave() override;
        bool Passed() const;
        bool Commit(const std::function<void()>& action);

    private:
        DrawAhead& owner;
        std::uint64_t serial;
        std::size_t offset;
        bool compile;
    };

    DrawAhead(Driver& driver, std::size_t workers);
    ~DrawAhead();
    DrawAhead(const DrawAhead&) = delete;
    DrawAhead& operator=(const DrawAhead&) = delete;

    static void SetPinnedReadHook(void (*hook)(bool compile));
    static void SetCompileAhead(bool enabled);
    static std::size_t Workers();
    static bool HasDirectDraws(std::span<const std::uint32_t> commands);
    static bool CompilesAhead(const Graphics::State& state, const Pm4::DrawParameters& parameters);
    void Post(const Submission& submission, const QueueState& state);
    void Progress(std::uint64_t serial, std::size_t cursor);
    void Skip(std::uint64_t serial, std::size_t end);
    void Retire(std::uint64_t serial);
    const PublishedDraw* Find(const Submission& submission, std::span<const std::uint32_t> packet, const QueueState& queue, std::uint64_t deviceSerial);

private:
    struct Publication {
        std::uint64_t serial = 0;
        const std::uint32_t* commands = nullptr;
        const ShaderRegistry* shaders = nullptr;
        std::unique_ptr<PublishedDraw[]> records;
        std::size_t capacity = 0;
        std::atomic<std::size_t> published{0};
        std::size_t readCursor = 0;
    };

    struct Scan {
        std::uint64_t serial = 0;
        std::vector<std::uint32_t> commands;
        std::map<std::size_t, std::vector<std::uint32_t>> registerLists;
        std::map<std::size_t, std::size_t> conditionalEnds;
        std::shared_ptr<const ShaderRegistry> shaders;
        std::unique_ptr<QueueState> state;
        std::shared_ptr<Publication> publication;
    };

    struct Job {
        std::uint64_t serial = 0;
        std::size_t offset = 0;
        std::uint64_t key = 0;
        std::uint64_t shape = 0;
        std::shared_ptr<const DrawDecode> decode;
        Pm4::DrawParameters parameters{};
        std::shared_ptr<VulkanDevice> device;
    };

    bool passed(std::uint64_t serial, std::size_t offset);
    bool pin(std::uint64_t serial, std::size_t offset);
    void unpin(std::uint64_t serial, std::size_t offset);
    void pass(std::uint64_t serial, std::size_t end);
    void scanLoop(std::stop_token token);
    void workLoop(std::stop_token token);
    void scan(Scan& work, std::stop_token token);
    void queueJob(Job job);

    Driver& driver;
    std::size_t workerCount;
    std::mutex mutex;
    std::condition_variable_any scansReady;
    std::condition_variable_any jobsReady;
    std::deque<Scan> scans;
    bool scanning = false;
    std::deque<Job> jobs;
    std::mutex gateMutex;
    std::condition_variable gateLeft;
    std::uint64_t passedSerial = 0;
    std::size_t passedEnd = 0;
    std::vector<std::pair<std::uint64_t, std::size_t>> pinned;
    std::shared_ptr<Publication> current;
    std::vector<std::jthread> threads;
};
}

#endif
