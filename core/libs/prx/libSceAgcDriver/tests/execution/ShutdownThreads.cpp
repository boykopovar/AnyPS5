#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ParallelCompare.hpp"
#include "CompiledVariant.hpp"
#include "ShaderDiskCache.hpp"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <future>
#include <iostream>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <vector>

namespace {

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void SetEnvironment(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    setenv(name, value.c_str(), 1);
#endif
}

void ClosingStartsNoPreparation() {
    using namespace AgcDriver::DriverDetail;
    Require(RegistrationPreparationThreads() == 0, "a shader preparation thread ran before anything was prepared");
    CloseRegistrationPreparation();
    Require(RegistrationPreparationThreads() == 0, "closing the shader preparation started preparation threads");
    const auto caller = std::this_thread::get_id();
    bool ranHere = false;
    SubmitRegistrationPreparation([&] { ranHere = std::this_thread::get_id() == caller; });
    SubmitLibraryPrebuild([&] { ranHere = ranHere && std::this_thread::get_id() == caller; });
    Require(ranHere, "a preparation submitted after closing ran on another thread");
    Require(RegistrationPreparationThreads() == 0, "a preparation submitted after closing started preparation threads");
}

void ClosingJoinsPreparation() {
    using namespace AgcDriver::DriverDetail;
    std::promise<std::thread::id> prepared;
    std::promise<std::thread::id> prebuilt;
    auto preparedOn = prepared.get_future();
    auto prebuiltOn = prebuilt.get_future();
    std::promise<void> gate;
    auto opened = gate.get_future().share();
    SubmitRegistrationPreparation([&] { prepared.set_value(std::this_thread::get_id()); opened.wait(); });
    SubmitLibraryPrebuild([&] { prebuilt.set_value(std::this_thread::get_id()); opened.wait(); });
    const auto caller = std::this_thread::get_id();
    const bool offCaller = preparedOn.get() != caller && prebuiltOn.get() != caller;
    auto closing = std::async(std::launch::async, [] { CloseRegistrationPreparation(); });
    const bool waited = closing.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
    gate.set_value();
    closing.get();
    Require(offCaller, "a queued preparation ran on the caller instead of a preparation thread");
    Require(waited, "closing returned while preparation tasks were still running");
    Require(RegistrationPreparationThreads() == 0, "closing the shader preparation left its threads running");
    CloseRegistrationPreparation();
    bool ranHere = false;
    SubmitRegistrationPreparation([&] { ranHere = std::this_thread::get_id() == caller; });
    SubmitLibraryPrebuild([&] { ranHere = ranHere && std::this_thread::get_id() == caller; });
    Require(ranHere && RegistrationPreparationThreads() == 0, "a closed preparation pool restarted after being joined");
}

void ClosingDropsQueuedPrebuilds() {
    using namespace AgcDriver::DriverDetail;
    std::promise<void> started;
    auto running = started.get_future();
    std::promise<void> gate;
    auto opened = gate.get_future().share();
    bool queuedRan = false;
    SubmitLibraryPrebuild([&] { started.set_value(); opened.wait(); });
    for (int index = 0; index < 3; ++index) SubmitLibraryPrebuild([&] { queuedRan = true; });
    running.wait();
    auto entering = std::make_shared<std::promise<void>>();
    auto entered = entering->get_future();
    auto awaited = std::make_shared<std::promise<void>>();
    auto woken = awaited->get_future();
    std::thread([entering, awaited] {
        entering->set_value();
        AwaitLibraryPrebuilds();
        awaited->set_value();
    }).detach();
    entered.wait();
    const bool waiting = woken.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
    auto closing = std::async(std::launch::async, [] { CloseRegistrationPreparation(); });
    const bool waited = closing.wait_for(std::chrono::milliseconds(20)) == std::future_status::timeout;
    gate.set_value();
    const bool returned = closing.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    const bool wokeUp = woken.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    closing.get();
    Require(waiting, "a waiter for the library prebuilds returned while one was still running");
    Require(waited, "closing returned while a library prebuild was still running");
    Require(returned, "closing did not return once the running library prebuild ended");
    Require(wokeUp, "a waiter for the library prebuilds did not return after closing dropped the queued ones");
    Require(!queuedRan, "closing ran the queued library prebuilds");
    Require(RegistrationPreparationThreads() == 0, "closing the library prebuilds left their thread running");
}

void StoppingJoinsCompareHelpers() {
    using namespace AgcDriver::Graphics;
    const std::vector<std::byte> first(std::size_t{2} << 20u, std::byte{1});
    auto second = first;
    second.back() = std::byte{2};
    const auto half = first.size() / 2u;
    const std::array<CompareSpan, 2> spans{{{first.data(), second.data(), half}, {first.data() + half, second.data() + half, half}}};
    std::array<std::uint8_t, 2> equal{};
    CompareSpans(spans, equal);
    Require(equal[0] == 1u && equal[1] == 0u, "the compare helpers compared the spans wrongly");
    Require(CompareHelpersRunning() == CompareHelpers(), "a parallel compare started no compare helpers");
    StopCompareHelpers();
    Require(CompareHelpersRunning() == 0, "stopping the compare helpers left them running");
    equal = {};
    CompareSpans(spans, equal);
    Require(equal[0] == 1u && equal[1] == 0u, "a compare after the helpers stopped compared the spans wrongly");
    Require(CompareHelpersRunning() == 0, "a compare after the helpers stopped started them again");
}

void ShutdownJoinsDrawCommitter() {
    using namespace AgcDriver::DriverDetail;
    auto& pipeline = DrawPipeline::Queue0();
    const auto caller = std::this_thread::get_id();
    std::promise<std::thread::id> started;
    auto startedOn = started.get_future();
    std::promise<void> gate;
    auto opened = gate.get_future().share();
    bool queuedRan = false;
    pipeline.Enqueue([&started, opened] {
        started.set_value(std::this_thread::get_id());
        opened.wait();
    }, {});
    pipeline.Enqueue([&] { queuedRan = true; }, {});
    Require(startedOn.get() != caller, "a pipelined commit ran on the caller instead of the draw committer");
    Require(DrawPipeline::Committers() == 1, "a pipelined commit started no draw committer");
    gate.set_value();
    AgcDriverShutdown_nid_postfix();
    Require(DrawPipeline::Committers() == 0, "the driver's shutdown left the draw committer running");
    Require(queuedRan, "the driver's shutdown dropped a pipelined commit");
    bool ranHere = false;
    pipeline.Enqueue([&] { ranHere = std::this_thread::get_id() == caller; }, {});
    Require(ranHere, "a commit enqueued after the shutdown ran on another thread");
    Require(DrawPipeline::Committers() == 0, "a commit enqueued after the shutdown started a draw committer");
}

void ShutdownJoinsShaderCacheWriter(bool lateInitialization = false) {
    namespace Cache = ShaderRecompiler::ShaderDiskCache;
    const auto directory = std::filesystem::temp_directory_path() / ("aps5-shutdown-shader-cache-" + std::to_string(std::random_device{}()));
    SetEnvironment("ANYPS5_NO_SHADER_CACHE", "0");
    SetEnvironment("ANYPS5_SHADER_CACHE_DIR", directory.string());
    if (lateInitialization) AgcDriverShutdown_nid_postfix();
    Require(Cache::Enabled(), "the shader cache is not enabled");
    Require(Cache::Writers() == (lateInitialization ? 0u : 1u), "the shader cache writer did not respect the shutdown state");
    const auto variant = std::make_shared<const ShaderRecompiler::CompiledVariant>();
    const std::vector<std::byte> queued{std::byte{1}};
    const std::vector<std::byte> late{std::byte{2}};
    Cache::Store(queued, variant);
    AgcDriverShutdown_nid_postfix();
    Require(Cache::Writers() == 0, "the driver's shutdown left the shader cache writer running");
    Require(std::filesystem::exists(Cache::EntryDirectory() / Cache::EntryName(queued)), "the driver's shutdown dropped a queued shader cache write");
    Cache::Store(late, variant);
    Require(std::filesystem::exists(Cache::EntryDirectory() / Cache::EntryName(late)), "a shader cache store after the shutdown was not written at once");
    Require(Cache::Writers() == 0, "a shader cache store after the shutdown started a writer");
    std::error_code error;
    std::filesystem::remove_all(directory, error);
}

void ShutdownPassesHeldAsyncLog() {
    static_cast<void>(AgcDriver::DriverDetail::Driver::Get());
    std::promise<void> held;
    std::promise<void> release;
    auto released = release.get_future();
    std::thread holder([&held, &released] {
        const auto lock = AgcDriver::HoldAsyncLog();
        held.set_value();
        released.wait();
    });
    held.get_future().wait();
    auto shutdown = std::async(std::launch::async, [] { AgcDriverShutdown_nid_postfix(); });
    const bool returned = shutdown.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    release.set_value();
    holder.join();
    shutdown.get();
    Require(returned, "the driver's shutdown waited for the async log's lock");
}

}

int main(int argc, char** argv) {
    try {
        const std::string_view mode = argc > 1 ? argv[1] : "";
        if (mode == "--joined") ClosingJoinsPreparation();
        else if (mode == "--queued-prebuilds") ClosingDropsQueuedPrebuilds();
        else if (mode == "--compare-helpers") StoppingJoinsCompareHelpers();
        else if (mode == "--draw-committer") ShutdownJoinsDrawCommitter();
        else if (mode == "--shader-cache-writer") ShutdownJoinsShaderCacheWriter();
        else if (mode == "--late-shader-cache") ShutdownJoinsShaderCacheWriter(true);
        else if (mode == "--async-log-held") ShutdownPassesHeldAsyncLog();
        else if (mode == "--exit-after-start") static_cast<void>(AgcDriver::DriverDetail::Driver::Get());
        else ClosingStartsNoPreparation();
        std::cout << "shutdown threads tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
