#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <iostream>
#include <thread>

namespace {

using namespace AgcDriver::DriverDetail;
using namespace ShaderRecompiler;

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

template<typename TAction>
void Reject(TAction action) {
    bool failed = false;
    try { action(); } catch (const std::runtime_error&) { failed = true; }
    Require(failed, "failed shader transaction was accepted");
}

void Transactions() {
    ShaderSnapshot front{};
    auto pixel = std::make_shared<ShaderSnapshot>();
    {
        ShaderPreparationTransaction transaction;
        transaction.Edit(front).entries.push_back({1, {}});
        {
            ShaderPreparationTransaction nested;
            nested.Edit(*pixel).entries.push_back({2, {}});
            nested.Commit();
        }
        Require(front.prepared->entries.empty() && pixel->prepared->entries.empty(), "nested preparation published a partial group");
    }
    Require(front.prepared->entries.empty() && pixel->prepared->entries.empty(), "aborted preparation published artifacts");
    {
        ShaderPreparationTransaction transaction;
        transaction.Edit(front).rectangleRequested = true;
        {
            ShaderPreparationTransaction nested;
            nested.Edit(*pixel).rectangleRequested = true;
        }
        Reject([&] { transaction.Commit(); });
    }
    Require(!front.prepared->rectangleRequested && !pixel->prepared->rectangleRequested, "nested abort did not roll back the group");
    Reject([&] { ResolvePreparedGraphics(front, pixel, 7, {}); });
    Require(!front.prepared->rectangleRequested && front.prepared->fragments.empty(), "failed rectangle preparation published a link");
    const auto rectangle = std::make_shared<const RectListShaders>();
    front.prepared->rectangles.push_back({1, 2, rectangle});
    {
        ShaderPreparationTransaction transaction;
        auto& prepared = transaction.Edit(front);
        Require(prepared.rectangles.front().shaders == rectangle, "transaction copied an immutable rectangle artifact");
        {
            ShaderPreparationTransaction nested;
            Require(&nested.Edit(front) == &prepared, "nested transaction did not reuse root changes");
            nested.Edit(*pixel).rectangleRequested = true;
            nested.Commit();
        }
        Require(!pixel->prepared->rectangleRequested, "nested commit published before root commit");
        transaction.Commit();
    }
    Require(pixel->prepared->rectangleRequested && front.prepared->rectangles.front().shaders == rectangle, "root commit lost nested changes or shared artifacts");
    std::weak_ptr<PreparedShaders> staged;
    {
        auto temporary = std::make_shared<ShaderSnapshot>();
        staged = temporary->prepared;
        ShaderPreparationTransaction transaction;
        transaction.Edit(*temporary).rectangleRequested = true;
        temporary.reset();
        Require(!staged.expired(), "pending publication released its destination");
        transaction.Commit();
    }
    Require(staged.expired(), "completed publication retained its destination");
    std::barrier start(3);
    std::atomic<bool> partial = false;
    std::jthread reader([&](std::stop_token stop) {
        start.arrive_and_wait();
        while (!stop.stop_requested()) {
            std::scoped_lock lock(front.prepared->mutex, pixel->prepared->mutex);
            if (front.prepared->entries.size() != pixel->prepared->entries.size()) partial = true;
        }
    });
    const auto write = [&] {
        start.arrive_and_wait();
        for (unsigned i = 0; i < 100; ++i) {
            ShaderPreparationTransaction transaction;
            transaction.Edit(front).entries.push_back({i, {}});
            std::this_thread::yield();
            transaction.Edit(*pixel).entries.push_back({i, {}});
            transaction.Commit();
        }
    };
    auto first = std::async(std::launch::async, write);
    auto second = std::async(std::launch::async, write);
    first.get();
    second.get();
    reader.request_stop();
    reader.join();
    Require(!partial && front.prepared->entries.size() == 200 && pixel->prepared->entries.size() == 200, "concurrent group publication was partial or lost an update");
}

void Registration(AgcDriver::VulkanDevice& device) {
    const std::array<std::uint32_t, 1> code{0xbf810000u};
    RecompileRequest request{{ShaderStage::Compute, 0x10000, code, 0, {}}, {32, 0, {}, ShaderComputeStageInfo{{1, 1, 1}, 0, {false, false, false}, false, 0, {}}, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
    const auto handle = PrepareShader(request);
    const auto makeSnapshot = [&] {
        auto snapshot = std::make_shared<ShaderSnapshot>();
        snapshot->codeAddress = 0x10000;
        snapshot->headerAddress = 0x20000;
        snapshot->code.assign(code.begin(), code.end());
        return snapshot;
    };
    auto original = makeSnapshot();
    original->prepared->entries.push_back({0, handle});
    original->prepared->rectangleRequested = true;
    std::shared_ptr<ShaderRegistry> registry;
    PublishRegisteredShader(registry, original);
    std::barrier start(4);
    std::vector<std::future<void>> workers;
    for (unsigned i = 0; i < 4; ++i) {
        workers.push_back(std::async(std::launch::async, [&] {
            start.arrive_and_wait();
            for (unsigned iteration = 0; iteration < 40; ++iteration) {
                ShaderPreparationTransaction transaction;
                PublishRegisteredShader(registry, makeSnapshot());
                const auto current = registry->at(0x10000);
                Require(current == original && current->prepared->rectangleRequested, "identical registration replaced a prepared snapshot");
                auto& entries = transaction.Edit(*current).entries;
                entries.push_back({iteration + 1, handle});
                transaction.Commit();
            }
        }));
    }
    for (auto& worker : workers) worker.get();
    Require(original->prepared->entries.size() == 161, "re-registration lost prepared artifacts");
    std::shared_ptr<const ShaderRegistry> submission = registry;
    std::weak_ptr<const ShaderSnapshot> old = original;
    auto replacement = makeSnapshot();
    replacement->headerAddress += 256;
    std::promise<void> registering;
    auto started = registering.get_future();
    std::future<void> replacementWriter;
    {
        ShaderPreparationTransaction transaction;
        transaction.Edit(*original).entries.push_back({999, handle});
        replacementWriter = std::async(std::launch::async, [&] {
            registering.set_value();
            PublishRegisteredShader(registry, replacement);
        });
        started.get();
        Require(replacementWriter.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout, "replacement interleaved with an unfinished preparation");
        Require(original->prepared->entries.size() == 161, "unfinished preparation changed a live snapshot");
        transaction.Commit();
    }
    replacementWriter.get();
    Require(registry->at(0x10000) == replacement && submission->at(0x10000) == original, "replacement modified an in-flight registry");
    Require(original->prepared->entries.size() == 162 && replacement->prepared->entries.empty(), "stale preparation was published into a replacement snapshot");
    original.reset();
    request.shader.code = submission->at(0x10000)->code;
    const auto invocation = InvocationFor(*submission->at(0x10000), 0, request);
    const auto capture = invocation.Capture({});
    const auto result = invocation.Materialize(*capture);
    Require(result->variantId == GetPreparedArtifact(*handle).variantId, "old submission lost its prepared artifact");
    device.Dispatch(*result, 1, 1, 1);
    device.WaitIdle();
    Require(!old.expired(), "in-flight submission released its snapshot early");
    submission.reset();
    Require(old.expired(), "completed submission retained an obsolete snapshot");
    Require(invocation.Materialize(*capture)->spirv.data() == result->spirv.data(), "prepared handle did not retain its artifact");
    auto changed = makeSnapshot();
    changed->headerAddress = replacement->headerAddress;
    changed->code[0] ^= 1;
    PublishRegisteredShader(registry, changed);
    Require(registry->at(0x10000) == changed, "changed shader code was treated as identical");
    auto metadata = makeSnapshot();
    metadata->headerAddress = changed->headerAddress;
    metadata->code = changed->code;
    metadata->header.push_back(std::byte{1});
    PublishRegisteredShader(registry, metadata);
    Require(registry->at(0x10000) == metadata, "changed shader metadata was treated as identical");
}

}

int main() {
    try {
        Transactions();
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Registration(*device);
        std::cout << "shader publication and lifetime tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
