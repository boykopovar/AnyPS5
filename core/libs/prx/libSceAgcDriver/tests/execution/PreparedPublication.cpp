#include "VulkanTestDevice.hpp"
#include "SceShaders.hpp"
#include "prx/libSceAgcDriver/Execution/include/ShaderPreparation.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Shaders/ShaderRegistry.hpp"
#include "Optimization/ResourceProgram.hpp"
#include <array>
#include <algorithm>
#include <atomic>
#include <barrier>
#include <chrono>
#include <future>
#include <cstring>
#include <iostream>
#include <latch>
#include <map>
#include <optional>
#include <semaphore>
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

void HeaderSelection() {
    const auto makeSnapshot = [](std::uint64_t headerAddress, std::uint8_t type) {
        auto snapshot = std::make_shared<ShaderSnapshot>();
        snapshot->codeAddress = 0x10000;
        snapshot->headerAddress = headerAddress;
        snapshot->type = type;
        snapshot->code = {0xbf810000u};
        Shader header{};
        header.code = reinterpret_cast<const void*>(snapshot->codeAddress);
        header.type = type;
        snapshot->header.resize(sizeof(Shader));
        std::memcpy(snapshot->header.data(), &header, sizeof(Shader));
        return snapshot;
    };
    const auto vertex = makeSnapshot(0x20000, 2);
    const auto compute = makeSnapshot(0x30000, 0);
    std::shared_ptr<ShaderRegistry> registry;
    PublishRegisteredShader(registry, vertex);
    PublishRegisteredShader(registry, compute);
    Require(RegisteredProgram(*registry, 0x10000, {2}) == vertex && RegisteredProgram(*registry, 0x10000, {0}) == compute, "shared code selected another registered type");
    Reject([&] { static_cast<void>(RegisteredProgram(*registry, 0x10000, {1})); });
    Require(RegisteredProgram(*registry, 0x10100, {2}) == nullptr, "an out-of-range entry selected registered code");
    const auto copy = makeSnapshot(0x40000, 2);
    PublishRegisteredShader(registry, copy);
    Require(RegisteredProgram(*registry, 0x10000, {2}) == copy, "equivalent registered headers became ambiguous");
    PublishRegisteredShader(registry, makeSnapshot(vertex->headerAddress, 2));
    Require(RegisteredProgram(*registry, 0x10000, {2}) == vertex && registry->at(0x10000).size() == 3, "identical earlier registration did not restore its prepared snapshot as current");
    const auto replacement = makeSnapshot(copy->headerAddress, 2);
    Shader changed;
    std::memcpy(&changed, replacement->header.data(), sizeof(Shader));
    changed.target = 1;
    std::memcpy(replacement->header.data(), &changed, sizeof(Shader));
    PublishRegisteredShader(registry, replacement);
    Require(RegisteredProgram(*registry, 0x10000, {2}) == replacement, "same-type registration did not keep the latest program metadata");
    const auto geometry = makeSnapshot(0x60000, 4);
    PublishRegisteredShader(registry, geometry);
    Reject([&] { static_cast<void>(RegisteredProgram(*registry, 0x10000, {2, 4})); });
}

