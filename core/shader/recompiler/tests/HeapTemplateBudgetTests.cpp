#include "HeapTemplateBudget.hpp"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using ShaderRecompiler::HeapTemplateLru;

void Require(bool condition, const std::string& what) {
    if (!condition) throw std::runtime_error(what);
}

std::string Names(const std::vector<HeapTemplateLru::Evicted>& evicted) {
    std::string names;
    for (const auto& gone : evicted) names += " " + std::to_string(gone.name.owner) + ":" + std::to_string(gone.name.key);
    return names.empty() ? " none" : names;
}

void LeastRecentlyUsedAcrossShaders() {
    HeapTemplateLru order(200);
    const HeapTemplateLru::Name a{1, 7, 1}, b{2, 7, 2}, c{3, 7, 3};
    Require(order.Insert(a, 100).empty() && order.Insert(b, 100).empty(), "two templates that fit the budget evicted one");
    order.Touch(a);
    order.Touch(b);
    const auto evicted = order.Insert(c, 100);
    Require(evicted.size() == 1 && evicted[0].name.owner == a.owner && evicted[0].name.id == a.id, "insert A, insert B, touch A, touch B, insert C evicted" + Names(evicted) + " instead of A, the least recently used");
    Require(order.Entries() == 2 && order.Bytes() == 200, "the budget does not hold B and C after A was evicted");
    order.Touch(b);
    const auto next = order.Insert({4, 7, 4}, 100);
    Require(next.size() == 1 && next[0].name.owner == c.owner, "after touching B, the next insert evicted" + Names(next) + " instead of C");
}

void StaleTouchAndReplacedTemplate() {
    HeapTemplateLru order(200);
    Require(order.Insert({1, 5, 1}, 100).empty() && order.Insert({2, 5, 2}, 100).empty(), "two templates that fit the budget evicted one");
    order.Touch({1, 5, 9});
    const auto evicted = order.Insert({3, 5, 3}, 100);
    Require(evicted.size() == 1 && evicted[0].name.owner == 1, "a touch naming a template id that is not kept moved the kept template, evicted" + Names(evicted));
    Require(order.Insert({2, 5, 4}, 150).size() == 1 && order.Bytes() == 150 && order.Entries() == 1, "a template replacing the one kept under the same shader and key was charged on top of it");
}

void ChargesOwnedArrays() {
    ShaderRecompiler::RecompileResult result;
    const auto empty = ShaderRecompiler::OwnedBytes(result);
    result.imageTableReadRanges.resize(64);
    for (auto& ranges : result.imageTableReadRanges) ranges.resize(1024);
    const auto readRanges = std::uint64_t{64} * 1024u * sizeof(std::pair<std::uint64_t, std::uint64_t>);
    Require(ShaderRecompiler::OwnedBytes(result) >= empty + readRanges, "a template whose image table read ranges hold " + std::to_string(readRanges) + " bytes was charged " + std::to_string(ShaderRecompiler::OwnedBytes(result) - empty));
    const auto withRanges = ShaderRecompiler::OwnedBytes(result);
    result.heapSlotWords.resize(4096);
    result.imageTableRanges.resize(4096);
    result.imageTablePoison.resize(1024);
    const auto arrays = ShaderRecompiler::OwnedBytes(result.heapSlotWords) + ShaderRecompiler::OwnedBytes(result.imageTableRanges) + ShaderRecompiler::OwnedBytes(result.imageTablePoison);
    Require(ShaderRecompiler::OwnedBytes(result) >= withRanges + arrays, "a template's heap slot words, image table ranges and poison were not charged");
    result.bindings.resize(8);
    for (auto& binding : result.bindings) binding.guestDescriptor.resize(4096);
    Require(ShaderRecompiler::OwnedBytes(result) >= withRanges + arrays + 8u * 4096u * sizeof(std::uint32_t), "the guest descriptor words of a template's bindings were not charged");
}

}

int main() {
    try {
        LeastRecentlyUsedAcrossShaders();
        StaleTouchAndReplacedTemplate();
        ChargesOwnedArrays();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "heap template budget tests passed\n";
    return 0;
}
