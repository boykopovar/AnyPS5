#include "prx/libSceAgcDriver/Graphics/include/VertexInput.hpp"
#include <atomic>
#include <barrier>
#include <chrono>
#include <ctime>
#include <string_view>
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
    Require(a.bindings.size() == b.bindings.size() && a.attributes.size() == b.attributes.size(), "layout sizes differ");
    for (std::size_t i = 0; i < a.bindings.size(); ++i) {
        const auto& x = a.bindings[i];
        const auto& y = b.bindings[i];
        Require(x.binding == y.binding && x.stride == y.stride && x.inputRate == y.inputRate, "binding differs");
        const auto& p = a.attributes[i];
        const auto& q = b.attributes[i];
        Require(p.location == q.location && p.binding == q.binding && p.format == q.format && p.offset == q.offset, "attribute differs");
    }
}

std::shared_ptr<const VertexInputLayout> get(VertexInputCache& cache, const Context& ctx, std::uint64_t variant, std::span<const VertexAttribute> attributes) {
    auto result = cache.TryGet(variant, attributes);
    return result != nullptr ? result : std::make_shared<const VertexInputLayout>(BuildVertexInputLayout(ctx, attributes));
}

template<class TAction> void fails(TAction&& fn) {
    bool failed = false;
    try { fn(); } catch (const std::runtime_error&) { failed = true; }
    Require(failed, "invalid vertex input was accepted");
}

void descriptorDiagnostics(const Context& ctx) {
    auto input = attribute(7);
    input.resource.fields = {0x89abcde0u, 0x0010abcdu, 0x12345678u, 0x0104dfacu};
    Require(BuildVertexInputLayout(ctx, std::span(&input, 1)).attributes.size() == 1, "valid diagnostic control was rejected");
    struct Case {
        unsigned field;
        std::uint32_t bits;
        const char* words;
    };
    constexpr std::array cases{
        Case{1, 0x80000000u, "V#=[89abcde0 8010abcd 12345678 0104dfac]"},
        Case{3, 0x00800000u, "V#=[89abcde0 0010abcd 12345678 0184dfac]"},
        Case{3, 0x40000000u, "V#=[89abcde0 0010abcd 12345678 4104dfac]"},
        Case{3, 0x80000000u, "V#=[89abcde0 0010abcd 12345678 8104dfac]"},
        Case{3, 0xc0800000u, "V#=[89abcde0 0010abcd 12345678 c184dfac]"}
    };
    for (const auto& value : cases) {
        auto described = input;
        described.resource.fields[value.field] |= value.bits;
        const auto before = described.resource.fields;
        const std::string message(VertexDescriptorFlagsMessage(described).data());
        Require(message.find("unsupported vertex buffer descriptor flags at location 7") != std::string::npos && message.find(value.words) != std::string::npos,
            "vertex flag message did not identify the attribute and all four descriptor words: " + message);
        Require(described.resource.fields == before, "formatting changed the descriptor");
    }
}

