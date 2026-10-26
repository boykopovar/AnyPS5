#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawRegisterKey.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgcDriver/Graphics/include/State.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <type_traits>

using namespace AgcDriver;
using namespace AgcDriver::DriverDetail;

namespace {

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::uint64_t baselineRegisterStateKey(const QueueState& queue, bool allUserWords = false) {
    std::uint64_t key = 0xcbf29ce484222325ull;
    const auto mix = [&](std::uint64_t value) {
        key ^= value;
        key *= 0x100000001b3ull;
    };
    const auto userEnd = [&](std::uint32_t base) {
        if (allUserWords) return base + 32u;
        const auto resources = queue.shader.find(base - 1);
        if (resources == queue.shader.end()) return base;
        const auto count = ((resources->second >> 1u) & 0x1fu) | (((resources->second >> 27u) & 1u) << 5u);
        return base + std::min(count, 32u);
    };
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 3> users{{{0x00cu, userEnd(0x00cu)}, {0x08cu, userEnd(0x08cu)}, {0x10cu, userEnd(0x10cu)}}};
    const auto unread = [&](std::uint32_t offset) {
        return std::any_of(users.begin(), users.end(), [&](const auto& user) { return offset >= user.second && offset < user.first + 32u; });
    };
    for (const auto& range : Graphics::DrawKeyRegisters) {
        const auto& bank = range.bank == Graphics::RegisterBank::Context ? queue.context : range.bank == Graphics::RegisterBank::Shader ? queue.shader : queue.userConfig;
        mix((static_cast<std::uint64_t>(range.bank) << 32u) | range.first);
        const auto end = range.first + range.count;
        const bool shader = range.bank == Graphics::RegisterBank::Shader;
        for (auto it = bank.lower_bound(range.first); it != bank.end() && it->first < end; ++it) {
            if (shader && unread(it->first)) continue;
            mix(it->first);
            mix(it->second);
        }
    }
    return key;
}

void testFingerprints() {
    static_assert(!std::is_convertible_v<Registers::Reference, std::uint32_t&>);
    std::mt19937 random(42);
    QueueState queue;
    RegisterStateKeyParts parts;
    const auto initialKey = RegisterStateKey(queue, false, true, nullptr, &parts);
    const auto initialParts = parts;
    queue.context[0x000] = 1;
    check(RegisterStateKey(queue, false, true, nullptr, &parts) != initialKey, "stable register mutation did not change the key");
    check(parts.userDataRegisters == initialParts.userDataRegisters && parts.auxiliaryRegisters == initialParts.auxiliaryRegisters, "stable register change altered a dynamic-register fingerprint");
    check(parts.stableRegisters != initialParts.stableRegisters, "stable register change was not attributed");
    queue = QueueState{};
    queue.shader[0x00b] = 2;
    queue.shader[0x00c] = 1;
    const auto beforeRuntimeKey = RegisterStateKey(queue, false, true, nullptr, &parts);
    const auto beforeRuntimeMutation = parts;
    queue.shader[0x00c] = 2;
    check(RegisterStateKey(queue, false, true, nullptr, &parts) != beforeRuntimeKey, "runtime-register mutation did not change the key");
    check(parts.userDataRegisters != beforeRuntimeMutation.userDataRegisters && parts.auxiliaryRegisters == beforeRuntimeMutation.auxiliaryRegisters && parts.stableRegisters == beforeRuntimeMutation.stableRegisters, "user-data register change was not isolated");
    queue = QueueState{};
    auto reference = baselineRegisterStateKey(queue);
    auto tracked = RegisterStateKey(queue);
    const auto compare = [&] {
        const auto expected = baselineRegisterStateKey(queue);
        const auto actual = RegisterStateKey(queue);
        check(actual == RegisterStateKey(queue, false, false), "incremental fingerprint missed a mutation");
        check(RegisterStateKey(queue, true) == RegisterStateKey(queue, true, false), "all-user fingerprint missed a mutation");
        check((expected == reference) == (actual == tracked), "exact key changed its dependency set");
        reference = expected;
        tracked = actual;
    };
    for (const auto& range : Graphics::DrawKeyRegisters) {
        auto& bank = range.bank == Graphics::RegisterBank::Context ? queue.context : range.bank == Graphics::RegisterBank::Shader ? queue.shader : queue.userConfig;
        for (auto offset = range.first; offset < range.first + range.count; ++offset) {
            bank.erase(offset); compare();
            bank.emplace(offset, 0); compare();
            bank.insert_or_assign(offset, 0); compare();
            bank[offset] = random(); compare();
            bank.at(offset) ^= 1; compare();
        }
    }
    for (std::uint32_t i = 0; i < 30000; ++i) {
        auto& bank = i % 3 == 0 ? queue.context : i % 3 == 1 ? queue.shader : queue.userConfig;
        const auto offset = random() % 0x500;
        switch (i % 11) {
            case 0: bank.erase(offset); break;
            case 1: bank.emplace(offset, random()); break;
            case 2: bank[offset] |= random(); break;
            case 3: bank[offset] &= random(); break;
            case 6: bank[offset] += random(); break;
            case 4: {
                auto retained = bank[offset];
                bank.insert_or_assign(0x1000, random());
                compare();
                retained = random();
                break;
            }
            case 5: {
                auto copy = bank;
                copy.insert_or_assign(offset, random());
                compare();
                bank = copy;
                break;
            }
            default: bank.insert_or_assign(offset, random()); break;
        }
        compare();
    }
    queue.shader[0x8b] = 0; compare();
    queue.shader[0x8c] = random(); compare();
    queue.shader[0x8b] = 2; compare();
    queue.shader[0x8b] = 1u << 27u; compare();
    queue.shader.erase(0x8b); compare();
    queue.savedContext = queue.context;
    queue.ClearContext(); compare();
    queue.context = *queue.savedContext; compare();
    auto moved = std::move(queue.context);
    check(queue.context.empty(), "moved register bank retained its population");
    queue.context.emplace(0x200, 1); compare();
    queue.context = std::move(moved); compare();
    queue.context = queue.context; compare();
    queue.context.clear(); compare();
    queue.context.emplace(0x200, 0); compare();
    queue = QueueState{}; compare();
    Registers registers{{0, 7}, {64, 9}};
    auto low = std::make_shared<const Registers::Mask>(std::vector<std::uint64_t>{1});
    auto high = std::make_shared<const Registers::Mask>(std::vector<std::uint64_t>{0, 1});
    const auto lowHash = registers.Fingerprint(low);
    check(registers.Fingerprint(high) != lowHash && registers.Fingerprint(low) == lowHash, "projection switch reused another mask's hash");
    registers[64] = 8;
    check(registers.Fingerprint(low) == lowHash && registers.Fingerprint(high) == registers.RecomputeFingerprint(*high), "projection switch lost a masked write");
}

void testShaderSetKey() {
    QueueState queue;
    queue.shader[0x008] = 1;
    queue.shader[0x009] = 0;
    ShaderRegistry first;
    ShaderRegistry second;
    first[0x100].push_back(std::make_shared<ShaderSnapshot>());
    second[0x100].push_back(std::make_shared<ShaderSnapshot>());
    check(DrawShaderSetKey(queue, first) != DrawShaderSetKey(queue, second), "different shader snapshots shared a shader-set key");
}


}

int main() {
    try {
        testFingerprints();
        testShaderSetKey();
        std::puts("Register fingerprint dependencies and mutations passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
