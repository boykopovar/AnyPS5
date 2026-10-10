#include "VulkanTestDevice.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <array>
#include <atomic>
#include <barrier>
#include <chrono>
#include <cstddef>
#include <future>
#include <memory>
#include <stdexcept>
#include <thread>
#include <vector>

namespace {

using namespace AgcDriver::DriverDetail;
using namespace ShaderRecompiler;

template<typename TAction>
void RequireRejected(TAction action) {
    Testing::RequireThrows<std::runtime_error>(action, "failed shader transaction was accepted");
}

struct SnapshotPair {
    ShaderSnapshot front{};
    std::shared_ptr<ShaderSnapshot> pixel = std::make_shared<ShaderSnapshot>();
};

const Testing::Case nestedCommitInAbortedRoot{"Transaction_NestedCommitInsideAbortedRoot_PublishesNothing", [] {
    SnapshotPair snapshots;
    auto& front = snapshots.front;
    auto& pixel = snapshots.pixel;
    {
        ShaderPreparationTransaction transaction;
        transaction.Edit(front).entries.push_back({1, {}});
        {
            ShaderPreparationTransaction nested;
            nested.Edit(*pixel).entries.push_back({2, {}});
            nested.Commit();
        }
        Testing::Require(front.prepared->entries.empty() && pixel->prepared->entries.empty(), "nested preparation published a partial group");
    }
    Testing::Require(front.prepared->entries.empty() && pixel->prepared->entries.empty(), "aborted preparation published artifacts");
}};

const Testing::Case nestedAbortRollsBack{"Transaction_NestedAbort_RejectsRootCommitAndRollsBackTheGroup", [] {
    SnapshotPair snapshots;
    auto& front = snapshots.front;
    auto& pixel = snapshots.pixel;
    {
        ShaderPreparationTransaction transaction;
        transaction.Edit(front).rectangleRequested = true;
        {
            ShaderPreparationTransaction nested;
            nested.Edit(*pixel).rectangleRequested = true;
        }
        RequireRejected([&] { transaction.Commit(); });
    }
    Testing::Require(!front.prepared->rectangleRequested && !pixel->prepared->rectangleRequested, "nested abort did not roll back the group");
}};

const Testing::Case failedRectanglePreparation{"ResolvePreparedGraphics_FailedRectanglePreparation_PublishesNoLink", [] {
    SnapshotPair snapshots;
    auto& front = snapshots.front;
    RequireRejected([&] { ResolvePreparedGraphics(front, snapshots.pixel, 7, {}); });
    Testing::Require(!front.prepared->rectangleRequested && front.prepared->fragments.empty(), "failed rectangle preparation published a link");
}};

const Testing::Case nestedSharesRootChanges{"Transaction_NestedInsideRoot_SharesChangesAndPublishesOnRootCommit", [] {
    SnapshotPair snapshots;
    auto& front = snapshots.front;
    auto& pixel = snapshots.pixel;
    const auto rectangle = std::make_shared<const RectListShaders>();
    front.prepared->rectangles.push_back({1, 2, rectangle});
    {
        ShaderPreparationTransaction transaction;
        auto& prepared = transaction.Edit(front);
        Testing::Require(prepared.rectangles.front().shaders == rectangle, "transaction copied an immutable rectangle artifact");
        {
            ShaderPreparationTransaction nested;
            Testing::Require(&nested.Edit(front) == &prepared, "nested transaction did not reuse root changes");
            nested.Edit(*pixel).rectangleRequested = true;
            nested.Commit();
        }
        Testing::Require(!pixel->prepared->rectangleRequested, "nested commit published before root commit");
        transaction.Commit();
    }
    Testing::Require(pixel->prepared->rectangleRequested && front.prepared->rectangles.front().shaders == rectangle, "root commit lost nested changes or shared artifacts");
}};

const Testing::Case pendingPublicationLifetime{"Transaction_PendingPublication_RetainsDestinationOnlyUntilCommit", [] {
    std::weak_ptr<PreparedShaders> staged;
    {
        auto temporary = std::make_shared<ShaderSnapshot>();
        staged = temporary->prepared;
        ShaderPreparationTransaction transaction;
        transaction.Edit(*temporary).rectangleRequested = true;
        temporary.reset();
        Testing::Require(!staged.expired(), "pending publication released its destination");
        transaction.Commit();
    }
    Testing::Require(staged.expired(), "completed publication retained its destination");
}};

const Testing::Case concurrentGroupPublication{"Transaction_ConcurrentGroupPublication_IsAtomicAndComplete", [] {
    SnapshotPair snapshots;
    auto& front = snapshots.front;
    auto& pixel = snapshots.pixel;
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
    Testing::Require(!partial, "concurrent group publication was partial");
    Testing::RequireEqual(front.prepared->entries.size(), std::size_t{200}, "concurrent group publication lost a front update");
    Testing::RequireEqual(pixel->prepared->entries.size(), std::size_t{200}, "concurrent group publication lost a pixel update");
}};

constexpr std::uintptr_t CodeAddress = 0x10000;
constexpr std::array<std::uint32_t, 1> EndProgram{0xbf810000u};

RecompileRequest ComputeRequest(AgcDriver::VulkanDevice& device) {
    return RecompileRequest{{ShaderStage::Compute, CodeAddress, EndProgram, 0, {}}, {32, 0, {}, ShaderComputeStageInfo{{1, 1, 1}, 0, {false, false, false}, false, 0, {}}, {}, {}, {}}, device.ComputeTarget(32), {0, 0, 0, 128}};
}

std::shared_ptr<ShaderSnapshot> MakeSnapshot() {
    auto snapshot = std::make_shared<ShaderSnapshot>();
    snapshot->codeAddress = CodeAddress;
    snapshot->headerAddress = 0x20000;
    snapshot->code.assign(EndProgram.begin(), EndProgram.end());
    return snapshot;
}

struct RegisteredShader {
    explicit RegisteredShader(const std::shared_ptr<const SourceHandle>& handle) : original(MakeSnapshot()) {
        original->prepared->entries.push_back({0, handle});
        original->prepared->rectangleRequested = true;
        PublishRegisteredShader(registry, original);
    }

