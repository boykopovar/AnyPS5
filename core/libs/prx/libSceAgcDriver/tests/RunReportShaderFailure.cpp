#include "Recompiler.hpp"
#include "RdnaDecoder/DiagnosticSink.hpp"
#include "RdnaDecoder/RdnaInstruction.hpp"
#include "RdnaDecoder/RdnaInstructionDecoder.hpp"
#include "RdnaDecoder/RdnaProgram.hpp"
#include "prx/libSceAgcDriver/Graphics/include/GuestWorkMark.hpp"
#include <spirv/unified1/spirv.hpp>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <exception>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

using namespace ShaderRecompiler;

std::atomic<int> compiled{0}, failed{0}, reused{0}, unsupported{0};
std::atomic<int> lastEncodingIsSop1{0};

void Shader(int what) {
    (what == 0 ? compiled : what == 1 ? failed : reused).fetch_add(1);
}

void Unsupported(const char* encoding, const char*, long long, const char*, int) {
    unsupported.fetch_add(1);
    if (encoding != nullptr && std::string_view(encoding) == "SOP1") lastEncodingIsSop1 = 1;
}

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::array<std::uint32_t, 4> MovStore{0x7e020300u, 0xe0700000u, 0x80000100u, 0xbf810000u};
constexpr std::array<std::uint32_t, 2> UnsupportedSop1{0xbe807e80u, 0xbf810000u};

RecompileResult recompile(std::span<const std::uint32_t> code, bool useCache) {
    const std::array<std::uint32_t, 2> capabilities{spv::CapabilityShader, spv::CapabilityGroupNonUniform};
    const std::array<std::uint32_t, 4> userData{0x10000000u, 0x00100000u, 0x40u, 0x00027facu};
    const std::array<std::string_view, 1> extensions{"SPV_KHR_storage_buffer_storage_class"};
    RecompileRequest request{};
    request.shader = {ShaderStage::Compute, 0x20000u, code, 0, {}};
    request.context.waveSize = 64;
    request.context.userDataBaseRegister = 0;
    request.context.userData = userData;
    request.context.compute = ShaderComputeStageInfo{{64u, 1u, 1u}, 0u, {false, false, false}, false, 1u};
    request.target.vulkanVersion = 0x00403000u;
    request.target.spirvVersion = 0x00010600u;
    request.target.subgroupSize = 64;
    request.target.bdaAbiVersion = 1;
    request.target.supportedCapabilities = capabilities;
    request.target.supportedExtensions = extensions;
    request.target.maxWorkgroupSize = {1024u, 1024u, 64u};
    request.target.maxWorkgroupInvocations = 1024;
    request.target.maxWorkgroupSharedMemoryBytes = 49152;
    request.layout = {0, 0, 0, 128};
    request.useCache = useCache;
    return Recompile(request);
}

void verifyGuestWorkMarks() {
    namespace Mark = AgcDriver::Graphics::GuestWorkMark;
    int handleA = 0, handleB = 0;
    Mark::Note(&handleA);
    Mark::Note(&handleB);
    require(Mark::Take(&handleB) && !Mark::Take(&handleB), "a mark is taken once");
    Mark::Forget(&handleA);
    require(!Mark::Take(&handleA), "a released batch's mark survived and would mark whoever is handed its handle next");
    for (int index = 0; index < 20; ++index) Mark::Note(&handleA);
    require(Mark::Take(&handleA) && !Mark::Take(&handleA), "a mark repeated is still one mark");
    int many[32] = {};
    for (auto& handle : many) Mark::Note(&handle);
    int taken = 0;
    for (auto& handle : many) taken += Mark::Take(&handle) ? 1 : 0;
    require(taken <= 8, "more marks are kept than the table holds");
}

void verifyConstructingTheErrorReportsNothing() {
    static const DiagnosticSink sink{.unsupportedInstruction = Unsupported};
    SetDiagnosticSink(&sink);
    unsupported = 0;
    static_cast<void>(UnsupportedInstructionError("unsupported SOP1 opcode 5", "SOP1", 5, __FILE__, __LINE__));
    require(unsupported == 0, "building the error already reported an instruction that nothing ended on");
    SetDiagnosticSink(nullptr);
}

std::uint32_t findUndecodableWord() {
    SetDiagnosticSink(nullptr);
    const std::array<std::pair<std::uint32_t, std::uint32_t>, 5> classes{{{0xbe800000u, 8u}, {0x80000000u, 23u}, {0xb0000000u, 23u}, {0xbf800000u, 16u}, {0xbf000000u, 16u}}};
    for (const auto& [base, shift] : classes) {
        for (std::uint32_t opcode = 0; opcode < 128u; ++opcode) {
            const std::uint32_t word = base | (opcode << shift);
            const std::array<std::uint32_t, 2> code{word, 0xbf810000u};
            try {
                static_cast<void>(DecodeRdnaInstruction(0, code, 0));
            } catch (const UnsupportedInstructionError&) {
                return word;
            } catch (const std::exception&) {
            }
        }
    }
    return 0;
}

void verifyUndecodableTrailingWordIsNotReported() {
    const auto word = findUndecodableWord();
    require(word != 0, "no word that the decoder rejects was found to place after a program");
    const std::array<std::uint32_t, 3> code{0xbf820001u, word, 0xbf810000u};
    static const DiagnosticSink sink{.shader = Shader, .unsupportedInstruction = Unsupported};
    SetDiagnosticSink(&sink);
    unsupported = 0;
    RdnaProgram program;
    DecodeRdnaProgram(code, program);
    require(program.instructions.size() == 2, "the branch over the undecodable word and the program end were not both decoded");
    require(unsupported == 0, "the decoder reported a word after the end of the program that it skipped");
    SetDiagnosticSink(nullptr);
}

void verify() {
    verifyGuestWorkMarks();
    verifyUndecodableTrailingWordIsNotReported();
    verifyConstructingTheErrorReportsNothing();
#ifdef _WIN32
    _putenv_s("ANYPS5_NO_SHADER_CACHE", "1");
#else
    setenv("ANYPS5_NO_SHADER_CACHE", "1", 1);
#endif
    static const DiagnosticSink sink{.shader = Shader, .unsupportedInstruction = Unsupported};
    SetDiagnosticSink(&sink);
    for (const bool cache : {false, true}) {
        compiled = failed = reused = unsupported = 0;
        lastEncodingIsSop1 = 0;
        const auto before = std::string(cache ? "cached" : "uncached");
        recompile(MovStore, cache);
        require(compiled == 1 && failed == 0, before + ": a good compile was not counted as one compile (compiled " + std::to_string(compiled) + ", failed " + std::to_string(failed) + ")");
        bool threw = false;
        try {
            recompile(UnsupportedSop1, cache);
        } catch (const std::exception&) {
            threw = true;
        }
        require(compiled + failed == 2, before + ": each attempt is counted once, as a compile or as a failure (compiled " + std::to_string(compiled) + ", failed " + std::to_string(failed) + ")");
        require(threw ? failed == 1 && compiled == 1 : failed == 0 && compiled == 2, before + ": the outcome and the counts disagree");
        require(threw ? unsupported >= 1 && lastEncodingIsSop1 == 1 : unsupported == 0, before + ": the instruction that ended a shader was not reported with its encoding, or one was reported for a shader that compiled");
    }
}

}

int main() {
    try {
        verify();
        std::puts("run report shader failure tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        return 1;
    }
}
