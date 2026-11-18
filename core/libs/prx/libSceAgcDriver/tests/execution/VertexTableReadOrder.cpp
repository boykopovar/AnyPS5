#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/GuestMemory.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "SceShaders.hpp"
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <span>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::DriverDetail::DrawPipeline;

void check(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error("vertex table read order: " + what);
}

constexpr std::size_t UserDataOffset = 288;
constexpr std::size_t DirectOffset = 352;
constexpr std::size_t SemanticOffset = 384;

alignas(64) std::array<std::uint32_t, 8> BufferTable{0x1000u, 0x20000u, 64u, 0x3000u, 0x7000u, 0x20000u, 32u, 0x3000u};
alignas(64) std::array<std::uint32_t, 1> AttribTable{(0x2cu << 5u) | 1u};
alignas(64) std::array<std::uint32_t, 4> Elsewhere{};
std::vector<std::byte> Header(SemanticOffset + sizeof(ShaderSemantic));

void BuildHeader() {
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 0;
    direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 2;
    std::memcpy(Header.data() + DirectOffset, direct.data(), sizeof(direct));
    ShaderSemantic semantic{};
    semantic.size_in_elements = 1;
    std::memcpy(Header.data() + SemanticOffset, &semantic, sizeof(semantic));
    ShaderUserData userData{};
    userData.direct_resource_offset = reinterpret_cast<std::uint16_t*>(Header.data() + DirectOffset);
    userData.direct_resource_count = ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT;
    std::memcpy(Header.data() + UserDataOffset, &userData, sizeof(userData));
    Shader shader{};
    shader.user_data = reinterpret_cast<ShaderUserData*>(Header.data() + UserDataOffset);
    shader.input_semantics = reinterpret_cast<ShaderSemantic*>(Header.data() + SemanticOffset);
    shader.num_input_semantics = 1;
    std::memcpy(Header.data(), &shader, sizeof(shader));
}

std::uint64_t Address(const void* data) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(data));
}

std::array<std::uint32_t, 4> UserWords() {
    const auto buffer = Address(BufferTable.data());
    const auto attrib = Address(AttribTable.data());
    return {static_cast<std::uint32_t>(buffer), static_cast<std::uint32_t>(buffer >> 32u), static_cast<std::uint32_t>(attrib), static_cast<std::uint32_t>(attrib >> 32u)};
}

ShaderRecompiler::ShaderVertexStageInfo Decode(std::vector<AgcDriver::Graphics::DecodeRead>& reads) {
    const auto userWords = UserWords();
    return AgcDriver::DriverDetail::DecodeOrderedVertexStageInfo(Header, Address(Header.data()), userWords, reads, 0);
}

struct Gate {
    std::atomic<bool> release{false};
};

std::vector<std::shared_ptr<Gate>> Gates;

