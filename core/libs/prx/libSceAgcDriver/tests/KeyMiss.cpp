#include "prx/libSceAgcDriver/Graphics/include/ShaderResources.hpp"
#include <cstdio>
#include <optional>
#include <stdexcept>
#include <vector>

using namespace AgcDriver;
using Graphics::ContentKeyComponent;
using Graphics::ContentKeyDiff;
using Graphics::ShaderResources;
using ShaderRecompiler::DescriptorRole;

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Binding {
    DescriptorRole role;
    std::vector<std::uint32_t> words;
    std::uint32_t count = 1;
    std::vector<std::uint32_t> shapes;
};

std::vector<std::uint32_t> shaderKey(bool dataWords, bool movable, const std::vector<Binding>& bindings) {
    std::vector<std::uint32_t> key{dataWords ? 1u : 0u, 0u, 7u, 0u, static_cast<std::uint32_t>(bindings.size())};
    for (const auto& binding : bindings) {
        const bool data = binding.role == DescriptorRole::ShaderData || binding.role == DescriptorRole::FlattenedSrt;
        for (const auto word : {0u, static_cast<std::uint32_t>(binding.role), 0u, 0u, binding.count, 0u, 0u, static_cast<std::uint32_t>(binding.words.size())}) key.push_back(word);
        const bool emitted = (movable && binding.role == DescriptorRole::GuestBuffers && binding.words.size() == binding.count * 4u) || (dataWords && !movable) || !data;
        if (emitted) key.insert(key.end(), binding.words.begin(), binding.words.end());
        for (int flags = 0; flags < 9; ++flags) key.push_back(0u);
        key.push_back(0u);
        key.push_back(static_cast<std::uint32_t>(binding.shapes.size()));
        key.insert(key.end(), binding.shapes.begin(), binding.shapes.end());
    }
    return key;
}

std::vector<std::uint32_t> drawKey(std::uint32_t device, const std::vector<std::uint32_t>& stage, std::uint32_t target) {
    std::vector<std::uint32_t> key{0xffffffffu, device, 0u, 1u, static_cast<std::uint32_t>(stage.size())};
    key.insert(key.end(), stage.begin(), stage.end());
    key.push_back(1u);
    for (const auto word : {target, 0u, 0x100u, 0u, 0u, 0u, 0u, 0u}) key.push_back(word);
    return key;
}

std::vector<Binding> bindings() {
    return {{DescriptorRole::GuestBuffers, {10, 11, 12, 13}}, {DescriptorRole::GuestImages, {20, 21, 22, 23, 24, 25, 26, 27}}, {DescriptorRole::GuestSamplers, {30, 31, 32, 33}}, {DescriptorRole::ShaderData, {40, 41, 42, 43}}};
}

void expectOnly(const ContentKeyDiff& diff, std::optional<ContentKeyComponent> component, const char* message) {
    for (std::size_t i = 0; i < diff.size(); ++i) {
        check(diff[i] == (component && i == static_cast<std::size_t>(*component)), message);
    }
}

