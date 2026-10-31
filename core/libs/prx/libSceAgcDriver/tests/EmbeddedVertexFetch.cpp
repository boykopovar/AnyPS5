#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "Translation/EmbeddedVertexFetch.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <initializer_list>
#include <span>
#include <vector>

using namespace ShaderRecompiler;

namespace {

constexpr std::uint32_t AttributeTable = 0;
constexpr std::uint32_t BufferTable = 2;

std::int32_t FetchedAttribute(std::uint32_t source, std::uint32_t field) {
    const std::array<std::uint32_t, 11> code{
        0xbe9403ffu, source,
        0xbe9503ffu, field,
        0x93961514u,
        0xf4080405u, 0x2c000000u,
        0x02140b08u,
        0xe00c2000u, 0x80040b0au,
        0xbf810000u,
    };
    const auto program = RdnaInstructionDecoder{}.Decode(code);
    const auto plan = EmbeddedVertexFetchAnalyzer{}.Analyze(program, AttributeTable, BufferTable, 0, 16, 64);
    return plan.loads.size() == 1 ? plan.loads.front().attributeId : -1;
}

int Expect(const char* name, std::uint32_t source, std::uint32_t field, std::uint32_t offset) {
    const auto expected = static_cast<std::int32_t>(offset / 16u);
    const auto actual = FetchedAttribute(source, field);
    if (actual == expected) return 0;
    std::fprintf(stderr, "%s: s_bfe_u32 0x%08x, 0x%08x gives buffer slot %d, expected %d\n", name, source, field, actual, expected);
    return 1;
}

int NoCarryOffsets() {
    const std::array<std::uint32_t, 10> code{
        0x4a0a0a10u, 0x4a101011u,
        0xf4080506u, 0xfa000000u,
        0x02060b08u,
        0xbf8cc07fu,
        0xe0002000u, 0x80050a03u,
        0xbf8c3f70u, 0xbf810000u,
    };
    const auto decoded = RdnaInstructionDecoder{}.Decode(code);
    int failures = 0;
    for (const auto wave : {32u, 64u}) {
        const auto analyze = [&](const RdnaProgram& program) { return EmbeddedVertexFetchAnalyzer{}.Analyze(program, 6, 4, 8, 16, wave); };
        const auto plan = analyze(decoded);
        if (plan.loads.size() != 1 || plan.vertexOffsetSgpr != 16 || plan.instanceOffsetSgpr != 17 || plan.vertexOffsetShared || plan.instanceOffsetShared || plan.vertexIndexObserved || plan.instanceIndexObserved) {
            std::fprintf(stderr, "wave%u: no-carry offsets: loads=%zu vertex=%d instance=%d shared=%d/%d observed=%d/%d\n", wave, plan.loads.size(), plan.vertexOffsetSgpr, plan.instanceOffsetSgpr, plan.vertexOffsetShared, plan.instanceOffsetShared, plan.vertexIndexObserved, plan.instanceIndexObserved);
            ++failures;
        }
        const auto rejected = [&](const char* label, std::initializer_list<std::uint32_t> instruction) {
            std::vector<std::uint32_t> words(instruction);
            words.insert(words.end(), code.begin() + 1, code.end());
            if (analyze(RdnaInstructionDecoder{}.Decode(words)).vertexOffsetSgpr == -1) return;
            std::fprintf(stderr, "wave%u: %s was admitted as a no-carry offset\n", wave, label);
            ++failures;
        };
        rejected("VOP3 saturation", {0xd5258005u, 0x00020a10u});
        rejected("SDWA byte source", {0x4a0a0af9u, 0x06800610u});
        rejected("vector offset", {0x4a0a0b10u});
        rejected("different index source", {0x4a0a0c10u});
        rejected("non-user scalar", {0x4a0a0a1fu});
        rejected("subtraction", {0x4c0a0a10u});
        auto sharedCode = std::vector<std::uint32_t>(code.begin(), code.end());
        sharedCode.insert(sharedCode.end() - 1, 0x7e080210u);
        const auto shared = analyze(RdnaInstructionDecoder{}.Decode(sharedCode));
        if (shared.vertexOffsetSgpr != 16 || !shared.vertexOffsetShared) {
            std::fprintf(stderr, "wave%u: a second reader of the offset was not marked shared\n", wave);
            ++failures;
        }
        auto observedCode = std::vector<std::uint32_t>(code.begin(), code.end());
        observedCode.insert(observedCode.begin(), 0x7e020305u);
        if (!analyze(RdnaInstructionDecoder{}.Decode(observedCode)).vertexIndexObserved) {
            std::fprintf(stderr, "wave%u: a read before the offset add was not preserved\n", wave);
            ++failures;
        }
    }
    return failures;
}

int IndexProvenanceAdmission() {
    const std::vector<std::uint32_t> baseline{
        0x4a0a0a10u, 0x4a101011u,
        0xf4080506u, 0xfa000000u,
        0x02060b08u, 0xbf8cc07fu,
        0xe0002000u, 0x80050a03u,
        0xbf8c3f70u, 0xbf810000u,
    };
    int failures = 0;
    for (const auto wave : {32u, 64u}) {
        const auto analyze = [&](const std::vector<std::uint32_t>& code) {
            return EmbeddedVertexFetchAnalyzer{}.Analyze(RdnaInstructionDecoder{}.Decode(code), 6, 4, 8, 16, wave);
        };
        const auto rejected = [&](const char* label, const std::vector<std::uint32_t>& code) {
            const auto plan = analyze(code);
            if (plan.loads.empty() && plan.vertexOffsetSgpr == -1 && plan.instanceOffsetSgpr == -1) return;
            std::fprintf(stderr, "wave%u: %s retained an unproven embedded lift or offset: loads=%zu vertex=%d instance=%d\n", wave, label, plan.loads.size(), plan.vertexOffsetSgpr, plan.instanceOffsetSgpr);
            ++failures;
        };
        const auto accepted = [&](const char* label, const std::vector<std::uint32_t>& code, std::uint32_t descriptorPc, std::initializer_list<std::uint32_t> fetchPcs, std::int32_t vertexOffset = 16, std::int32_t instanceOffset = 17) {
            const auto plan = analyze(code);
            bool matches = plan.loads.size() == fetchPcs.size() && plan.vertexOffsetSgpr == vertexOffset && plan.instanceOffsetSgpr == instanceOffset;
            if (matches) {
                auto load = plan.loads.begin();
                for (const auto fetchPc : fetchPcs) {
                    matches = matches && load->attributeId == 0 && load->componentCount == 1 &&
                        load->programCounter == fetchPc && load->prologLoads == std::vector<std::uint32_t>{descriptorPc};
                    ++load;
                }
            }
            if (matches) return;
            std::fprintf(stderr, "wave%u: %s lost proven initial-user-data offsets or fetch metadata\n", wave, label);
            ++failures;
        };
        const auto prefixRejected = [&](const char* label, std::initializer_list<std::uint32_t> instruction) {
            std::vector<std::uint32_t> code(instruction);
            code.insert(code.end(), baseline.begin(), baseline.end());
            rejected(label, code);
        };
        const auto snapshotRejected = [&](const char* label, std::initializer_list<std::uint32_t> instruction) {
            auto code = baseline;
            code.insert(code.begin() + 5, instruction);
            rejected(label, code);
        };
        accepted("initial offsets before snapshot", baseline, 8u, {24u});
        prefixRejected("immediate overwrites entry vertex offset", {0xbe900382u});
        prefixRejected("immediate overwrites entry instance offset", {0xbe910382u});
        prefixRejected("scalar x2 overwrites offsets", {0xf4040400u, 0xfa000000u});
        prefixRejected("buffer scalar x4 overwrites offsets", {0xf4280400u, 0xfa000000u});
        auto overwrittenInstance = baseline;
        overwrittenInstance.insert(overwrittenInstance.begin() + 1, 0xbe910382u);
        rejected("instance offset overwritten between adds", overwrittenInstance);
        auto rawSnapshot = baseline;
        rawSnapshot.erase(rawSnapshot.begin() + 4);
        rawSnapshot.insert(rawSnapshot.begin(), 0x02060b08u);
        rejected("snapshot before offset adds", rawSnapshot);
        auto partialSnapshot = baseline;
        partialSnapshot.erase(partialSnapshot.begin() + 4);
        partialSnapshot.insert(partialSnapshot.begin() + 1, 0x02060b08u);
        rejected("snapshot before instance offset add", partialSnapshot);
        partialSnapshot.front() = 0x4a101011u;
        partialSnapshot[2] = 0x4a0a0a10u;
        rejected("snapshot before vertex offset add", partialSnapshot);
        auto refreshedSnapshot = baseline;
        refreshedSnapshot.insert(refreshedSnapshot.begin(), 0x02060b08u);
        accepted("raw snapshot replaced after both adds", refreshedSnapshot, 12u, {28u});
        auto consumedOffset = baseline;
        consumedOffset.insert(consumedOffset.begin() + 2, 0xbe900382u);
        accepted("entry offset overwritten after its add", consumedOffset, 12u, {28u});
        auto lateOffset = baseline;
        lateOffset.insert(lateOffset.end() - 1, 0xbe900382u);
        accepted("entry offset overwritten after last fetch", lateOffset, 8u, {24u});
        prefixRejected("F64 conversion overwrites vertex index high word", {0x7e082100u});
        snapshotRejected("F64 conversion overwrites snapshot high word", {0x7e042100u});
        prefixRejected("F64 add overwrites vertex index high word", {0xd5640004u, 0x00020500u});
        snapshotRejected("F64 add overwrites snapshot high word", {0xd5640002u, 0x00020500u});
        prefixRejected("B64 shift overwrites vertex index high word", {0xd6ff0004u, 0x00020081u});
        snapshotRejected("B64 shift overwrites snapshot high word", {0xd6ff0002u, 0x00020081u});
        prefixRejected("F64 div-scale overwrites vertex index high word", {0xd56e0004u, 0x04020500u});
        snapshotRejected("F64 div-scale overwrites snapshot high word", {0xd56e0002u, 0x04020500u});
        prefixRejected("image status overwrites vertex index", {0xf0011108u, 0x00010400u});
        snapshotRejected("image status overwrites snapshot", {0xf0011108u, 0x00010200u});
        prefixRejected("image LWE status overwrites vertex index", {0xf0021108u, 0x00010400u});
        snapshotRejected("image LWE status overwrites snapshot", {0xf0021108u, 0x00010200u});
        prefixRejected("image sample LWE status overwrites vertex index", {0xf0820108u, 0x00010400u});
        snapshotRejected("image sample LWE status overwrites snapshot", {0xf0820108u, 0x00010200u});
        prefixRejected("image sample TFE and LWE status overwrites vertex index", {0xf0830108u, 0x00010400u});
        snapshotRejected("image sample TFE and LWE status overwrites snapshot", {0xf0830108u, 0x00010200u});
        auto imagePayload = baseline;
        imagePayload.insert(imagePayload.begin(), {0xf0800108u, 0x00010400u});
        accepted("image payload leaves vertex index intact", imagePayload, 16u, {32u});
        auto imageSnapshot = baseline;
        imageSnapshot.insert(imageSnapshot.begin() + 5, {0xf0800108u, 0x00010200u});
        accepted("image payload leaves snapshot intact", imageSnapshot, 8u, {32u});
        auto lateImageStatus = baseline;
        lateImageStatus.insert(lateImageStatus.end() - 1, {0xf0820108u, 0x00010200u});
        accepted("image LWE status after last fetch", lateImageStatus, 8u, {24u});
        auto backedge = baseline;
        backedge.insert(backedge.end() - 1, 0xbf82fff6u);
        rejected("post-fetch branch returns to offset adds", backedge);
        auto secondFetch = baseline;
        secondFetch.insert(secondFetch.end() - 1, {0x7e060280u, 0xe0002000u, 0x80050b03u});
        rejected("second fetch uses an overwritten snapshot", secondFetch);
        prefixRejected("relative scalar destination before offset adds", {0xbea83000u});
        snapshotRejected("relative vector destination before fetch", {0x7e288500u});
        auto repeatedVertex = baseline;
        repeatedVertex.insert(repeatedVertex.begin() + 1, 0x4a0a0a10u);
        rejected("vertex offset added twice before snapshot", repeatedVertex);
        auto repeatedInstance = baseline;
        repeatedInstance.insert(repeatedInstance.begin() + 2, 0x4a101011u);
        rejected("instance offset added twice before snapshot", repeatedInstance);
        auto rawRead = baseline;
        rawRead.insert(rawRead.begin(), 0x7e020305u);
        accepted("raw index read to another register", rawRead, 12u, {28u});
        auto scalarReload = baseline;
        scalarReload.insert(scalarReload.begin() + 5, {0xf4280400u, 0xfa000000u});
        accepted("offsets consumed before scalar reload", scalarReload, 8u, {32u});
        auto unrelatedWide = baseline;
        unrelatedWide.insert(unrelatedWide.begin(), 0x7e282100u);
        accepted("unrelated F64 destination", unrelatedWide, 12u, {28u});
        auto lateWide = baseline;
        lateWide.insert(lateWide.end() - 1, 0x7e082100u);
        accepted("F64 destination after last fetch", lateWide, 8u, {24u});
        auto forwardBranch = baseline;
        forwardBranch.insert(forwardBranch.end() - 1, {0xbf820001u, 0xbf800000u});
        accepted("forward branch after last fetch", forwardBranch, 8u, {24u});
        auto rawIndices = baseline;
        rawIndices.erase(rawIndices.begin(), rawIndices.begin() + 2);
        accepted("raw snapshot without offset adds", rawIndices, 0u, {16u}, -1, -1);
        auto twoFetches = baseline;
        twoFetches.insert(twoFetches.end() - 1, {0xe0002000u, 0x80050b03u});
        accepted("two fetches share a valid snapshot", twoFetches, 8u, {24u, 36u});
        auto execAll = baseline;
        execAll.insert(execAll.begin(), 0xbefe04c1u);
        accepted("all lanes enabled before offset adds", execAll, 12u, {28u});
        auto carryAdds = baseline;
        carryAdds.erase(carryAdds.begin(), carryAdds.begin() + 2);
        carryAdds.insert(carryAdds.begin(), {0xd70f7d05u, 0x00020a10u, 0xd70f7d08u, 0x00021011u});
        accepted("carry-form vertex and instance adds", carryAdds, 16u, {32u});
        auto sadAdd = baseline;
        sadAdd.erase(sadAdd.begin());
        sadAdd.insert(sadAdd.begin(), {0xd55d0005u, 0x04150010u});
        accepted("unclamped SAD-form vertex add", sadAdd, 12u, {28u});
    }
    return failures;
}

int BufferStatusIndexProvenance() {
    const std::vector<std::uint32_t> baseline{
        0x4a0a0a10u, 0x4a101011u,
        0xf4080506u, 0xfa000000u,
        0x02060b08u, 0xbf8cc07fu,
        0xe0002000u, 0x80050a03u,
        0xbf8c3f70u, 0xbf810000u,
    };
    int failures = 0;
    for (const auto wave : {32u, 64u}) {
        for (const auto firstWord : {0xe0300000u, 0xe8b00000u}) {
            const char* family = firstWord == 0xe0300000u ? "MUBUF" : "MTBUF";
            const auto check = [&](const char* label, const std::vector<std::uint32_t>& code, bool lift, std::uint32_t descriptorPc = 0u, std::uint32_t fetchPc = 0u) {
                const auto plan = EmbeddedVertexFetchAnalyzer{}.Analyze(RdnaInstructionDecoder{}.Decode(code), 6, 4, 8, 16, wave);
                const bool matches = lift ? plan.loads.size() == 1 && plan.vertexOffsetSgpr == 16 && plan.instanceOffsetSgpr == 17 &&
                    plan.loads.front().attributeId == 0 && plan.loads.front().componentCount == 1 &&
                    plan.loads.front().programCounter == fetchPc && plan.loads.front().prologLoads == std::vector<std::uint32_t>{descriptorPc} :
                    plan.loads.empty() && plan.vertexOffsetSgpr == -1 && plan.instanceOffsetSgpr == -1;
                if (matches) return;
                std::fprintf(stderr, "buffer-status wave%u %s: %s: loads=%zu vertex=%d instance=%d; expected lift=%d\n", wave, family, label, plan.loads.size(), plan.vertexOffsetSgpr, plan.instanceOffsetSgpr, lift ? 1 : 0);
                ++failures;
            };
            auto prefix = baseline;
            prefix.insert(prefix.begin(), {firstWord, 0x80800400u});
            check("status overwrites raw vertex index", prefix, false);
            prefix[1] = 0x80000400u;
            check("payload-only destination leaves raw index intact", prefix, true, 16u, 32u);
            auto snapshot = baseline;
            snapshot.insert(snapshot.begin() + 5, {firstWord, 0x80800200u});
            check("status overwrites saved fetch index", snapshot, false);
            snapshot[6] = 0x80000200u;
            check("payload-only destination leaves saved index intact", snapshot, true, 8u, 32u);
            auto lateRaw = baseline;
            lateRaw.insert(lateRaw.end() - 1, {firstWord, 0x80800400u});
            check("raw-index status write after last fetch", lateRaw, true, 8u, 24u);
            auto lateSnapshot = baseline;
            lateSnapshot.insert(lateSnapshot.end() - 1, {firstWord, 0x80800200u});
            check("saved-index status write after last fetch", lateSnapshot, true, 8u, 24u);
        }
    }
    return failures;
}

}

int main() {
    int failures = 0;
    failures += Expect("width 30 at offset 0", 0x9e000000u, 0x001e0000u, 0x1e000000u);
    failures += Expect("width 8 at offset 4", 0xfff00320u, 0x00080004u, 0x32u);
    failures += Expect("width 0", 0x00000320u, 0x00000000u, 0u);
    failures += Expect("width above 32", 0x00000320u, 0x007f0000u, 0x320u);
    failures += NoCarryOffsets();
    failures += IndexProvenanceAdmission();
    failures += BufferStatusIndexProvenance();
    return failures == 0 ? 0 : 1;
}
