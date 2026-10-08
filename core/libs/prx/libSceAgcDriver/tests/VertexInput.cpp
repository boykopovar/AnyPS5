#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <atomic>
#include <barrier>
#include <iostream>
#include <random>
#include <thread>

namespace {

using namespace AgcDriver::Graphics;
using ShaderRecompiler::VertexAttribute;
std::atomic<unsigned> queries{0};

Context context() {
    Context result{};
    result.limits.maxVertexInputBindings = 32;
    result.limits.maxVertexInputAttributes = 32;
    result.limits.maxVertexInputBindingStride = 2048;
    result.formatProperties = [](VkPhysicalDevice, VkFormat format, VkFormatProperties* properties) {
        ++queries;
        *properties = {};
        if (format != VK_FORMAT_R8_UINT) properties->bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
    };
    return result;
}

VertexAttribute attribute(std::uint32_t location = 0) {
    VertexAttribute result{};
    result.location = location;
    result.components = 4;
    result.resource.fields = {0x1000, 32u << 16u, 128, 77u << 12u};
    return result;
}

void same(const VertexInputLayout& a, const VertexInputLayout& b) {
    Require(a.bindings.size() == b.bindings.size() && a.attributes.size() == b.attributes.size() && a.alignments == b.alignments, "layout sizes differ");
    for (std::size_t i = 0; i < a.bindings.size(); ++i) {
        const auto& x = a.bindings[i];
        const auto& y = b.bindings[i];
        Require(x.binding == y.binding && x.stride == y.stride && x.inputRate == y.inputRate, "binding differs");
        const auto& p = a.attributes[i];
        const auto& q = b.attributes[i];
        Require(p.location == q.location && p.binding == q.binding && p.format == q.format && p.offset == q.offset, "attribute differs");
    }
}

template<class TAction> void fails(TAction&& fn) {
    bool failed = false;
    try { fn(); } catch (const std::runtime_error&) { failed = true; }
    Require(failed, "invalid vertex input was accepted");
}

void run() {
    auto ctx = context();
    VertexInputCache cache(ctx, 2);
    std::array attributes{attribute(0), attribute(1)};
    auto first = cache.Get(1, attributes);
    const auto count = queries.load();
    attributes[0].resource.fields[0] += 256;
    attributes[0].resource.fields[1] |= 0x12u;
    attributes[0].resource.fields[2] = 17;
    Require(cache.Get(1, attributes) == first && queries == count, "live addresses or counts rebuilt the layout");
    auto invalid = attributes;
    invalid[0].resource.fields[0] += 1;
    fails([&] { cache.Get(1, invalid); });
    invalid = attributes;
    invalid[0].resource.fields[0] = 0;
    invalid[0].resource.fields[1] &= 0xffff0000u;
    fails([&] { cache.Get(1, invalid); });
    for (unsigned field = 0; field < 8; ++field) {
        invalid = attributes;
        switch (field) {
            case 0: invalid[0].components = 0; break;
            case 1: invalid[0].fetchIndex = 2; break;
            case 2: invalid[0].location = invalid[1].location; break;
            case 3: invalid[0].resource.fields[1] |= 0x80000000u; break;
            case 4: invalid[0].resource.fields[3] |= 0x00800000u; break;
            case 5: invalid[0].resource.fields[3] |= 0x40000000u; break;
            case 6: invalid[0].resource.fields[1] = 4096u << 16u; break;
            case 7: invalid[0].resource.fields[3] = 5u << 12u; break;
        }
        fails([&] { cache.Get(1, invalid); });
        fails([&] { BuildVertexInputLayout(ctx, invalid); });
    }
    attributes[0].resource.fields[1] = 16u << 16u;
    auto second = cache.Get(2, attributes);
    Require(first != second, "stride change reused a layout");
    attributes[0].fetchIndex = 1;
    auto third = cache.Get(3, attributes);
    Require(third != second && cache.Size() == 2 && first->bindings[0].stride == 32, "eviction or retained lifetime failed");
    attributes[0] = attribute(0);
    Require(cache.Get(1, attributes) != first, "least recently used layout survived eviction");
    Require(cache.Get(0, attributes) != cache.Get(0, attributes), "unidentified variant was cached");
    VertexInputCache other(ctx);
    Require(other.Get(1, attributes) != cache.Get(1, attributes), "device caches shared ownership");
    auto restricted = ctx;
    restricted.limits.maxVertexInputBindings = 1;
    VertexInputCache restrictedCache(restricted);
    fails([&] { restrictedCache.Get(1, attributes); });
    Require(cache.Get(1, {})->attributes.empty(), "empty layout failed");
    std::mt19937 random(719);
    for (unsigned i = 0; i < 10000; ++i) {
        std::vector<VertexAttribute> input;
        const auto size = random() % 9;
        for (unsigned n = 0; n < size; ++n) {
            auto a = attribute(n);
            a.components = 1 + random() % 4;
            a.fetchIndex = random() % 2;
            a.resource.fields[0] += (random() % 4096) * 16;
            a.resource.fields[1] = (random() % 33) * 16u << 16u;
            a.resource.fields[2] = random();
            const std::array formats{13u, 22u, 36u, 50u, 56u, 64u, 71u, 74u, 77u};
            a.resource.fields[3] = formats[random() % formats.size()] << 12u;
            input.push_back(a);
        }
        const auto expected = BuildVertexInputLayout(ctx, input);
        same(*cache.Get(1 + i % 16, input), expected);
        for (auto& a : input) a.resource.fields[0] += 256;
        same(*cache.Get(1 + i % 16, input), BuildVertexInputLayout(ctx, input));
    }
    VertexInputCache concurrent(ctx);
    std::array<std::shared_ptr<const VertexInputLayout>, 8> results;
    std::barrier ready(8);
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < results.size(); ++i) threads.emplace_back([&, i] {
        ready.arrive_and_wait();
        results[i] = concurrent.Get(1, attributes);
    });
    for (auto& thread : threads) thread.join();
    for (const auto& result : results) Require(result == results[0], "concurrent cache identity differs");
}

}

int main() {
    try {
        run();
        std::cout << "Vertex input cache tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
