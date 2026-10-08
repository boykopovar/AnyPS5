#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "prx/libc/include/GuestAllocations.hpp"
#include "SceShaders.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <limits>
#include <map>
#include <random>
#include <stdexcept>
#include <thread>
#if defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using namespace AgcDriver::Graphics;
using Info = ShaderRecompiler::ShaderVertexStageInfo;
constexpr auto bufferType = static_cast<unsigned>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable);
constexpr auto attributeType = static_cast<unsigned>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable);

void require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<class TAction> void fails(TAction&& action) {
    try { action(); } catch (const std::runtime_error&) { return; }
    throw std::runtime_error("invalid vertex table was accepted");
}

struct Header {
    Shader shader{};
    ShaderUserData user{};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> offsets;
    std::array<ShaderSemantic, 32> semantics{};

    Header() {
        shader.user_data = &user;
        shader.input_semantics = semantics.data();
        shader.num_input_semantics = 1;
        user.direct_resource_count = offsets.size();
        user.direct_resource_offset = offsets.data();
        offsets.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
        offsets[bufferType] = 0;
        offsets[attributeType] = 2;
        semantics[0].size_in_elements = 4;
    }
    Header(const Header&) = delete;
    Header& operator=(const Header&) = delete;
    VertexStagePlan plan() const {
        return {std::as_bytes(std::span(this, 1)), reinterpret_cast<std::uintptr_t>(this)};
    }
};

struct Tables {
    std::array<std::uint32_t, 256> attributes{};
    std::array<std::array<std::uint32_t, 4>, 32> buffers{};
    Tables() {
        GuestAllocations::Mutation mutation;
        mutation.Add(this, sizeof(*this), true, true);
    }
    ~Tables() { GuestAllocations::Mutation mutation; mutation.Remove(this); }
    Tables(const Tables&) = delete;
    Tables& operator=(const Tables&) = delete;
};

std::array<std::uint32_t, 4> roots(std::uint64_t buffers, std::uint64_t attributes) {
    return {static_cast<std::uint32_t>(buffers), static_cast<std::uint32_t>(buffers >> 32), static_cast<std::uint32_t>(attributes), static_cast<std::uint32_t>(attributes >> 32)};
}

auto roots(const Tables& tables) {
    return roots(reinterpret_cast<std::uintptr_t>(tables.buffers.data()), reinterpret_cast<std::uintptr_t>(tables.attributes.data()));
}

void expectBytes(const Header& header, const Tables& tables, std::span<const DecodeRead> reads) {
    std::map<std::uintptr_t, std::byte> expected, actual;
    const auto add = [](auto& result, const void* pointer, std::size_t bytes) {
        const auto* data = static_cast<const std::byte*>(pointer);
        for (std::size_t i = 0; i < bytes; ++i) result[reinterpret_cast<std::uintptr_t>(data + i)] = data[i];
    };
    for (unsigned i = 0; i < header.shader.num_input_semantics; ++i) {
        const auto* attribute = &tables.attributes[header.semantics[i].semantic];
        add(expected, attribute, 4);
        add(expected, tables.buffers[*attribute & 31].data(), 16);
    }
    for (const auto& read : reads) {
        for (std::size_t i = 0; i < read.bytes.size(); ++i) {
            require(actual.emplace(read.address + i, read.bytes[i]).second, "descriptor byte was read twice");
        }
    }
    require(actual == expected, "decode observed unrelated or missing bytes");
}

void liveInputs() {
    Header header;
    header.shader.num_input_semantics = 5;
    const std::array<unsigned, 5> ids{255, 2, 1, 2, 128};
    for (unsigned i = 0; i < ids.size(); ++i) {
        header.semantics[i].semantic = ids[i];
        header.semantics[i].hardware_mapping = i * 4;
        header.semantics[i].size_in_elements = i % 4 + 1;
    }
    const auto plan = header.plan();
    Tables first, second;
    std::mt19937 random(18243);
    for (unsigned pass = 0; pass < 128; ++pass) {
        auto& tables = pass % 2 ? first : second;
        for (auto& word : tables.attributes) word = random() % 32;
        for (auto& buffer : tables.buffers) for (auto& word : buffer) word = random();
        std::vector<DecodeRead> reads;
        const auto info = plan.Read(roots(tables), &reads);
        require(info.resourcesNum == ids.size() && info.fetchEmbedded && info.fetchBufferReg == 0 && info.fetchAttribReg == 2, "vertex metadata changed");
        for (unsigned i = 0; i < ids.size(); ++i) {
            require(info.resources[i].fields == tables.buffers[tables.attributes[ids[i]]], "changed descriptor or root was not read");
            const auto& destination = info.resourcesDst[i];
            require(destination.attrId == static_cast<int>(ids[i]) && destination.registerStart == static_cast<int>(i * 4) && destination.registersNum == static_cast<int>(i % 4 + 1), "semantic order or duplicates changed");
        }
        expectBytes(header, tables, reads);
    }
    header.semantics.fill({});
    require(plan.Read(roots(first)).resourcesDst[0].attrId == 255, "plan retained mutable header pointers");
}