void RegistryVersions() {
    using Expected = std::map<std::uint64_t, std::shared_ptr<const ShaderSnapshot>>;
    const auto matches = [](const ShaderRegistry& current, const Expected& expected) {
        const auto same = [](const auto& entry, const auto& wanted) { return entry.first == wanted.first && entry.second.size() == 1u && entry.second.front() == wanted.second; };
        if (current.size() != expected.size() || !std::equal(current.begin(), current.end(), expected.begin(), expected.end(), same)) return false;
        auto backward = current.end();
        for (auto wanted = expected.rbegin(); wanted != expected.rend(); ++wanted) {
            if (backward == current.begin() || !same(*--backward, *wanted)) return false;
        }
        if (backward != current.begin()) return false;
        for (const auto& entry : expected) {
            for (const auto probe : {std::uint64_t{0}, entry.first - 1u, entry.first, entry.first + 4u, ~std::uint64_t{0}}) {
                const auto found = current.upper_bound(probe);
                const auto wanted = expected.upper_bound(probe);
                if ((found == current.end()) != (wanted == expected.end()) || (found != current.end() && found->first != wanted->first)) return false;
                if (current.contains(probe) != expected.contains(probe) || (current.find(probe) == current.end()) == expected.contains(probe)) return false;
            }
        }
        return true;
    };
    const auto makeSnapshot = [](std::uint64_t codeAddress) {
        auto snapshot = std::make_shared<ShaderSnapshot>();
        snapshot->codeAddress = codeAddress;
        snapshot->headerAddress = codeAddress + 0x80u;
        snapshot->code = {0xbf810000u};
        return snapshot;
    };
    std::vector<std::shared_ptr<const ShaderSnapshot>> snapshots;
    Expected expected;
    std::shared_ptr<ShaderRegistry> registry;
    std::uint64_t state = 1;
    while (snapshots.size() < 4096u) {
        state = state * 6364136223846793005ull + 1442695040888963407ull;
        const auto address = 0x100000u + ((state >> 33u) % 0x100000u) * 0x100u;
        if (expected.contains(address)) continue;
        const auto snapshot = makeSnapshot(address);
        expected.emplace(address, snapshot);
        snapshots.push_back(snapshot);
        PublishRegisteredShader(registry, snapshot);
    }
    Require(matches(*registry, expected), "the shader registry did not keep its code addresses in order");
    std::shared_ptr<const ShaderRegistry> earlier = registry;
    const auto previous = expected;
    const auto added = makeSnapshot(0x100040u);
    expected.emplace(added->codeAddress, added);
    std::vector<long> uses;
    for (const auto& snapshot : snapshots) uses.push_back(snapshot.use_count());
    PublishRegisteredShader(registry, added);
    std::size_t copied = 0;
    for (std::size_t index = 0; index < snapshots.size(); ++index) copied += snapshots[index].use_count() != uses[index] ? 1u : 0u;
    Require(copied <= 128u, "publishing a shader copied the registry's entries for other code addresses");
    Require(matches(*registry, expected) && matches(*earlier, previous), "publishing a shader changed an earlier registry or lost an entry");
    ShaderRegistry edited = *registry;
    auto remaining = expected;
    std::size_t index = 0;
    for (const auto& entry : expected) {
        if (index++ % 3u != 0u) continue;
        Require(edited.erase(entry.first) == 1u && edited.erase(entry.first) == 0u, "the shader registry did not erase a code address once");
        remaining.erase(entry.first);
    }
    Require(matches(edited, remaining) && matches(*registry, expected), "erasing from a copy of the registry changed the original or lost an entry");
    for (const auto& entry : remaining) edited.erase(entry.first);
    Require(edited.empty() && edited.begin() == edited.end() && edited.upper_bound(0) == edited.end(), "an emptied shader registry kept entries");
    Require(edited.emplace(added->codeAddress, ShaderRegistry::mapped_type{added}).second && !edited.emplace(added->codeAddress, ShaderRegistry::mapped_type{}).second, "an emptied shader registry did not take a new entry once");
    Require(edited.size() == 1u && edited.at(added->codeAddress).size() == 1u && edited.at(added->codeAddress).front() == added, "an emptied shader registry lost its new entry");
}

void FusedHullHeaderSelection() {
    struct Hull {
        Shader shader{};
        std::array<ShaderRegister, 7> registers{{{0x100, 0x11}, {0x108, 0x100}, {0x109, 0}, {0x10a, 0x2}, {0x10b, 0x10}, {0x148, 0}, {0x149, 0}}};
        ShaderSpecialRegs specials{};
    } back;
    back.shader.file_header = 0x34333231u;
    back.shader.version = 0x18;
    back.shader.header_size = sizeof(Hull);
    back.shader.shader_size = 4;
    back.shader.code = reinterpret_cast<const void*>(std::uintptr_t{0x50000});
    back.shader.type = 7;
    back.shader.sh_registers = back.registers.data();
    back.shader.num_sh_registers = back.registers.size();
    back.shader.specials = &back.specials;
    auto snapshot = std::make_shared<ShaderSnapshot>();
    snapshot->codeAddress = 0x50000;
    snapshot->headerAddress = reinterpret_cast<std::uintptr_t>(&back);
    snapshot->type = 7;
    snapshot->code = {0xbf810000u};
    snapshot->header.resize(sizeof(Hull));
    std::memcpy(snapshot->header.data(), &back, sizeof(Hull));
    std::shared_ptr<ShaderRegistry> registry;
    PublishRegisteredShader(registry, snapshot);
    auto registers = back.registers;
    registers[0].value = 0x22;
    registers[3].value = 0x3;
    registers[4].value = 0x1e;
    registers[5].value = 0x600;
    registers[6].value = 0x1;
    Shader fused = back.shader;
    fused.type = 3;
    fused.sh_registers = registers.data();
    fused.user_data = nullptr;
    Require(RegisteredHeader(*registry, &fused) == snapshot, "a fused hull header did not select its registered back half");
    registers[1].value = 0x200;
    Require(RegisteredHeader(*registry, &fused) == nullptr, "a fused hull header with another hull program selected the back half");
    registers[1].value = 0x100;
    registers[2].offset = 0x10c;
    Require(RegisteredHeader(*registry, &fused) == nullptr, "a fused hull header with other registers selected the back half");
    registers[2].offset = 0x109;
    fused.target ^= 1u;
    Require(RegisteredHeader(*registry, &fused) == nullptr, "a fused hull header with another target selected the back half");
    fused.target ^= 1u;
    fused.type = 2;
    Require(RegisteredHeader(*registry, &fused) == nullptr, "a vertex header selected a registered hull back half");
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
                const auto current = registry->at(0x10000).back();
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
    Require(registry->at(0x10000).back() == replacement && submission->at(0x10000).back() == original, "replacement modified an in-flight registry");
    Require(original->prepared->entries.size() == 162 && replacement->prepared->entries.empty(), "stale preparation was published into a replacement snapshot");
    original.reset();
    request.shader.code = submission->at(0x10000).back()->code;
    const auto invocation = InvocationFor(*submission->at(0x10000).back(), 0, request);
    const auto capture = invocation.Capture({});
    const auto result = invocation.Materialize(*capture);
    Require(result->variantId == GetPreparedArtifact(*handle).variantId, "old submission lost its prepared artifact");
    device.Dispatch(*result, 1, 1, 1);
    device.WaitIdle();
    Require(!old.expired(), "in-flight submission released its snapshot early");
    submission.reset();
    Require(!old.expired() && registry->at(0x10000).size() == 2, "registration discarded an earlier header for shared code");
    Require(invocation.Materialize(*capture)->spirv.data() == result->spirv.data(), "prepared handle did not retain its artifact");
    auto changed = makeSnapshot();
    changed->headerAddress = replacement->headerAddress;
    changed->code[0] ^= 1;
    PublishRegisteredShader(registry, changed);
    Require(registry->at(0x10000).back() == changed && registry->at(0x10000).size() == 1 && old.expired(), "changed shader code retained obsolete headers");
    auto metadata = makeSnapshot();
    metadata->headerAddress = changed->headerAddress;
    metadata->code = changed->code;
    metadata->header.push_back(std::byte{1});
    PublishRegisteredShader(registry, metadata);
    Require(registry->at(0x10000).back() == metadata, "changed shader metadata was treated as identical");
}

