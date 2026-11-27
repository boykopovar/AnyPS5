#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawPipeline.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Draw/DrawCache.hpp"
#include "prx/libSceAgcDriver/Graphics/include/ShaderInputState.hpp"
#include "prx/libSceAgc/Shader/include/ShaderConstants.hpp"
#include "SceShaders.hpp"
#include "Recompiler.hpp"
#include <array>
#include <atomic>
#include <cstring>
#include <vector>
#include <chrono>
#include <cstring>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <semaphore>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

namespace {

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

class StderrCapture {
public:
    StderrCapture() : path(std::filesystem::temp_directory_path() / ("aps5_pipeline_profile_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".txt")) {
        std::fflush(stderr);
#ifdef _WIN32
        saved = _dup(_fileno(stderr));
        Check(saved >= 0, "cannot save stderr");
        Check(_wfreopen(path.wstring().c_str(), L"wb", stderr) != nullptr, "cannot redirect stderr");
#else
        saved = dup(fileno(stderr));
        Check(saved >= 0, "cannot save stderr");
        Check(std::freopen(path.c_str(), "wb", stderr) != nullptr, "cannot redirect stderr");
#endif
    }

    ~StderrCapture() {
        std::fflush(stderr);
#ifdef _WIN32
        _dup2(saved, _fileno(stderr));
        _close(saved);
#else
        dup2(saved, fileno(stderr));
        close(saved);
#endif
        std::error_code error;
        std::filesystem::remove(path, error);
    }

    StderrCapture(const StderrCapture&) = delete;
    StderrCapture& operator=(const StderrCapture&) = delete;

    std::string ReadReport(std::size_t occurrence = 0) const {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        do {
            std::ifstream file(path, std::ios::binary);
            const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
            auto begin = text.find("[draw] pipelined ");
            for (std::size_t skipped = 0; skipped < occurrence && begin != std::string::npos; ++skipped) begin = text.find("[draw] pipelined ", begin + 1);
            if (begin != std::string::npos) {
                const auto end = text.find('\n', begin);
                if (end != std::string::npos) return text.substr(begin, end - begin);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        } while (std::chrono::steady_clock::now() < deadline);
        throw std::runtime_error("pipeline profile report was not written");
    }

private:
    std::filesystem::path path;
    int saved = -1;
};

std::size_t Count(std::string_view text, std::string_view value) {
    std::size_t count = 0;
    for (std::size_t offset = 0; (offset = text.find(value, offset)) != std::string_view::npos; offset += value.size()) ++count;
    return count;
}

void TestReport() {
    using AgcDriver::DriverDetail::DrawPipeline;
    auto& pipeline = DrawPipeline::Queue0();
    Check(DrawPipeline::Depth() >= 2, "test requires at least two pipeline slots");
    const auto reportAfter = std::chrono::steady_clock::now() + std::chrono::milliseconds(10100);
    const auto clockReads = DrawPipeline::CommitClockReads();
    StderrCapture capture;
    std::binary_semaphore release{0};
    bool released = false;
    std::exception_ptr failure;
    try {
        pipeline.Enqueue([&] { release.acquire(); }, {}, 0, {}, {{0x1000, 0x1010}, {0x2000, 0x2010}}, true);
        Check(DrawPipeline::HeldReads(0x1000, 4), "queued read range was not held");
        for (unsigned i = 0; i < 3; ++i) pipeline.NoteDecodeDrain();
        for (unsigned i = 0; i < 2; ++i) pipeline.NoteDecodeDrainSkip();
        std::this_thread::sleep_until(reportAfter);
        pipeline.Enqueue([] {}, {}, 0, {}, {}, true);
        release.release();
        released = true;
        std::this_thread::sleep_until(reportAfter + std::chrono::milliseconds(10100));
        pipeline.Enqueue([] {}, {}, 0, {}, {}, true);
    } catch (...) {
        failure = std::current_exception();
    }
    if (!released) release.release();
    pipeline.Stop();
    if (failure) std::rethrow_exception(failure);
    const auto report = capture.ReadReport();
    std::cout << report << '\n';
    Check(DrawPipeline::CommitClockReads() - clockReads == 6, "the three timed commits did not read the clock exactly twice each");
    Check(report.find(" 2 read ranges, 1 stores held, 3 decode drains") != std::string::npos, "pipeline read counters changed");
    Check(report.find(" 3 decode drains (2 skipped)") != std::string::npos, "pipeline decode-drain skip counter changed");
    Check(Count(report, " read ranges") == 1, "pipeline read-range counters were printed more than once");
    Check(Count(report, " stores held") == 1, "pipeline held-store counters were printed more than once");
    Check(Count(report, " decode drains") == 1, "pipeline decode-drain counters were printed more than once");
    Check(report.find("; drains:") != std::string::npos, "pipeline drain counters were lost");
    Check(report.find("packet drains by opcode:") != std::string::npos, "pipeline opcode counters were lost");
    const auto second = capture.ReadReport(1);
    std::cout << second << '\n';
    Check(second.find(" 0 read ranges, 0 stores held, 0 decode drains") != std::string::npos, "a later report repeated the read ranges of an earlier one");
}

void TestUntimedCommits() {
    using AgcDriver::DriverDetail::DrawPipeline;
    auto& pipeline = DrawPipeline::Queue0();
    Check(DrawPipeline::Depth() >= 2, "test requires at least two pipeline slots");
    unsigned ran = 0;
    for (unsigned i = 0; i < 3; ++i) pipeline.Enqueue([&] { ++ran; }, {}, 0, {}, {}, true);
    pipeline.Stop();
    Check(ran == 3, "the queued commits did not run");
    Check(DrawPipeline::CommitClockReads() == 0, "the committer read the clock with draw profiling and pipeline statistics off");
}

void TestDecodeDrainSkip() {
    using namespace AgcDriver::DriverDetail;
    using namespace std::chrono_literals;
    auto& finisher = DrawFinisher::Get();
    auto& queue = DrawPipeline::Queue0();
    finisher.Drain();
    queue.Drain(DrawPipeline::DrainReason::Submission);
    struct Restore {
        bool active;
        ~Restore() { AgcDriver::DriverDetail::DrawPipeline::Active() = active; }
    } restore{DrawPipeline::Active()};
    DrawPipeline::Active() = true;
    constexpr std::size_t userDataOffset = 288;
    constexpr std::size_t directOffset = 352;
    constexpr std::size_t semanticOffset = 384;
    static std::vector<std::byte> header(semanticOffset + sizeof(ShaderSemantic));
    static std::array<std::uint32_t, 4> bufferTable{0x1000u, 0x20000u, 64u, 0x3000u};
    static std::array<std::uint32_t, 1> attribTable{(0x2cu << 5u)};
    std::array<std::uint16_t, ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT> direct{};
    direct.fill(ShaderRegs::AGC_ILLEGAL_DIRECT_OFFSET);
    direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexBufferTable)] = 4;
    direct[static_cast<std::size_t>(ShaderRegs::AgcDirectResourceType::PtrVertexAttribDescTable)] = 6;
    std::memcpy(header.data() + directOffset, direct.data(), sizeof(direct));
    ShaderSemantic semantic{};
    semantic.size_in_elements = 1;
    std::memcpy(header.data() + semanticOffset, &semantic, sizeof(semantic));
    ShaderUserData userDataHeader{};
    userDataHeader.direct_resource_offset = reinterpret_cast<std::uint16_t*>(header.data() + directOffset);
    userDataHeader.direct_resource_count = ShaderRegs::AGC_DIRECT_RESOURCE_TYPE_COUNT;
    std::memcpy(header.data() + userDataOffset, &userDataHeader, sizeof(userDataHeader));
    Shader shader{};
    shader.user_data = reinterpret_cast<ShaderUserData*>(header.data() + userDataOffset);
    shader.input_semantics = reinterpret_cast<ShaderSemantic*>(header.data() + semanticOffset);
    shader.num_input_semantics = 1;
    std::memcpy(header.data(), &shader, sizeof(shader));
    const auto headerAddress = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(header.data()));
    const auto low = [](const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer)); };
    const auto high = [](const void* pointer) { return static_cast<std::uint32_t>(reinterpret_cast<std::uintptr_t>(pointer) >> 32u); };
    const std::array<std::uint32_t, 8> tableRegisters{0x31016facu, 0x7efff390u, 0x100002u, 0x49u, low(bufferTable.data()), high(bufferTable.data()), low(attribTable.data()), high(attribTable.data())};
    const std::array<std::uint32_t, 4> plainRegisters{1, 2, 3, 4};
    const auto waitFor = [](const std::atomic<bool>& gate) {
        const auto until = std::chrono::steady_clock::now() + 5s;
        while (!gate.load() && std::chrono::steady_clock::now() < until) std::this_thread::sleep_for(1ms);
    };
    struct Outcome {
        bool drained = false;
        ShaderRecompiler::ShaderVertexStageInfo info{};
        std::size_t reads = 0;
    };
    const auto decodeBehindJobs = [&](std::span<const std::byte> bytes, std::uint64_t address, std::span<const std::uint32_t> userData) {
        std::atomic<bool> gate{false};
        std::atomic<bool> jobDone{false};
        DrawFinisher::Pending held;
        held.unknown = true;
        finisher.Enqueue([&] {
            waitFor(gate);
            jobDone = true;
        }, held);
        std::thread releaser([&] {
            std::this_thread::sleep_for(150ms);
            gate = true;
        });
        std::vector<AgcDriver::Graphics::DecodeRead> reads;
        Outcome outcome;
        outcome.info = DecodeOrderedVertexStageInfo(bytes, address, userData, reads, 0);
        outcome.drained = jobDone.load();
        outcome.reads = reads.size();
        releaser.join();
        finisher.Drain();
        queue.Drain(DrawPipeline::DrainReason::Submission);
        return outcome;
    };
    const auto skipsBefore = DrawPipeline::Queue0().DecodeDrainSkips();
    const auto kept = decodeBehindJobs({}, 0, plainRegisters);
    Check(DrawPipeline::Queue0().DecodeDrainSkips() == skipsBefore + 1, "decode drain skip: the skipped drain was not counted");
    Check(kept.reads == 0u && !kept.drained, "decode drain skip: a decode that read no guest memory still drained behind a job with unknown writes");
    const auto read = decodeBehindJobs(header, headerAddress, tableRegisters);
    Check(DrawPipeline::Queue0().DecodeDrainSkips() == skipsBefore + 1, "decode drain skip: a decode that read vertex tables was counted as skipped");
    Check(read.reads != 0u && read.drained, "decode drain skip: a decode that read vertex tables did not drain behind a job with unknown writes");
    Check(read.info.fetchEmbedded && read.info.resourcesNum == 1 && read.info.resources[0].fields[0] == 0x1000u, "decode drain skip: the decode of the vertex tables after the drain lost its buffer");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::strcmp(argv[1], "--untimed") == 0) {
            TestUntimedCommits();
            std::cout << "Draw pipeline untimed commits passed\n";
            return 0;
        }
        if (argc == 2 && std::string_view(argv[1]) == "decode-drain-skip") {
            TestDecodeDrainSkip();
            std::cout << "Decode drain skip passed\n";
            return 0;
        }
        TestReport();
        std::cout << "Draw pipeline profile passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
