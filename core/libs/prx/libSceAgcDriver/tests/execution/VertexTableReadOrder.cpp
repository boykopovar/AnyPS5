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

        StopQueue();
        std::puts("vertex table read order tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        try { StopQueue(); } catch (...) {}
        return 1;
    }
}
