#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Graphics/include/RuntimeSwitches.hpp"
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <iostream>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawFinisher;
using AgcDriver::DriverDetail::DrawPipeline;
using AgcDriver::Graphics::RuntimeSwitch;
namespace RuntimeSwitches = AgcDriver::Graphics::RuntimeSwitches;
using namespace std::chrono_literals;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::uint32_t Commits = 3000;
constexpr std::uint64_t Base = 0x7000'0000'0000ull;
constexpr std::size_t Stride = 64;

enum class Mode { Spinning, Blocking, Flipping };

const char* Name(Mode mode) {
    return mode == Mode::Spinning ? "spinning" : mode == Mode::Blocking ? "blocking" : "flipping";
}

class Flipper {
public:
    explicit Flipper(Mode mode) {
        RuntimeSwitches::Assign(RuntimeSwitch::NoSpinHandoff, mode == Mode::Blocking);
        if (mode != Mode::Flipping) return;
        thread = std::thread([this] {
            bool blocking = false;
            while (!stop.load(std::memory_order_acquire)) {
                blocking = !blocking;
                RuntimeSwitches::Assign(RuntimeSwitch::NoSpinHandoff, blocking);
                std::this_thread::sleep_for(50us);
            }
        });
    }
    ~Flipper() {
        stop.store(true, std::memory_order_release);
        if (thread.joinable()) thread.join();
        RuntimeSwitches::Assign(RuntimeSwitch::NoSpinHandoff, false);
    }

private:
    std::atomic<bool> stop{false};
    std::thread thread;
};

class Contender {
public:
    Contender() {
        thread = std::thread([this] {
            auto& pipeline = DrawPipeline::Queue0();
            std::uint32_t index = 0;
            while (!stop.load(std::memory_order_acquire)) {
                const auto address = Base + static_cast<std::uint64_t>(index % Commits) * Stride;
                if (const auto label = pipeline.PendingLabel(address, 4); label.has_value() && *label != index % Commits) wrongLabels.fetch_add(1, std::memory_order_relaxed);
                static_cast<void>(pipeline.Overlaps(address, Stride));
                static_cast<void>(DrawPipeline::HeldReads(address, Stride));
                static_cast<void>(DrawPipeline::QueuedItems());
                DrawFinisher::Get().ReleaseRan();
                ++index;
            }
        });
    }
    ~Contender() { Stop(); }
    void Stop() {
        stop.store(true, std::memory_order_release);
        if (thread.joinable()) thread.join();
    }
    std::uint64_t WrongLabels() const { return wrongLabels.load(std::memory_order_relaxed); }

private:
    std::atomic<bool> stop{false};
    std::atomic<std::uint64_t> wrongLabels{0};
    std::thread thread;
};

std::vector<std::byte> LabelBytes(std::uint32_t value) {
    std::vector<std::byte> bytes(sizeof(value));
    std::memcpy(bytes.data(), &value, sizeof(value));
    return bytes;
}

void CommitsKeepTheirOrder(Mode mode) {
    auto& pipeline = DrawPipeline::Queue0();
    std::vector<std::uint32_t> committed;
    committed.reserve(Commits);
    {
        const Flipper flipper(mode);
        Contender contender;
        for (std::uint32_t index = 0; index < Commits; ++index) {
            const auto address = Base + static_cast<std::uint64_t>(index) * Stride;
            pipeline.Enqueue([&committed, index] { committed.push_back(index); }, {{address, address + Stride}}, address, LabelBytes(index), {{address, address + Stride}});
        }
        pipeline.Drain(DrawPipeline::DrainReason::Submission);
        contender.Stop();
        Require(contender.WrongLabels() == 0, std::string(Name(mode)) + ": a queued label read back another commit's value");
    }
    Require(committed.size() == Commits, std::string(Name(mode)) + ": the committer ran " + std::to_string(committed.size()) + " of " + std::to_string(Commits) + " commits");
    for (std::uint32_t index = 0; index < Commits; ++index) Require(committed[index] == index, std::string(Name(mode)) + ": commit " + std::to_string(index) + " ran out of order");
    Require(!pipeline.Overlaps(Base, Commits * Stride), std::string(Name(mode)) + ": a drained pipeline still holds queued writes");
}

void FinishedJobsCommitInOrder(Mode mode) {
    auto& finisher = DrawFinisher::Get();
    auto& pipeline = DrawPipeline::Queue0();
    std::vector<std::uint32_t> finished;
    std::vector<std::uint32_t> committed;
    finished.reserve(Commits);
    committed.reserve(Commits);
    {
        const Flipper flipper(mode);
        Contender contender;
        for (std::uint32_t index = 0; index < Commits; ++index) {
            const auto address = Base + static_cast<std::uint64_t>(index) * Stride;
            DrawFinisher::Pending held;
            held.unknown = false;
            held.key = index;
            held.writes = {{address, address + Stride}};
            finisher.Enqueue([&finished, &committed, &pipeline, index, address] {
                finished.push_back(index);
                if (index % 64 == 0) {
                    const auto until = std::chrono::steady_clock::now() + 20us;
                    while (std::chrono::steady_clock::now() < until) {}
                }
                pipeline.Enqueue([&committed, index] { committed.push_back(index); }, {{address, address + Stride}});
            }, std::move(held));
            static_cast<void>(finisher.PendingOverlaps(address, address + Stride));
            static_cast<void>(finisher.PendingKey(index, 0));
        }
        pipeline.Drain(DrawPipeline::DrainReason::Submission);
        contender.Stop();
    }
    Require(finished.size() == Commits && committed.size() == Commits, std::string(Name(mode)) + ": not every finisher job reached the committer");
    for (std::uint32_t index = 0; index < Commits; ++index) Require(finished[index] == index && committed[index] == index, std::string(Name(mode)) + ": finisher job " + std::to_string(index) + " ran or committed out of order");
    Require(!DrawFinisher::Busy() && !pipeline.Busy(), std::string(Name(mode)) + ": a drain left work behind");
}