void run() {
    auto ctx = context();
    descriptorDiagnostics(ctx);
    VertexInputCache cache(ctx, 2);
    std::array attributes{attribute(0), attribute(1)};
    auto first = get(cache, ctx, 1, attributes);
    const auto count = queries.load();
    attributes[0].resource.fields[0] += 256;
    attributes[0].resource.fields[1] |= 0x12u;
    attributes[0].resource.fields[2] = 17;
    Require(get(cache, ctx, 1, attributes) == first && queries == count, "live addresses or counts rebuilt the layout");
    auto invalid = attributes;
    invalid[0].resource.fields[0] += 1;
    fails([&] { get(cache, ctx, 1, invalid); });
    invalid = attributes;
    invalid[0].resource.fields[0] = 0;
    invalid[0].resource.fields[1] &= 0xffff0000u;
    fails([&] { get(cache, ctx, 1, invalid); });
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
        fails([&] { get(cache, ctx, 1, invalid); });
        fails([&] { BuildVertexInputLayout(ctx, invalid); });
    }
    attributes[0].resource.fields[1] = 16u << 16u;
    auto second = get(cache, ctx, 2, attributes);
    Require(first != second, "stride change reused a layout");
    attributes[0].fetchIndex = 1;
    auto third = get(cache, ctx, 3, attributes);
    Require(third != second && cache.Size() == 2 && first->bindings[0].stride == 32, "eviction or retained lifetime failed");
    attributes[0] = attribute(0);
    Require(get(cache, ctx, 1, attributes) != first, "least recently used layout survived eviction");
    Require(get(cache, ctx, 0, attributes) != get(cache, ctx, 0, attributes), "unidentified variant was cached");
    VertexInputCache other(ctx);
    Require(get(other, ctx, 1, attributes) != get(cache, ctx, 1, attributes), "device caches shared ownership");
    auto restricted = ctx;
    restricted.limits.maxVertexInputBindings = 1;
    VertexInputCache restrictedCache(restricted);
    fails([&] { get(restrictedCache, restricted, 1, attributes); });
    Require(get(cache, ctx, 1, {})->attributes.empty(), "empty layout failed");
    VertexInputCache generated(ctx, 16);
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
        same(*get(generated, ctx, 1 + i, input), expected);
        for (auto& a : input) a.resource.fields[0] += 256;
        same(*get(generated, ctx, 1 + i, input), BuildVertexInputLayout(ctx, input));
    }
    {
        VertexInputCache live(ctx);
        std::array input{attribute()};
        const auto stored = live.TryGet(100, input);
        for (const auto records : {0u, 1u, 128u}) {
            input[0].resource.fields[2] = records;
            Require(live.TryGet(100, input) == stored, "record count rebuilt a vertex layout");
            Require(VertexBufferReadSize(input[0], 5, 1) == 176u, "record count changed the direct fetch extent");
            if (records != 0) {
                Require(VertexBufferExtent(input[0]) == 32u * (records - 1u) + 16u, "cached layout froze the indirect descriptor extent");
                Require(VertexBufferExtent(input[0]) == VertexBufferReadSize(input[0], records - 1u, 1), "the indirect extent is not the fetch of the last record");
            } else {
                fails([&] { VertexBufferExtent(input[0]); });
            }
        }
        auto last = attribute();
        last.resource.fields[0] += 16u;
        Require(16u + VertexBufferExtent(last) == 32u * 128u, "the last element of each record was copied past the descriptor's records");
        input[0].resource.fields[1] = 0;
        input[0].resource.fields[3] |= 2u << 28u;
        input[0].resource.fields[2] = 0;
        const auto zeroStride = live.TryGet(101, input);
        Require(VertexFetchOutOfRange(input[0]), "zero records did not select zero fill");
        input[0].resource.fields[2] = 64;
        Require(live.TryGet(101, input) == zeroStride && !VertexFetchOutOfRange(input[0]) && VertexBufferExtent(input[0]) == 16, "cached zero-stride layout froze the live zero-fill decision");
        input[0].resource.fields[2] = 0;
        Require(live.TryGet(101, input) == zeroStride && VertexFetchOutOfRange(input[0]), "record count zero reused a live fetch");
        input[0] = attribute();
        input[0].resource.fields[1] = 0;
        input[0].resource.fields[3] |= 3u << 28u;
        input[0].resource.fields[2] = 16;
        const auto wholeFetch = live.TryGet(105, input);
        for (const auto records : {0u, 8u, 15u}) {
            input[0].resource.fields[2] = records;
            Require(live.TryGet(105, input) == wholeFetch && VertexFetchOutOfRange(input[0]), "a fetch that only partly fits was not read as zero");
        }
        for (const auto records : {16u, 17u, 0xffffffffu}) {
            input[0].resource.fields[2] = records;
            Require(live.TryGet(105, input) == wholeFetch && !VertexFetchOutOfRange(input[0]) && VertexBufferExtent(input[0]) == 16, "a fetch inside the byte range was read as zero or sized by NUM_RECORDS");
        }
        input[0].resource.fields[3] &= ~(3u << 28u);
        Require(VertexFetchOutOfRange(input[0]), "OOB_SELECT 0 read a zero-stride fetch");
        input[0] = attribute();
        input[0].formatComponents = 1;
        const auto narrow = live.TryGet(102, input);
        input[0].formatComponents = 2;
        const auto wide = get(live, ctx, 102, input);
        Require(narrow->attributes[0].format == VK_FORMAT_R32_SFLOAT && wide->attributes[0].format == VK_FORMAT_R32G32_SFLOAT, "format component count reused the wrong vertex layout");
        input[0] = {};
        input[0].components = 4;
        input[0].formatComponents = 1;
        const auto nullLayout = live.TryGet(103, input);
        Require(live.TryGet(103, input) == nullLayout && nullLayout->attributes[0].format == VK_FORMAT_R32G32B32A32_SFLOAT, "null vertex descriptor was rejected or narrowed on reuse");
        input[0].resource.fields[2] = 1;
        fails([&] { get(live, ctx, 103, input); });
        input[0] = attribute();
        input[0].resource.fields[3] |= 2u << 28u;
        input[0].resource.fields[2] = 6;
        const auto bounded = live.TryGet(104, input);
        Require(VertexBufferReadSize(input[0], 5, 1) == 176, "bounded vertex fetch failed");
        input[0].resource.fields[2] = 5;
        Require(live.TryGet(104, input) == bounded, "bounded record count rebuilt the layout");
        fails([&] { VertexBufferReadSize(input[0], 5, 1); });
    }
    {
        VertexInputCache thrashing(ctx, 2);
        std::array input{attribute()};
        const auto kept = thrashing.TryGet(1, input);
        thrashing.TryGet(2, input);
        thrashing.TryGet(3, input);
        const auto queryCount = queries.load();
        for (unsigned i = 0; i < 1000; ++i) Require(thrashing.TryGet(1, input) == nullptr, "an evicted variant kept re-entering the cache");
        Require(queries == queryCount && thrashing.Size() == 2 && kept->bindings[0].stride == 32, "bypassed variant rebuilt or invalidated a retained layout");
        Require(thrashing.TryGet(65537, input) == nullptr, "bypass hash collision was not conservative");
        same(*get(thrashing, ctx, 65537, input), BuildVertexInputLayout(ctx, input));
        input[0].resource.fields[0] = 1;
        fails([&] { get(thrashing, ctx, 1, input); });
        Require(thrashing.TryGet(0, input) == nullptr && thrashing.TryGet(2, {}) == nullptr, "empty or unidentified layout entered the cache");
        input[0] = attribute();
        input[0].resource.fields[1] = 16u << 16u;
        Require(thrashing.TryGet(2, input) == nullptr, "a changing variant shape stayed cached");
        const auto afterChange = queries.load();
        input[0].resource.fields[1] = 32u << 16u;
        for (unsigned i = 0; i < 1000; ++i) Require(thrashing.TryGet(2, input) == nullptr, "a changing variant shape kept taking the cache path");
        Require(queries == afterChange, "changing shape rebuilt in the cache");
    }
    VertexInputCache concurrent(ctx);
    std::array<std::shared_ptr<const VertexInputLayout>, 8> results;
    std::barrier ready(8);
    std::vector<std::thread> threads;
    for (unsigned i = 0; i < results.size(); ++i) threads.emplace_back([&, i] {
        ready.arrive_and_wait();
        results[i] = concurrent.TryGet(1, attributes);
    });
    for (auto& thread : threads) thread.join();
    for (const auto& result : results) Require(result == results[0], "concurrent cache identity differs");
    VertexInputCache evicting(ctx, 4);
    const auto expected = BuildVertexInputLayout(ctx, attributes);
    threads.clear();
    for (unsigned i = 0; i < 8; ++i) threads.emplace_back([&, i] {
        ready.arrive_and_wait();
        for (unsigned call = 0; call < 1000; ++call) same(*get(evicting, ctx, 1 + (call + i * 17) % 64, attributes), expected);
    });
    for (auto& thread : threads) thread.join();
    Require(evicting.Size() <= 4, "concurrent evictions exceeded the layout budget");
}

