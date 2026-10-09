#include "ControlFlow/GraphBuilder.hpp"
#include "ControlFlow/RequestSerializer.hpp"
#include "ControlFlow/UserDataCalls.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "CacheKey.hpp"
#include "ShaderDiskCache.hpp"
#include <algorithm>
#include <array>
#include <cstdio>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>

using namespace ShaderRecompiler;

namespace {

void Require(bool value, const char* message) {
    if (!value) throw std::runtime_error(message);
}

template<typename TFunction>
void Reject(TFunction function, const char* message) {
    try {
        function();
    } catch (const std::exception& error) {
        Require(std::string(error.what()).find(message) != std::string::npos, error.what());
        return;
    }
    throw std::runtime_error(std::string("negative control accepted: ") + message);
}

struct Fixture {
    std::vector<std::uint32_t> code{0xbe920304u, 0xbe930305u, 0xbe8e0312u, 0xbe8f0313u, 0xbe8e210eu, 0x80148314u, 0xbf810000u};
    std::array<std::uint32_t, 12> users{};
    std::map<std::uint64_t, std::vector<std::uint32_t>> memory{{0x2000u, {0x80148714u, 0xbe80200eu}}, {0x3000u, {0x80148b14u, 0xbe80200eu}}};

    Fixture() { users[4] = 0x2000u; users[8] = 0x3000u; }

    RecompileRequest Request() const {
        RecompileRequest request{};
        request.shader = {ShaderStage::Compute, 0x1000u, code, 0, {}};
        request.context.userData = users;
        request.context.waveSize = 32;
        return request;
    }

    static bool Read(void* context, std::uint64_t address, std::uint32_t* value) {
        const auto& memory = static_cast<Fixture*>(context)->memory;
        for (const auto& [base, words] : memory) {
            if (address >= base && address - base < words.size() * 4u) {
                *value = words[(address - base) / 4u];
                return true;
            }
        }
        return false;
    }