class Gate {
public:
    void Open() { open.store(true, std::memory_order_release); }
    void Pass() const {
        while (!open.load(std::memory_order_acquire)) std::this_thread::yield();
    }

private:
    std::atomic<bool> open{false};
};

void AwaitThread(std::thread& thread, const std::atomic<bool>& done, const std::string& failure) {
    const auto until = std::chrono::steady_clock::now() + 5s;
    while (!done.load(std::memory_order_acquire) && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
    if (!done.load(std::memory_order_acquire)) {
        std::cerr << failure << '\n';
        std::_Exit(1);
    }
    thread.join();
}

void RoomWaiterWakes(Mode mode) {
    auto& finisher = DrawFinisher::Get();
    const Flipper flipper(mode);
    constexpr std::size_t FinisherDepth = 16;
    Gate gate;
    std::atomic<std::size_t> enqueued{0};
    std::atomic<bool> done{false};
    std::thread producer([&] {
        for (std::size_t job = 0; job <= FinisherDepth; ++job) {
            finisher.Enqueue([&gate] { gate.Pass(); });
            enqueued.fetch_add(1, std::memory_order_release);
        }
        done.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(30ms);
    const auto held = enqueued.load(std::memory_order_acquire);
    gate.Open();
    AwaitThread(producer, done, std::string(Name(mode)) + ": an enqueue waiting for room in the finisher was not woken");
    finisher.Drain();
    Require(held == FinisherDepth, std::string(Name(mode)) + ": " + std::to_string(held) + " jobs entered a finisher of depth 16 while it was held");
    Require(!DrawFinisher::Busy(), std::string(Name(mode)) + ": the finisher did not run every held job");
}

void IdleWaiterWakes(Mode mode, bool settle) {
    auto& finisher = DrawFinisher::Get();
    const Flipper flipper(mode);
    const std::string waiter = std::string(Name(mode)) + (settle ? ": a settle" : ": a drain");
    Gate gate;
    finisher.Enqueue([&gate] { gate.Pass(); });
    std::atomic<bool> done{false};
    std::thread thread([&] {
        if (settle) finisher.Settle();
        else finisher.Drain();
        done.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(30ms);
    const bool early = done.load(std::memory_order_acquire);
    gate.Open();
    AwaitThread(thread, done, waiter + " was not woken when the finisher emptied");
    finisher.Drain();
    Require(!early, waiter + " returned while a finisher job was held");
}

void EveryWaiterWakes(Mode mode) {
    auto& finisher = DrawFinisher::Get();
    const Flipper flipper(mode);
    constexpr std::size_t FinisherDepth = 16;
    Gate gate;
    std::atomic<std::size_t> enqueued{0};
    std::atomic<bool> produced{false};
    std::atomic<bool> settled{false};
    std::atomic<bool> drained{false};
    std::thread producer([&] {
        for (std::size_t job = 0; job <= FinisherDepth; ++job) {
            finisher.Enqueue([&gate] { gate.Pass(); });
            enqueued.fetch_add(1, std::memory_order_release);
        }
        produced.store(true, std::memory_order_release);
    });
    while (enqueued.load(std::memory_order_acquire) < FinisherDepth) std::this_thread::yield();
    std::thread settler([&] {
        finisher.Settle();
        settled.store(true, std::memory_order_release);
    });
    std::thread drainer([&] {
        finisher.Drain();
        drained.store(true, std::memory_order_release);
    });
    std::this_thread::sleep_for(30ms);
    const bool early = settled.load(std::memory_order_acquire) || drained.load(std::memory_order_acquire);
    gate.Open();
    AwaitThread(producer, produced, std::string(Name(mode)) + ": an enqueue waiting for room beside a settle and a drain was not woken");
    AwaitThread(settler, settled, std::string(Name(mode)) + ": a settle beside an enqueue and a drain was not woken");
    AwaitThread(drainer, drained, std::string(Name(mode)) + ": a drain beside an enqueue and a settle was not woken");
    finisher.Drain();
    Require(!early, std::string(Name(mode)) + ": a settle or a drain returned while finisher jobs were held");
}

}

int main() {
    try {
        if (DrawPipeline::Depth() == 0) return 77;
        Require(RuntimeSwitches::Find("APS5_NO_SPIN_HANDOFF") == RuntimeSwitch::NoSpinHandoff, "APS5_NO_SPIN_HANDOFF is not a runtime switch");
        for (const auto mode : {Mode::Spinning, Mode::Blocking, Mode::Flipping}) {
            CommitsKeepTheirOrder(mode);
            FinishedJobsCommitInOrder(mode);
            RoomWaiterWakes(mode);
            IdleWaiterWakes(mode, true);
            IdleWaiterWakes(mode, false);
            EveryWaiterWakes(mode);
        }
        std::cout << "draw handoff spin tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