void testDispatchKey() {
    ContentKeyDiff diff;
    const auto base = shaderKey(true, false, bindings());
    check(ShaderResources::DiffContentKeys(base, base, false, diff), "equal keys were not comparable");
    expectOnly(diff, std::nullopt, "equal keys reported a difference");
    const auto change = [&](std::size_t binding, std::size_t word, ContentKeyComponent expected, const char* message) {
        auto changed = bindings();
        changed[binding].words[word] += 1;
        check(ShaderResources::DiffContentKeys(base, shaderKey(true, false, changed), false, diff), message);
        expectOnly(diff, expected, message);
    };
    change(0, 0, ContentKeyComponent::BufferWord0, "V# word 0");
    change(0, 1, ContentKeyComponent::BufferWord1, "V# word 1");
    change(0, 2, ContentKeyComponent::BufferWord2, "V# word 2");
    change(0, 3, ContentKeyComponent::BufferWord3, "V# word 3");
    change(1, 5, ContentKeyComponent::Image, "image descriptor word");
    change(2, 1, ContentKeyComponent::Sampler, "sampler word");
    change(3, 2, ContentKeyComponent::ShaderData, "shader data word");
    auto two = bindings();
    two[0].words[0] += 1;
    two[0].words[3] += 1;
    check(ShaderResources::DiffContentKeys(base, shaderKey(true, false, two), false, diff), "two components");
    check(diff[static_cast<std::size_t>(ContentKeyComponent::BufferWord0)] && diff[static_cast<std::size_t>(ContentKeyComponent::BufferWord3)] && !diff[static_cast<std::size_t>(ContentKeyComponent::BufferWord1)], "two components in one miss are both counted");
    auto fewer = bindings();
    fewer.pop_back();
    check(!ShaderResources::DiffContentKeys(base, shaderKey(true, false, fewer), false, diff), "different shapes were compared");
    expectOnly(diff, ContentKeyComponent::Layout, "shape change is a layout change");
    const auto noData = shaderKey(false, false, bindings());
    auto changedData = bindings();
    changedData[3].words[0] += 1;
    check(ShaderResources::DiffContentKeys(noData, shaderKey(false, false, changedData), false, diff), "data words left out");
    expectOnly(diff, std::nullopt, "data words are not part of the key without dataWords");
    std::vector<Binding> table{{DescriptorRole::ImageTable, {50, 51, 52, 53, 54, 55, 56, 57}, 1, {1}}, {DescriptorRole::SamplerTable, {60, 61, 62, 63}}};
    const auto tableBase = shaderKey(true, false, table);
    auto tableWord = table;
    tableWord[0].words[2] += 1;
    check(ShaderResources::DiffContentKeys(tableBase, shaderKey(true, false, tableWord), false, diff), "image table word");
    expectOnly(diff, ContentKeyComponent::Image, "image table word");
    auto tableShape = table;
    tableShape[0].shapes[0] += 1;
    check(ShaderResources::DiffContentKeys(tableBase, shaderKey(true, false, tableShape), false, diff), "image table shape");
    expectOnly(diff, ContentKeyComponent::Image, "image table shape");
    auto samplerWord = table;
    samplerWord[1].words[1] += 1;
    check(ShaderResources::DiffContentKeys(tableBase, shaderKey(true, false, samplerWord), false, diff), "sampler table word");
    expectOnly(diff, ContentKeyComponent::Sampler, "sampler table word");
}

std::vector<std::uint32_t> withDevice(std::vector<std::uint32_t> key, std::uint32_t device) {
    key.push_back(device);
    key.push_back(0u);
    return key;
}

void testDispatchKeyWithDevice() {
    ContentKeyDiff diff;
    const auto base = withDevice(shaderKey(false, false, bindings()), 5);
    check(ShaderResources::DiffContentKeys(base, base, false, diff), "dispatch key with device words was not comparable");
    expectOnly(diff, std::nullopt, "equal dispatch keys with device words reported a difference");
    auto buffer = bindings();
    buffer[0].words[0] += 1;
    check(ShaderResources::DiffContentKeys(base, withDevice(shaderKey(false, false, buffer), 5), false, diff), "dispatch V# word 0 with device words");
    expectOnly(diff, ContentKeyComponent::BufferWord0, "dispatch V# word 0 with device words");
    check(ShaderResources::DiffContentKeys(base, withDevice(shaderKey(false, false, bindings()), 6), false, diff), "dispatch device change");
    expectOnly(diff, ContentKeyComponent::Other, "dispatch device change");
}

void testDrawKey() {
    ContentKeyDiff diff;
    const auto base = drawKey(5, shaderKey(true, true, bindings()), 0x1000);
    const auto compare = [&](const std::vector<std::uint32_t>& key, std::optional<ContentKeyComponent> expected, const char* message) {
        check(ShaderResources::DiffContentKeys(base, key, true, diff), message);
        expectOnly(diff, expected, message);
    };
    compare(drawKey(5, shaderKey(true, true, bindings()), 0x2000), ContentKeyComponent::DrawTarget, "draw target");
    compare(drawKey(6, shaderKey(true, true, bindings()), 0x1000), ContentKeyComponent::Other, "device");
    auto buffer = bindings();
    buffer[0].words[0] += 1;
    compare(drawKey(5, shaderKey(true, true, buffer), 0x1000), ContentKeyComponent::BufferWord0, "draw V# word 0");
    auto data = bindings();
    data[3].words[0] += 1;
    compare(drawKey(5, shaderKey(true, true, data), 0x1000), std::nullopt, "movable draw keys leave data words out");
}

}

int main() {
    try {
        testDispatchKey();
        testDispatchKeyWithDevice();
        testDrawKey();
        std::puts("Key miss attribution passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