    std::shared_ptr<ShaderSnapshot> original;
    std::shared_ptr<ShaderRegistry> registry;
};

const Testing::Case identicalReregistration{"Registration_ConcurrentIdenticalRegistration_KeepsThePreparedSnapshot", [] {
    auto& device = SharedVulkanTestDevice();
    const auto handle = PrepareShader(ComputeRequest(device));
    RegisteredShader shader(handle);
    auto& registry = shader.registry;
    const auto& original = shader.original;
    std::barrier start(4);
    std::vector<std::future<void>> workers;
    for (unsigned i = 0; i < 4; ++i) {
        workers.push_back(std::async(std::launch::async, [&] {
            start.arrive_and_wait();
            for (unsigned iteration = 0; iteration < 40; ++iteration) {
                ShaderPreparationTransaction transaction;
                PublishRegisteredShader(registry, MakeSnapshot());
                const auto current = registry->at(CodeAddress);
                Testing::Require(current == original && current->prepared->rectangleRequested, "identical registration replaced a prepared snapshot");
                auto& entries = transaction.Edit(*current).entries;
                entries.push_back({iteration + 1, handle});
                transaction.Commit();
            }
        }));
    }
    for (auto& worker : workers) worker.get();
    Testing::RequireEqual(original->prepared->entries.size(), std::size_t{161}, "re-registration lost prepared artifacts");
}};

const Testing::Case replacementDuringPreparation{"Registration_ReplacementDuringPreparation_WaitsAndKeepsInFlightSubmissionAlive", [] {
    auto& device = SharedVulkanTestDevice();
    auto request = ComputeRequest(device);
    const auto handle = PrepareShader(request);
    RegisteredShader shader(handle);
    auto& registry = shader.registry;
    auto& original = shader.original;
    const auto prepared = original->prepared->entries.size();
    std::shared_ptr<const ShaderRegistry> submission = registry;
    std::weak_ptr<const ShaderSnapshot> old = original;
    auto replacement = MakeSnapshot();
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
        Testing::Require(replacementWriter.wait_for(std::chrono::milliseconds(0)) == std::future_status::timeout, "replacement interleaved with an unfinished preparation");
        Testing::RequireEqual(original->prepared->entries.size(), prepared, "unfinished preparation changed a live snapshot");
        transaction.Commit();
    }
    replacementWriter.get();
    Testing::Require(registry->at(CodeAddress) == replacement && submission->at(CodeAddress) == original, "replacement modified an in-flight registry");
    Testing::Require(original->prepared->entries.size() == prepared + 1 && replacement->prepared->entries.empty(), "stale preparation was published into a replacement snapshot");
    original.reset();
    request.shader.code = submission->at(CodeAddress)->code;
    const auto invocation = InvocationFor(*submission->at(CodeAddress), 0, request);
    const auto capture = invocation.Capture({});
    const auto result = invocation.Materialize(*capture);
    Testing::Require(result->variantId == GetPreparedArtifact(*handle).variantId, "old submission lost its prepared artifact");
    device.Dispatch(*result, 1, 1, 1);
    device.WaitIdle();
    Testing::Require(!old.expired(), "in-flight submission released its snapshot early");
    submission.reset();
    Testing::Require(old.expired(), "completed submission retained an obsolete snapshot");
    Testing::Require(invocation.Materialize(*capture)->spirv.data() == result->spirv.data(), "prepared handle did not retain its artifact");
}};

const Testing::Case changedShaderReplaces{"Registration_ChangedCodeOrMetadata_ReplacesTheSnapshot", [] {
    auto& device = SharedVulkanTestDevice();
    RegisteredShader shader(PrepareShader(ComputeRequest(device)));
    auto& registry = shader.registry;
    auto changed = MakeSnapshot();
    changed->headerAddress = shader.original->headerAddress;
    changed->code[0] ^= 1;
    PublishRegisteredShader(registry, changed);
    Testing::Require(registry->at(CodeAddress) == changed, "changed shader code was treated as identical");
    auto metadata = MakeSnapshot();
    metadata->headerAddress = changed->headerAddress;
    metadata->code = changed->code;
    metadata->header.push_back(std::byte{1});
    PublishRegisteredShader(registry, metadata);
    Testing::Require(registry->at(CodeAddress) == metadata, "changed shader metadata was treated as identical");
}};

} // namespace