std::shared_ptr<Gate> Hold(const void* range, std::size_t bytes, std::function<void()> store) {
    auto gate = Gates.emplace_back(std::make_shared<Gate>());
    DrawPipeline::Queue0().Enqueue([gate, store = std::move(store)] {
        while (!gate->release.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        store();
    }, {{Address(range), Address(range) + bytes}});
    return gate;
}

void StopQueue() {
    AgcDriver::GuestMemory::SetReadGuard(nullptr);
    for (const auto& gate : Gates) gate->release.store(true, std::memory_order_release);
    DrawPipeline::Active() = false;
    DrawPipeline::Queue0().Stop();
}

void WaitEmpty() {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (DrawPipeline::QueuedItems() != 0) {
        check(std::chrono::steady_clock::now() < deadline, "the pipeline never emptied");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

std::shared_ptr<Gate> completeAtBufferRead;
const void* completeAtRead = BufferTable.data();

void CompleteQueuedStoreAtBufferRead(std::uint64_t address, std::size_t) {
    if (address != Address(completeAtRead) || completeAtBufferRead == nullptr) return;
    std::exchange(completeAtBufferRead, nullptr)->release.store(true, std::memory_order_release);
    WaitEmpty();
}

std::shared_ptr<Gate> reclaimGate;

void CompleteQueuedStoreAndEnqueue(std::uint64_t address, std::size_t) {
    if (address != Address(completeAtRead) || completeAtBufferRead == nullptr) return;
    std::exchange(completeAtBufferRead, nullptr)->release.store(true, std::memory_order_release);
    WaitEmpty();
    DrawPipeline::Queue0().Enqueue([gate = reclaimGate] {
        while (!gate->release.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, {{Address(Elsewhere.data()), Address(Elsewhere.data()) + sizeof(Elsewhere)}});
}

std::shared_ptr<Gate> HoldRanges(std::vector<DrawPipeline::Range> writes) {
    auto gate = Gates.emplace_back(std::make_shared<Gate>());
    DrawPipeline::Queue0().Enqueue([gate] {
        while (!gate->release.load(std::memory_order_acquire)) std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }, std::move(writes));
    return gate;
}

void Release(const std::vector<std::shared_ptr<Gate>>& gates) {
    for (const auto& gate : gates) gate->release.store(true, std::memory_order_release);
}

void WaitRetired(std::uint64_t count) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (DrawPipeline::Queue0().Retired() < count) {
        check(std::chrono::steady_clock::now() < deadline, "the queued item never retired");
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

void WrittenSinceMatchesPairwiseOverlap() {
    std::uint64_t state = 0x9e3779b97f4a7c15ull;
    const auto next = [&state] {
        state ^= state << 13u;
        state ^= state >> 7u;
        state ^= state << 17u;
        return state;
    };
    for (std::size_t trial = 0; trial < 300; ++trial) {
        WaitEmpty();
        std::vector<std::vector<DrawPipeline::Range>> queued(1 + next() % 3);
        std::vector<std::shared_ptr<Gate>> held;
        for (auto& writes : queued) {
            writes.resize(1 + next() % 3);
            for (auto& write : writes) {
                write.first = 0x1000u + next() % 0x4000u;
                write.second = write.first + 1u + next() % 0x200u;
            }
            held.push_back(HoldRanges(writes));
        }
        std::vector<DrawPipeline::Range> reads(1 + next() % 4);
        for (auto& read : reads) {
            read.first = 0x1000u + next() % 0x4000u;
            read.second = read.first + next() % 0x80u;
        }
        bool expected = false;
        for (const auto& writes : queued) {
            for (const auto& write : writes) {
                for (const auto& read : reads) expected = expected || (read.first < write.second && write.first < read.second);
            }
        }
        auto watch = DrawPipeline::Queue0().WatchWrites();
        check(DrawPipeline::Queue0().WrittenSince(watch, reads) == expected, "WrittenSince disagrees with the pairwise overlap of the queued writes and the reads");
        Release(held);
        WaitEmpty();
    }
}

void RetiredItemsAreCheckedAfterTheyLeaveTheQueue() {
    WaitEmpty();
    auto& queue = DrawPipeline::Queue0();
    auto first = HoldRanges({{0x1000u, 0x1100u}});
    auto second = HoldRanges({{0x2000u, 0x2100u}});
    auto overlapsRetired = queue.WatchWrites();
    auto overlapsQueued = queue.WatchWrites();
    auto overlapsNothing = queue.WatchWrites();
    const auto before = queue.Retired();
    first->release.store(true, std::memory_order_release);
    WaitRetired(before + 1);
    const auto lookups = queue.CountedLookups();
    check(queue.WrittenSince(overlapsRetired, {{0x1080u, 0x1081u}}), "a write that retired while the watch was open was not seen");
    check(queue.WrittenSince(overlapsQueued, {{0x2080u, 0x2081u}}), "a write that is still queued was not seen");
    check(!queue.WrittenSince(overlapsNothing, {{0x3000u, 0x3100u}}), "a read that no queued or retired write overlaps was reported as written");
    check(queue.CountedLookups() == lookups + (DrawPipeline::CountedWatch() ? 2u : 0u), "the retired items were not looked up exactly when counted watches are on");
    second->release.store(true, std::memory_order_release);
    WaitEmpty();
}

void MoreRetiredThanQueuedIsWritten() {
    WaitEmpty();
    auto& queue = DrawPipeline::Queue0();
    auto gate = HoldRanges({{0x1000u, 0x1100u}});
    auto watch = queue.WatchWrites();
    gate->release.store(true, std::memory_order_release);
    WaitEmpty();
    queue.Enqueue([] {}, {{0x2000u, 0x2100u}});
    WaitEmpty();
    check(queue.WrittenSince(watch, {{0x3000u, 0x3100u}}), "an item queued after the watch that retired during it was not reported");
}

void ThrowingDecodeReleasesTheWatch() {
    WaitEmpty();
    auto gate = HoldRanges({{Address(Elsewhere.data()), Address(Elsewhere.data()) + sizeof(Elsewhere)}});
    std::vector<std::byte> truncated(4);
    std::vector<AgcDriver::Graphics::DecodeRead> reads;
    const auto words = UserWords();
    bool threw = false;
    try {
        static_cast<void>(AgcDriver::DriverDetail::DecodeOrderedVertexStageInfo(truncated, Address(truncated.data()), words, reads, 0));
    } catch (const std::runtime_error&) {
        threw = true;
    }
    check(threw, "a header smaller than the fixed AGC header did not throw");
    auto held = std::make_shared<int>(1);
    const std::weak_ptr<int> watched = held;
    DrawPipeline::Queue0().Enqueue([held = std::move(held)] {}, {});
    gate->release.store(true, std::memory_order_release);
    WaitEmpty();
    DrawPipeline::Queue0().Enqueue([] {}, {});
    check(watched.expired(), "a decode that threw left its watch open, so the retired items were never released");
    WaitEmpty();
}

ShaderRecompiler::ShaderVertexStageInfo DecodeReleasingAfterwards(std::vector<AgcDriver::Graphics::DecodeRead>& reads, const std::shared_ptr<Gate>& gate) {
    std::thread releaser([gate] {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        gate->release.store(true, std::memory_order_release);
    });
    ShaderRecompiler::ShaderVertexStageInfo info;
    try {
        info = Decode(reads);
    } catch (...) {
        releaser.join();
        throw;
    }
    releaser.join();
    return info;
}

ShaderRecompiler::ShaderVertexStageInfo DecodeBehindQueuedStore(const void* range, std::size_t bytes, std::function<void()> store, std::vector<AgcDriver::Graphics::DecodeRead>& reads) {
    WaitEmpty();
    auto gate = Hold(range, bytes, std::move(store));
    std::thread releaser([gate] {
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        gate->release.store(true, std::memory_order_release);
    });
    ShaderRecompiler::ShaderVertexStageInfo info;
    try {
        info = Decode(reads);
    } catch (...) {
        releaser.join();
        throw;
    }
    releaser.join();
    return info;
}

}

int main() {
    try {
        BuildHeader();
        DrawPipeline::Active() = true;
        std::vector<AgcDriver::Graphics::DecodeRead> reads;

        auto info = Decode(reads);
        check(info.fetchEmbedded && info.resourcesNum == 1 && info.resources[0].fields[0] == 0x7000u && info.resources[0].fields[2] == 32u, "the tables did not decode to the second vertex buffer");

        info = DecodeBehindQueuedStore(BufferTable.data() + 4, 16, [] {
            BufferTable[4] = 0x9000u;
            BufferTable[6] = 128u;
        }, reads);
        check(info.resources[0].fields[0] == 0x9000u && info.resources[0].fields[2] == 128u, "the vertex buffer V# was read before the queued store into the vertex buffer table");
        check(reads.size() == 2 && reads[1].address == Address(BufferTable.data() + 4) && std::memcmp(reads[1].Bytes().data(), BufferTable.data() + 4, 16) == 0, "the recorded V# read is not the stored V#");
        check(DrawPipeline::QueuedItems() == 0, "the store into the vertex buffer table is still queued");

        info = DecodeBehindQueuedStore(AttribTable.data(), 4, [] { AttribTable[0] = 0x2cu << 5u; }, reads);
        check(info.resources[0].fields[0] == 0x1000u && info.resources[0].fields[2] == 64u, "the attribute word was read before the queued store into the vertex attribute table");
        check(reads.size() == 2 && reads[0].address == Address(AttribTable.data()) && reads[1].address == Address(BufferTable.data()), "the decode did not read the stored attribute word and the V# it names");

        WaitEmpty();
        auto gate = Hold(Elsewhere.data(), sizeof(Elsewhere), [] {});
        info = Decode(reads);
        check(info.resources[0].fields[0] == 0x1000u, "the decode next to an unrelated queued store changed");
        check(DrawPipeline::QueuedItems() == 1, "a decode that overlaps nothing queued drained the pipeline");
        gate->release.store(true, std::memory_order_release);
        WaitEmpty();

        completeAtBufferRead = Hold(AttribTable.data(), 4, [] { AttribTable[0] = (0x2cu << 5u) | 1u; });
        AgcDriver::GuestMemory::SetReadGuard(&CompleteQueuedStoreAtBufferRead);
        auto drains = DrawPipeline::Queue0().DecodeDrains();
        info = Decode(reads);
        AgcDriver::GuestMemory::SetReadGuard(nullptr);
        check(completeAtBufferRead == nullptr, "the queued attribute store did not complete during the decode");
        check(info.resources[0].fields[0] == 0x9000u && info.resources[0].fields[2] == 128u, "the decode kept the vertex buffer of an attribute word that a queued store replaced during the decode");
        check(reads.size() == 2 && reads[1].address == Address(BufferTable.data() + 4), "the recorded reads are not the stored attribute word and the V# it names");
        check(DrawPipeline::Queue0().DecodeDrains() == drains + 1, "the drain the decode caused was not counted");

        WaitEmpty();
        auto finished = Hold(BufferTable.data() + 4, 16, [] {
            BufferTable[4] = 0xb000u;
            BufferTable[6] = 256u;
        });
        finished->release.store(true, std::memory_order_release);
        WaitEmpty();
        gate = Hold(Elsewhere.data(), sizeof(Elsewhere), [] {});
        drains = DrawPipeline::Queue0().DecodeDrains();
        info = Decode(reads);
        check(info.resources[0].fields[0] == 0xb000u && info.resources[0].fields[2] == 256u, "the decode did not read the V# a finished item stored");
        check(DrawPipeline::QueuedItems() == 1 && DrawPipeline::Queue0().DecodeDrains() == drains, "a decode of a table that a finished item wrote drained the pipeline");
        gate->release.store(true, std::memory_order_release);
        WaitEmpty();

        completeAtBufferRead = Hold(Elsewhere.data(), sizeof(Elsewhere), [] { Elsewhere[0] = 1u; });
        completeAtRead = BufferTable.data() + 4;
        AgcDriver::GuestMemory::SetReadGuard(&CompleteQueuedStoreAtBufferRead);
        drains = DrawPipeline::Queue0().DecodeDrains();
        info = Decode(reads);
        AgcDriver::GuestMemory::SetReadGuard(nullptr);
        check(completeAtBufferRead == nullptr, "the unrelated queued store did not complete during the decode");
        check(info.resources[0].fields[0] == 0xb000u && DrawPipeline::Queue0().DecodeDrains() == drains, "a store elsewhere that retired during the decode made the decode drain");

        WaitEmpty();
        reclaimGate = std::make_shared<Gate>();
        Gates.push_back(reclaimGate);
        completeAtBufferRead = Hold(AttribTable.data(), 4, [] { AttribTable[0] = 0x2cu << 5u; });
        completeAtRead = BufferTable.data() + 4;
        AgcDriver::GuestMemory::SetReadGuard(&CompleteQueuedStoreAndEnqueue);
        drains = DrawPipeline::Queue0().DecodeDrains();
        info = DecodeReleasingAfterwards(reads, reclaimGate);
        AgcDriver::GuestMemory::SetReadGuard(nullptr);
        check(completeAtBufferRead == nullptr, "the queued attribute store did not complete during the decode behind a reclaiming enqueue");
        check(info.resources[0].fields[0] == 0x1000u && info.resources[0].fields[2] == 64u, "the decode kept the stale vertex buffer when a retired store was written and the items were reclaimed during the decode");
        check(DrawPipeline::Queue0().DecodeDrains() == drains + 1, "the retired store the reclaim would have released was not drained for");
        WaitEmpty();

        reclaimGate = std::make_shared<Gate>();
        Gates.push_back(reclaimGate);
        completeAtBufferRead = Hold(Elsewhere.data(), sizeof(Elsewhere), [] { Elsewhere[1] = 1u; });
        completeAtRead = BufferTable.data();
        AgcDriver::GuestMemory::SetReadGuard(&CompleteQueuedStoreAndEnqueue);
        drains = DrawPipeline::Queue0().DecodeDrains();
        const auto lookups = DrawPipeline::Queue0().CountedLookups();
        info = DecodeReleasingAfterwards(reads, reclaimGate);
        AgcDriver::GuestMemory::SetReadGuard(nullptr);
        check(completeAtBufferRead == nullptr, "the unrelated queued store did not complete during the decode behind a reclaiming enqueue");
        check(info.resources[0].fields[0] == 0x1000u && DrawPipeline::Queue0().DecodeDrains() == drains, "an unrelated store that retired while another enqueue reclaimed the retired items made the decode drain");
        check(DrawPipeline::Queue0().CountedLookups() == lookups + (DrawPipeline::CountedWatch() ? 1u : 0u), "the retired items were not looked up exactly when counted watches are on");
        WaitEmpty();

        WrittenSinceMatchesPairwiseOverlap();
        RetiredItemsAreCheckedAfterTheyLeaveTheQueue();
        MoreRetiredThanQueuedIsWritten();
        ThrowingDecodeReleasesTheWatch();

        StopQueue();
        std::puts("vertex table read order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { StopQueue(); } catch (...) {}
        return 1;
    }
}