void fullTableAndFormats() {
    Header header;
    Tables tables;
    header.shader.num_input_semantics = 32;
    for (unsigned i = 0; i < 32; ++i) {
        header.semantics[i].semantic = 31 - i;
        tables.attributes[i] = i;
        tables.buffers[i] = {0xfffffff0u, 0x00200001u, 100u + i, 0x12345678u};
    }
    const auto plan = header.plan();
    std::vector<DecodeRead> reads;
    auto info = plan.Read(roots(tables), &reads);
    require(info.resourcesNum == 32 && reads.size() == 2 && reads[0].bytes.size() == 128 && reads[1].bytes.size() == 512, "full buffer mask did not form two exact reads");
    expectBytes(header, tables, reads);
    for (unsigned i = 0; i < 32; ++i) require(info.resources[i].fields == tables.buffers[31 - i], "full table changed result ordering");
    tables.attributes[31] |= ((77u * 4u + 3u) << 5u) | (32u << 14u) | (1u << 26u);
    info = plan.Read(roots(tables));
    require(info.resources[0].fields == std::array<std::uint32_t, 4>{0x10u, 0x00200002u, 131u, 0x1234dfacu}, "live format, channels, or address carry was lost");
    require(info.resourcesDst[0].fetchIndex == 1, "live instance fetch index was lost");
}

void invalidInputs() {
    Header header;
    Tables tables;
    auto user = roots(tables);
    const auto valid = header.plan();
    fails([&] { valid.Read(std::span(user).first(1)); });
    fails([&] { valid.Read(std::span(user).first(3)); });
    fails([&] { valid.Read(roots(0, 1)); });
    fails([&] { valid.Read(roots(1, 0)); });
    header.shader.user_data = nullptr;
    fails([&] { header.plan(); });
    header.shader.user_data = reinterpret_cast<ShaderUserData*>(std::numeric_limits<std::uintptr_t>::max() - 7);
    fails([&] { header.plan(); });
    header.shader.user_data = &header.user;
    header.user.direct_resource_count = header.offsets.size() + 1;
    fails([&] { header.plan(); });
    header.user.direct_resource_count = header.offsets.size();
    header.user.direct_resource_offset = nullptr;
    fails([&] { header.plan(); });
    header.user.direct_resource_offset = header.offsets.data();
    header.offsets[bufferType] = ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET;
    fails([&] { header.plan(); });
    header.offsets[bufferType] = 0;
    for (const auto count : {0u, 33u}) {
        header.shader.num_input_semantics = count;
        fails([&] { header.plan(); });
    }
    header.shader.num_input_semantics = 1;
    header.shader.input_semantics = nullptr;
    fails([&] { header.plan(); });
    header.shader.input_semantics = header.semantics.data();
    header.semantics[0].static_vb_index = 1;
    fails([&] { header.plan(); });
    header.semantics[0].static_vb_index = 0;
    header.semantics[0].static_attribute = 1;
    fails([&] { header.plan(); });
    header.semantics[0].static_attribute = 0;
    header.semantics[0].semantic = 255;
    const auto high = header.plan();
    fails([&] { high.Read(roots(1, std::numeric_limits<std::uintptr_t>::max() - 3)); });
    header.offsets[attributeType] = ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET;
    require(header.plan().Read({}).resourcesNum == 0, "shader without attribute table requires roots");
    fails([&] { VertexStagePlan({}, 0); });
}

void parallelReads() {
    Header header;
    const auto plan = header.plan();
    std::atomic<bool> okay{true};
    std::array<std::thread, 8> threads;
    for (unsigned i = 0; i < threads.size(); ++i) threads[i] = std::thread([&, i] {
        Tables tables;
        for (unsigned n = 0; n < 256; ++n) {
            tables.attributes[0] = (n + i) % 32;
            tables.buffers[tables.attributes[0]][2] = n + 1024 * i;
            if (plan.Read(roots(tables)).resources[0].fields[2] != n + 1024 * i) okay = false;
        }
    });
    for (auto& thread : threads) thread.join();
    require(okay, "shared plan mixed concurrent live inputs");
}

void guardBoundary() {
#if defined(__linux__)
    const auto page = static_cast<std::size_t>(sysconf(_SC_PAGESIZE));
    auto* memory = static_cast<std::byte*>(mmap(nullptr, page * 2, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    require(memory != MAP_FAILED, "guard allocation failed");
    require(mprotect(memory + page, page, PROT_NONE) == 0, "guard protection failed");
    { GuestAllocations::Mutation mutation; mutation.Add(memory, page, true, true); }
    Header header;
    const auto plan = header.plan();
    const std::array<std::uint32_t, 4> descriptor{0x1000, 0x00200000, 127, 0};
    std::memcpy(memory + page - 16, descriptor.data(), 16);
    std::uint32_t attribute = 31;
    std::memcpy(memory, &attribute, 4);
    const auto bufferBase = reinterpret_cast<std::uintptr_t>(memory + page - 512);
    require(plan.Read(roots(bufferBase, reinterpret_cast<std::uintptr_t>(memory))).resources[0].fields == descriptor, "last buffer descriptor failed");
    attribute = 0;
    std::memcpy(memory + page - 4, &attribute, 4);
    std::memcpy(memory, descriptor.data(), 16);
    require(plan.Read(roots(reinterpret_cast<std::uintptr_t>(memory), reinterpret_cast<std::uintptr_t>(memory + page - 4))).resources[0].fields == descriptor, "last attribute word failed");
    { GuestAllocations::Mutation mutation; mutation.Remove(memory); }
    munmap(memory, page * 2);
#endif
}

}

int main() {
    try {
        liveInputs();
        fullTableAndFormats();
        invalidInputs();
        parallelReads();
        guardBoundary();
        std::cout << "vertex stage plans passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
