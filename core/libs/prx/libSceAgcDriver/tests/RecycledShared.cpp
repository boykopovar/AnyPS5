#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/HeapStages.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/RecycledShared.hpp"
#include <cstddef>
#include <cstdint>
#include <exception>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::DriverDetail::RecycledShared;

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

void heldObjectsNotReusedTests() {
    RecycledShared<std::vector<std::uint32_t>> pool(4, 4);
    std::vector<std::shared_ptr<std::vector<std::uint32_t>>> held;
    for (std::uint32_t index = 0; index < 4; ++index) {
        held.push_back(pool.Take([](std::vector<std::uint32_t>& values) { values.clear(); }));
        held.back()->assign(16, index);
    }
    auto next = pool.Take([](std::vector<std::uint32_t>& values) { values.clear(); });
    for (std::size_t index = 0; index < held.size(); ++index) {
        require(next.get() != held[index].get(), "the recycled pool handed out object " + std::to_string(index) + " while a draw still held it");
        require(*held[index] == std::vector<std::uint32_t>(16, static_cast<std::uint32_t>(index)), "the recycled pool changed object " + std::to_string(index) + " while a draw still held it");
    }
    require(pool.Reused() == 0, "the recycled pool reused an object although every pooled object was held");
    std::cout << "the recycled pool never reuses an object another owner holds\n";
}

void releasedObjectsReusedTests() {
    RecycledShared<std::vector<std::uint32_t>> pool(8, 8);
    std::size_t resets = 0;
    const void* first = nullptr;
    for (std::uint32_t round = 0; round < 64; ++round) {
        auto values = pool.Take([&](std::vector<std::uint32_t>& reused) {
            ++resets;
            reused.clear();
        });
        require(values->empty(), "a recycled object kept the contents of its previous use in round " + std::to_string(round));
        values->assign(256, round);
        if (first == nullptr) first = values.get();
    }
    require(pool.Fresh() == 1 && pool.Reused() == 63 && resets == 63, "a pool whose objects were released after each use made " + std::to_string(pool.Fresh()) + " objects and reused " + std::to_string(pool.Reused()) + ", not 1 and 63");
    std::cout << "the recycled pool reuses released objects and resets them\n";
}

void fullPoolCyclesTests() {
    RecycledShared<std::vector<std::uint32_t>> pool(2, 2);
    std::vector<std::shared_ptr<std::vector<std::uint32_t>>> heldLong;
    for (int index = 0; index < 2; ++index) heldLong.push_back(pool.Take([](std::vector<std::uint32_t>&) {}));
    for (int index = 0; index < 6; ++index) static_cast<void>(pool.Take([](std::vector<std::uint32_t>&) {}));
    require(pool.Reused() == 5, "a pool full of objects held long ago reused " + std::to_string(pool.Reused()) + " of the 6 short-lived takes after it filled, not 5");
    std::cout << "a full recycled pool replaces long-held objects with newer ones\n";
}

void stageWordsResetTests() {
    ShaderRecompiler::HeapStageWords words;
    words.userData = {1, 2, 3};
    words.flattenedSrt = {4, 5};
    words.vertexAttributes.resize(2);
    words.bindings = {{6}, {7, 8}};
    words.base = std::make_shared<const ShaderRecompiler::HeapStageWords>();
    words.dataOrigin = 9;
    words.drawKeyMemo.words.store(std::make_shared<const std::vector<std::uint32_t>>(1, 10u), std::memory_order_relaxed);
    words.memoizeKey = false;
    AgcDriver::DriverDetail::ResetHeapStageWords(words);
    const ShaderRecompiler::HeapStageWords fresh;
    require(words.userData.empty() && words.flattenedSrt.empty() && words.vertexAttributes.empty() && words.bindings.empty(), "a recycled heap stage record kept its previous draw's words");
    require(words.base == nullptr, "a recycled heap stage record kept its previous draw's ring template");
    require(words.dataOrigin == fresh.dataOrigin, "a recycled heap stage record kept its previous data origin");
    require(words.drawKeyMemo.words.load(std::memory_order_relaxed) == nullptr, "a recycled heap stage record kept its previous draw key");
    require(words.memoizeKey == fresh.memoizeKey, "a recycled heap stage record kept its previous key memo switch");
    words.userData.assign(64u << 10u, 1u);
    words.flattenedSrt.assign(16, 2u);
    words.vertexAttributes.resize(64u << 10u);
    words.bindings.resize(4096);
    const auto smallCapacity = words.flattenedSrt.capacity();
    AgcDriver::DriverDetail::ResetHeapStageWords(words);
    require(words.userData.capacity() == 0u, "a recycled heap stage record kept a 256 KiB buffer in the pool");
    require(words.vertexAttributes.capacity() == 0u, "a recycled heap stage record kept a large vertex attribute buffer in the pool");
    require(words.bindings.capacity() == 0u, "a recycled heap stage record kept a large binding list in the pool");
    require(words.flattenedSrt.capacity() == smallCapacity, "a recycled heap stage record dropped a small buffer it should reuse");
    std::cout << "a recycled heap stage record is reset to a new record's state\n";
}

}

int main() {
    try {
        heldObjectsNotReusedTests();
        releasedObjectsReusedTests();
        fullPoolCyclesTests();
        stageWordsResetTests();
        std::cout << "recycled stage record tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "recycled stage record tests failed: " << error.what() << '\n';
        return 1;
    }
}