    CapturedCallProgram Resolve() { return ResolveUserDataCalls(Request(), &Read, this); }
};

std::vector<std::uint64_t> Key(const Fixture& fixture, const CapturedCallProgram& captured) {
    auto request = fixture.Request();
    request.shader.code = captured.code;
    request.shader.capturedCalls = captured.calls;
    std::vector<std::uint64_t> key;
    RecompileCacheKey::Build(request, key);
    return key;
}

std::vector<std::byte> DiskKey(const Fixture& fixture, const CapturedCallProgram& captured) {
    auto request = fixture.Request();
    request.shader.code = captured.code;
    request.shader.capturedCalls = captured.calls;
    std::vector<std::byte> key;
    ShaderDiskCache::BuildKey(request, 32, key);
    return key;
}

void CallsAndContinuations() {
    Fixture fixture;
    const auto original = RdnaInstructionDecoder{}.Decode(fixture.code);
    const SwappcInfo info{false, 0, 12};
    Reject([&] { static_cast<void>(GraphBuilder{}.Build(original, &info)); }, "not statically resolvable");
    fixture.code = {0xbe8e0404u, 0xbe8e210eu, 0x80148314u, 0xbe8e0408u, 0xbe8e210eu, 0x80148314u,
        0xbe8e0408u, 0xbe8e210eu, 0x80148314u, 0xbe8e0408u, 0xbe8e210eu, 0x80148314u, 0xbf810000u};
    const auto captured = fixture.Resolve();
    Require(captured.calls.size() == 4u, "call sites were lost");
    Require(captured.calls[1].targetAddress == captured.calls[2].targetAddress, "same callee was not reused");
    Require(captured.calls[1].targetProgramCounter != captured.calls[2].targetProgramCounter, "same callee was not cloned per call site");
    auto request = fixture.Request();
    request.shader.code = captured.code;
    request.shader.capturedCalls = captured.calls;
    auto boundInfo = info;
    boundInfo.capturedCalls = captured.calls;
    const auto graph = GraphBuilder{}.Build(DecodeShaderProgram(request.shader), &boundInfo);
    for (const auto& call : captured.calls) {
        const auto returns = std::ranges::count_if(graph.blocks, [&](const auto& block) {
            return block.startProgramCounter <= call.returnProgramCounter && block.endProgramCounter > call.returnProgramCounter &&
                block.successors.size() == 1 && graph.blocks[block.successors.front()].startProgramCounter == call.callProgramCounter + 4u;
        });
        Require(returns == 1, "callee returns to the wrong continuation");
    }
    const auto serialized = RequestSerializer{}.Serialize(request);
    const auto restored = RequestSerializer{}.Deserialize(serialized);
    Require(std::ranges::equal(restored.request.shader.capturedCalls, captured.calls), "capture serialization lost call bindings");
    Require(std::ranges::equal(restored.request.shader.code, captured.code), "capture serialization lost callee bytes");
    auto broken = captured;
    broken.calls[2].returnProgramCounter = broken.calls[1].returnProgramCounter;
    request.shader.capturedCalls = broken.calls;
    Reject([&] { static_cast<void>(DecodeShaderProgram(request.shader)); }, "boundaries");
}

void CacheIdentity() {
    Fixture fixture;
    const auto first = fixture.Resolve();
    const auto firstKey = Key(fixture, first);
    const auto firstDiskKey = DiskKey(fixture, first);
    fixture.users[4] = 0x3000u;
    const auto second = fixture.Resolve();
    Require(firstKey != Key(fixture, second), "different targets reuse the same source key");
    Require(firstDiskKey != DiskKey(fixture, second), "different targets reuse the same disk key");
    fixture.users[4] = 0x2000u;
    fixture.memory[0x2000u][0] = 0x80148b14u;
    Require(firstKey != Key(fixture, fixture.Resolve()), "changed callee bytes reuse the same source key");
    Require(firstDiskKey != DiskKey(fixture, fixture.Resolve()), "changed callee bytes reuse the same disk key");
    fixture.memory[0x2000u] = fixture.memory[0x3000u];
    const auto sameBytes = Key(fixture, fixture.Resolve());
    fixture.users[4] = 0x3000u;
    Require(sameBytes != Key(fixture, fixture.Resolve()), "target address is absent from the source key");
    auto changed = first;
    changed.calls[0].targetProgramCounter += 4;
    Require(firstKey != Key(fixture, changed), "call target mapping is absent from the key");
    changed = first;
    changed.calls[0].returnProgramCounter += 4;
    Require(firstKey != Key(fixture, changed), "call return mapping is absent from the key");
    changed = first;
    changed.calls[0].userDataIndex += 2;
    Require(firstKey != Key(fixture, changed), "call provenance is absent from the key");
}

void Failures() {
    Fixture fixture;
    fixture.users[4] = 0;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "null or misaligned");
    fixture.users[4] = 0x2001u;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "null or misaligned");
    fixture.users[4] = 0x4000u;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "unmapped");
    fixture.users[4] = 0x1000u;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "recursive");
    fixture.users[4] = 0xfffffffcu;
    fixture.users[5] = 0xffffffffu;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "overflow");
    fixture.users[4] = 0x2000u;
    fixture.users[5] = 0;
    fixture.memory[0x2000u] = {0xffffffffu};
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "unknown RDNA");
    fixture.memory[0x2000u] = {0xbe9403ffu};
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "unmapped");
    fixture.memory[0x2000u] = {0xbe80200cu};
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "unsupported return");
    fixture.memory[0x2000u] = {0xbf810000u};
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "without returning");
    fixture.memory[0x2000u] = {0xbe8e2104u};
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "nested scalar calls");
    fixture.memory[0x2000u] = {0xbe941f00u};
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "PC-relative");
    fixture.memory[0x2000u] = {0xbe840380u, 0xbe80200eu};
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "clobbers a user-data call target");
    fixture.memory[0x2000u] = std::vector<std::uint32_t>(16384, 0xbf800000u);
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "size limit");
    fixture.memory[0x2000u] = {0x80148714u, 0xbe80200eu};
    fixture.code[2] = 0x800e0112u;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "not statically resolvable");
    fixture = Fixture{};
    fixture.code[2] = 0x7e1c0500u;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "not statically resolvable");
    fixture = Fixture{};
    fixture.code.insert(fixture.code.begin(), 0xbf840002u);
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "not statically resolvable");
    fixture = Fixture{};
    fixture.code.insert(fixture.code.end() - 1, 0x800e810eu);
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "link escapes");
    fixture = Fixture{};
    fixture.code.resize(262144);
    fixture.memory[0x200000u] = fixture.memory[0x2000u];
    fixture.users[4] = 0x200000u;
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "combined program size limit");
    fixture = Fixture{};
    fixture.code.clear();
    for (unsigned index = 0; index <= MaxCapturedShaderCalls; ++index) fixture.code.insert(fixture.code.end(), {0xbe8e0304u, 0xbe8f0305u, 0xbe8e210eu});
    fixture.code.push_back(0xbf810000u);
    Reject([&] { static_cast<void>(fixture.Resolve()); }, "too many captured scalar calls");
}

}

int main() {
    try {
        Fixture fixture;
        Require(fixture.Resolve().calls.size() == 1, "SGPR copy chain was not resolved");
        fixture.code = {0xbe842104u, 0xbf810000u};
        fixture.memory[0x2000u] = {0x80148714u, 0xbe802004u};
        Require(fixture.Resolve().calls.size() == 1, "direct user-data target sharing its link pair was not resolved");
        fixture.code = {0xbe842104u, 0xbf82fffeu, 0xbf810000u};
        Reject([&] { static_cast<void>(fixture.Resolve()); }, "not statically resolvable");
        CallsAndContinuations();
        CacheIdentity();
        Failures();
        std::puts("user-data swappc analysis tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