void TransactionLock() {
    ShaderSnapshot shader{};
    const auto publish = [&] {
        ShaderPreparationTransaction other;
        other.Edit(shader).rectangleRequested = true;
        other.Commit();
    };
    std::optional<ShaderPreparationTransaction> open(std::in_place);
    auto unread = std::async(std::launch::async, publish);
    const bool unblocked = unread.wait_for(std::chrono::seconds(10)) == std::future_status::ready;
    if (!unblocked) open.reset();
    unread.get();
    Require(unblocked, "an open transaction that read nothing blocked another one");
    static_cast<void>(open->Read(shader));
    auto waiting = std::async(std::launch::async, publish);
    const bool excluded = waiting.wait_for(std::chrono::milliseconds(200)) == std::future_status::timeout;
    open->Commit();
    open.reset();
    waiting.get();
    Require(excluded, "a transaction ran beside one that had read prepared state");
}

void PreparationPriority(bool beforeSubmit) {
    const auto workers = std::max(2u, std::thread::hardware_concurrency() / 2u);
    std::barrier started(static_cast<std::ptrdiff_t>(workers + 1u));
    std::counting_semaphore<> release(0);
    std::latch finished(static_cast<std::ptrdiff_t>(workers));
    for (unsigned index = 0; index < workers; ++index) {
        SubmitRegistrationPreparation([&] {
            started.arrive_and_wait();
            release.acquire();
            finished.count_down();
        });
    }
    started.arrive_and_wait();
    PreparedShaders prepared;
    std::atomic<unsigned> order{0};
    std::promise<unsigned> background;
    std::promise<unsigned> demanded;
    auto backgroundDone = background.get_future();
    auto demandedDone = demanded.get_future();
    SubmitRegistrationPreparation([&] { background.set_value(order.fetch_add(1u)); });
    if (beforeSubmit) PrioritizeRegistrationPreparation(prepared);
    SubmitRegistrationPreparation([&] { demanded.set_value(order.fetch_add(1u)); }, &prepared);
    if (!beforeSubmit) {
        PrioritizeRegistrationPreparation(prepared);
        PrioritizeRegistrationPreparation(prepared);
    }
    release.release();
    const bool settled = demandedDone.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    const bool backgroundSettled = backgroundDone.wait_for(std::chrono::seconds(5)) == std::future_status::ready;
    release.release(static_cast<std::ptrdiff_t>(workers - 1u));
    finished.wait();
    Require(settled && backgroundSettled, "preparation queue did not complete its work");
    Require(demandedDone.get() == 0u && backgroundDone.get() == 1u && order.load() == 2u, "demanded preparation waited behind background work or ran twice");
}

}

int main() {
    try {
        Transactions();
        HeaderSelection();
        RegistryVersions();
        FusedHullHeaderSelection();
        TransactionLock();
        PreparationPriority(false);
        PreparationPriority(true);
        auto device = OpenVulkanTestDevice();
        if (!device) return VulkanTestSkipped;
        Registration(*device);
        std::cout << "shader publication and lifetime tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