template<bool TCached> void benchmark() {
    auto ctx = context();
    ctx.formatProperties = [](VkPhysicalDevice, VkFormat, VkFormatProperties* properties) {
        *properties = {};
        properties->bufferFeatures = VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT;
    };
    for (const unsigned count : {0u, 1u, 4u, 8u}) {
        for (const auto scenario : {"reuse-1", "reuse-64", "churn-2048", "evicted", "shape-churn"}) {
            VertexInputCache cache(ctx);
            std::vector<VertexAttribute> input;
            for (unsigned i = 0; i < count; ++i) input.push_back(attribute(i));
            const std::string_view name(scenario);
            const unsigned variants = name == "reuse-64" ? 64u : name == "churn-2048" ? 2048u : name == "evicted" ? 1024u : 1u;
            if (name == "evicted") for (unsigned variant = 1; variant <= 2048; ++variant) cache.TryGet(variant, input);
            std::vector<double> samples;
            std::vector<double> wallSamples;
            std::uint64_t checksum = 0;
            unsigned hits = 0;
            constexpr unsigned calls = 32768;
            for (unsigned pass = 0; pass < 10; ++pass) {
                hits = 0;
                const auto start = std::chrono::steady_clock::now();
                const auto cpuStart = std::clock();
                for (unsigned call = 0; call < calls; ++call) {
                    if constexpr (TCached) {
                        if (input.empty()) continue;
                    }
                    if (name == "shape-churn" && !input.empty()) input[0].resource.fields[1] = (32u + (call % 2u) * 16u) << 16u;
                    const auto variant = 1u + call % variants;
                    VertexInputLayout owned;
                    std::shared_ptr<const VertexInputLayout> retained;
                    if constexpr (TCached) {
                        if (!input.empty()) retained = cache.TryGet(variant, input);
                    }
                    if (retained == nullptr) owned = BuildVertexInputLayout(ctx, input);
                    const auto& layout = retained != nullptr ? *retained : owned;
                    hits += retained != nullptr;
                    checksum += layout.bindings.size();
                    for (const auto& binding : layout.bindings) checksum += binding.binding + binding.stride + binding.inputRate;
                    for (const auto& attribute : layout.attributes) checksum += attribute.location + attribute.binding + attribute.format + attribute.offset;
                }
                const auto cpuElapsed = static_cast<double>(std::clock() - cpuStart) * 1e9 / CLOCKS_PER_SEC / calls;
                const auto wallElapsed = std::chrono::duration<double, std::nano>(std::chrono::steady_clock::now() - start).count() / calls;
                if (pass != 0) {
                    samples.push_back(cpuElapsed);
                    wallSamples.push_back(wallElapsed);
                }
            }
            std::sort(samples.begin(), samples.end());
            std::sort(wallSamples.begin(), wallSamples.end());
            std::cout << count << ' ' << name << ": " << samples[4] << " ns/call, wall " << wallSamples[4] << " ns/call, hits " << hits << '/' << calls << ", checksum " << checksum << '\n';
        }
    }
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 3 && std::string_view(argv[1]) == "--benchmark-vertex-input") {
            const std::string_view mode(argv[2]);
            Require(mode == "baseline" || mode == "cache", "unknown vertex benchmark mode");
            if (mode == "cache") benchmark<true>();
            else benchmark<false>();
            return 0;
        }
        run();
        std::cout << "Vertex input cache tests passed\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
