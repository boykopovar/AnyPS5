#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <thread>
#include <vector>

using NKWebKit::WtfAbi::ParallelEnvironment;
using NKWebKit::WtfAbi::ParallelEnvironmentThread;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "parallel environment check failed at line %d\n", line);
        std::fflush(stderr);
        std::_Exit(1);
    }
}
#define Require(value) Check((value), __LINE__)

constexpr auto construct = &_ZN3WTF19ParallelEnvironmentC1EPFvPvEmi;
constexpr auto execute = &_ZN3WTF19ParallelEnvironment7executeEPv;

struct Parameter {
    std::atomic<int> calls{0};
    bool onMainThread = false;
    std::thread::id thread;
    std::function<void()> body;
    unsigned char spacing[24] = {};
};

static void APS5_VABI Work(void* raw) {
    auto* parameter = static_cast<Parameter*>(raw);
    parameter->onMainThread = _ZN3WTF12isMainThreadEv();
    parameter->thread = std::this_thread::get_id();
    ++parameter->calls;
    if (parameter->body) parameter->body();
}

template<typename TFunction>
static bool ThrowsInvalidArgument(TFunction&& function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

struct Environment {
    alignas(16) unsigned char bytes[96];

    explicit Environment(int requested, std::size_t stride = sizeof(Parameter)) {
        std::memset(bytes, 0xA5, sizeof(bytes));
        construct(get(), &Work, stride, requested);
        Require(get()->threadFunction == &Work && get()->sizeOfParameter == stride);
        Require(get()->numberOfJobs == static_cast<int>(get()->threads.size) + 1);
        Require(get()->threads.capacity == get()->threads.size && (get()->threads.buffer != nullptr) == (get()->threads.size != 0));
        Require(Untouched());
    }

    ~Environment() {
        for (unsigned index = 0; index < get()->threads.size; ++index) {
            Require(get()->threads.buffer[index]->refCount == 2);
            --get()->threads.buffer[index]->refCount;
        }
        if (get()->threads.buffer) _ZN3WTF8fastFreeEPv(get()->threads.buffer);
    }

    ParallelEnvironment* get() {
        return reinterpret_cast<ParallelEnvironment*>(bytes);
    }

    int Jobs() {
        return get()->numberOfJobs;
    }

    ParallelEnvironmentThread* Thread(unsigned index) {
        Require(index < get()->threads.size);
        return get()->threads.buffer[index];
    }

    bool Untouched() const {
        for (std::size_t index = 20; index < sizeof(bytes); ++index)
            if ((index < 24 || index >= 40) && bytes[index] != 0xA5) return false;
        return true;
    }

    void Run(std::vector<Parameter>& parameters) {
        Require(parameters.size() == static_cast<std::size_t>(Jobs()));
        execute(get(), parameters.data());
        std::set<std::thread::id> threads;
        for (std::size_t index = 0; index < parameters.size(); ++index) {
            threads.insert(parameters[index].thread);
            Require(parameters[index].onMainThread == (index + 1 == parameters.size()));
        }
        Require(threads.size() == parameters.size());
    }

    std::vector<Parameter> Run() {
        std::vector<Parameter> parameters(static_cast<std::size_t>(Jobs()));
        Run(parameters);
        for (const Parameter& parameter : parameters) Require(parameter.calls.load() == 1);
        return parameters;
    }
};

static int cores = 0;

static void TestOneJob() {
    Environment environment(1);
    Require(environment.Jobs() == 1 && environment.get()->threads.size == 0);
    const std::vector<Parameter> parameters = environment.Run();
    Require(parameters[0].thread == std::this_thread::get_id());
}

static void TestTwoJobs() {
    if (cores < 2) return;
    ParallelEnvironmentThread* worker = nullptr;
    std::thread::id workerThread;
    {
        Environment environment(2);
        Require(environment.Jobs() == 2);
        worker = environment.Thread(0);
        std::vector<Parameter> parameters(2);
        environment.Run(parameters);
        Require(parameters[0].calls.load() == 1 && parameters[1].calls.load() == 1);
        workerThread = parameters[0].thread;
        environment.Run(parameters);
        Require(parameters[0].calls.load() == 2 && parameters[1].calls.load() == 2 && parameters[0].thread == workerThread);
    }
    Require(worker->refCount == 1);
    Environment again(2);
    Require(again.Jobs() == 2 && again.Thread(0) == worker);
    Require(again.Run()[0].thread == workerThread);
}

static void TestTheWorkerFinishesLast() {
    if (cores < 2) return;
    Environment environment(2);
    std::vector<Parameter> parameters(2);
    std::atomic<bool> finished{false};
    parameters[0].body = [&] {
        std::this_thread::sleep_for(std::chrono::milliseconds(150));
        finished = true;
    };
    environment.Run(parameters);
    Require(finished && parameters[0].calls.load() == 1 && parameters[1].calls.load() == 1);
}

struct Wide {
    Parameter parameter;
    unsigned char tail[40];
};

static void TestTheStride() {
    Environment environment(cores < 3 ? cores : 3, sizeof(Wide));
    std::vector<Wide> parameters(static_cast<std::size_t>(environment.Jobs()));
    execute(environment.get(), parameters.data());
    for (std::size_t index = 0; index < parameters.size(); ++index) {
        Require(parameters[index].parameter.calls.load() == 1);
        Require(parameters[index].parameter.onMainThread == (index + 1 == parameters.size()));
    }
}

static void TestTheRequestedNumber() {
    for (const int requested : {0, cores, cores + 5}) {
        Environment environment(requested);
        Require(environment.Jobs() == cores);
        environment.Run();
    }
    {
        Environment environment(-3);
        Require(environment.Jobs() == cores + 1);
        environment.Run();
    }
    if (cores >= 3) {
        Environment environment(cores - 1);
        Require(environment.Jobs() == cores - 1);
        environment.Run();
    }
}

static void TestSeveralEnvironments() {
    if (cores < 3) return;
    Environment first(2);
    Environment second(2);
    Environment rest(cores);
    Require(first.Jobs() == 2 && second.Jobs() == 2 && first.Thread(0) != second.Thread(0));
    Require(rest.Jobs() == cores - 1);
    for (unsigned index = 0; index + 1 < static_cast<unsigned>(rest.Jobs()); ++index)
        Require(rest.Thread(index) != first.Thread(0) && rest.Thread(index) != second.Thread(0));
    Environment none(cores);
    Require(none.Jobs() == 1);
    none.Run();
    rest.Run();
    second.Run();
    first.Run();
}

static void TestABusyWorker() {
    if (cores < 2) return;
    Environment outer(2);
    std::vector<Parameter> parameters(2);
    std::atomic<bool> workerStarted{false};
    std::atomic<bool> release{false};
    parameters[0].body = [&] {
        workerStarted = true;
        while (!release) std::this_thread::yield();
    };
    parameters[1].body = [&] {
        while (!workerStarted) std::this_thread::yield();
        {
            Environment inner(2);
            Require(inner.Jobs() == 2 && inner.Thread(0) != outer.Thread(0));
            inner.Run();
        }
        release = true;
    };
    outer.Run(parameters);
    Require(parameters[0].calls.load() == 1 && parameters[1].calls.load() == 1);
}

static void TestSimultaneousConstruction() {
    if (cores < 5) return;
    for (int round = 0; round < 50; ++round) {
        std::unique_ptr<Environment> environments[4];
        std::atomic<bool> start{false};
        std::thread threads[4];
        for (int index = 0; index < 4; ++index)
            threads[index] = std::thread([&, index] {
                while (!start) std::this_thread::yield();
                environments[index] = std::make_unique<Environment>(2);
            });
        start = true;
        for (std::thread& thread : threads) thread.join();
        std::set<NKWebKit::WtfAbi::ParallelEnvironmentThread*> workers;
        for (auto& environment : environments) {
            Require(environment->Jobs() == 2);
            workers.insert(environment->Thread(0));
        }
        Require(workers.size() == 4);
        for (auto& environment : environments) environment->Run();
    }
}

static void TestInvalidArguments() {
    Parameter parameter;
    Require(ThrowsInvalidArgument([&] { construct(nullptr, &Work, sizeof(Parameter), 1); }));
    Require(ThrowsInvalidArgument([&] { _ZN3WTF19ParallelEnvironmentC2EPFvPvEmi(nullptr, &Work, sizeof(Parameter), 1); }));
    Require(ThrowsInvalidArgument([&] { execute(nullptr, &parameter); }));
    alignas(16) unsigned char bytes[40];
    std::memset(bytes, 0xA5, sizeof(bytes));
    auto* environment = reinterpret_cast<ParallelEnvironment*>(bytes);
    _ZN3WTF19ParallelEnvironmentC2EPFvPvEmi(environment, nullptr, 8, 1);
    Require(environment->threadFunction == nullptr && environment->sizeOfParameter == 8 && environment->numberOfJobs == 1);
    Require(environment->threads.buffer == nullptr && environment->threads.size == 0 && environment->threads.capacity == 0);
    Require(ThrowsInvalidArgument([&] { execute(environment, &parameter); }));
    Require(parameter.calls.load() == 0);
}

static void TestAnEnvironmentThatNeverRuns() {
    if (cores < 3) return;
    ParallelEnvironmentThread* kept = nullptr;
    {
        Environment unused(2);
        kept = unused.Thread(0);
    }
    Require(kept->refCount == 1);
    Environment next(2);
    Require(next.Jobs() == 2 && next.Thread(0) != kept);
    next.Run();
}

int main() {
    std::thread([] {
        std::this_thread::sleep_for(std::chrono::seconds(20));
        std::fprintf(stderr, "parallel environment test timed out\n");
        std::fflush(stderr);
        std::_Exit(3);
    }).detach();
    _ZN3WTF20initializeMainThreadEv();
    cores = _ZN3WTF22numberOfProcessorCoresEv();
    Require(cores >= 1);
    TestOneJob();
    TestTwoJobs();
    TestTheWorkerFinishesLast();
    TestTheStride();
    TestTheRequestedNumber();
    TestSeveralEnvironments();
    TestABusyWorker();
    TestSimultaneousConstruction();
    TestInvalidArguments();
    TestAnEnvironmentThatNeverRuns();
    std::puts("libSceNKWebKit parallel environment tests passed");
    return 0;
}
