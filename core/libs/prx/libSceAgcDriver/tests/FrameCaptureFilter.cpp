#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include "prx/libSceAgcDriver/tools/FrameCaptureFilter.hpp"
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace {

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message.c_str());
        std::exit(1);
    }
}

template <typename TFunction>
void checkThrows(TFunction&& function, const std::string& message) {
    try {
        function();
    } catch (const std::runtime_error&) {
        return;
    }
    check(false, message);
}

void writeCapture(const std::filesystem::path& path) {
    AgcDriver::FrameCaptureWriter writer(path);
    writer.WriteRange(0x10000, 0x4000, AgcDriver::FrameRangeKind::Watched, 3, AgcDriver::FrameRangeAccess::Readable | AgcDriver::FrameRangeAccess::Writable);
    std::vector<std::byte> snapshot(8192, std::byte{0x31});
    writer.WriteSnapshot(0x10000, snapshot);
    AgcDriver::FrameCall start;
    start.kind = AgcDriver::FrameCallKind::FrameStart;
    start.ordinal = 1;
    writer.WriteCall(start);
    AgcDriver::FrameCall memory;
    memory.kind = AgcDriver::FrameCallKind::Memory;
    memory.ordinal = 2;
    memory.values = {7, 0};
    writer.WriteCall(memory);
    std::vector<std::byte> delta(64, std::byte{0x52});
    writer.WriteDelta(7, 3, 0x11000, delta);
    writer.WriteDelta(7, 3, 0x12000, delta);
    AgcDriver::FrameCall submit;
    submit.kind = AgcDriver::FrameCallKind::Submit;
    submit.ordinal = 3;
    writer.WriteCall(submit);
    AgcDriver::FrameCall end;
    end.kind = AgcDriver::FrameCallKind::End;
    end.ordinal = 4;
    writer.WriteCall(end);
    writer.Finish("{\"magic\":\"aps5-frame-capture 1\"}\n");
}

void testCallFilter(const std::filesystem::path& root) {
    const auto source = root / "source";
    const auto output = root / "call-filtered";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    plan.calls.insert(1);
    AgcDriverTools::FilterFrameCapture(source, output, plan);

    const AgcDriver::FrameCaptureReader original(source);
    const AgcDriver::FrameCaptureReader filtered(output);
    check(filtered.Header() == original.Header(), "filtering changed the capture header");
    check(filtered.Calls().size() == 3, "filtering retained the removed memory call");
    check(filtered.Calls()[0].kind == AgcDriver::FrameCallKind::FrameStart && filtered.Calls()[1].kind == AgcDriver::FrameCallKind::Submit && filtered.Calls()[2].kind == AgcDriver::FrameCallKind::End,
          "filtering changed the order of retained calls");
    check(filtered.Chunks().size() == 2, "filtering retained an excluded snapshot or memory-group delta");
    check(filtered.Chunks()[0].kind == AgcDriver::FrameMemoryKind::Range && filtered.Chunks()[1].kind == AgcDriver::FrameMemoryKind::Snapshot,
          "filtering removed range metadata or retained a removed memory-group delta");
    check(filtered.Read(filtered.Chunks()[1]) == std::vector<std::byte>(8192, std::byte{0x31}), "filtering changed retained snapshot bytes");
    const auto description = AgcDriverTools::DescribeFrameCapture(output);
    check(description.find("\"kind\":\"submit\"") != std::string::npos, "capture inspection omitted retained calls");
    check(description.find("\"memory_group\":null") != std::string::npos, "capture inspection omitted non-memory call group markers");
}

void testChunkFilter(const std::filesystem::path& root) {
    const auto source = root / "source";
    const auto output = root / "chunk-filtered";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    plan.chunks.insert(1);
    AgcDriverTools::FilterFrameCapture(source, output, plan);
    const AgcDriver::FrameCaptureReader filtered(output);
    check(filtered.Chunks().size() == 3, "filtering did not remove exactly one memory chunk");
    check(filtered.Chunks()[0].kind == AgcDriver::FrameMemoryKind::Range && filtered.Chunks()[1].kind == AgcDriver::FrameMemoryKind::Delta && filtered.Chunks()[2].kind == AgcDriver::FrameMemoryKind::Delta,
          "filtering changed range metadata or retained the excluded snapshot");
    check(filtered.Read(filtered.Chunks()[1]) == std::vector<std::byte>(64, std::byte{0x52}), "filtering changed retained delta bytes");
}

void testInvalidPlan(const std::filesystem::path& root) {
    const auto source = root / "source";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan outside;
    outside.calls.insert(99);
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, root / "bad-index", outside); }, "filtering accepted an out-of-range call index");
    AgcDriverTools::FrameCaptureFilterPlan alias;
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, source, alias); }, "filtering accepted the source as its output");
    const auto existing = root / "existing";
    std::filesystem::create_directories(existing);
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, existing, alias); }, "filtering overwrote an existing destination");
}

void testPlanReader(const std::filesystem::path& root) {
    const auto path = root / "capture.plan";
    {
        std::ofstream output(path);
        output << "aps5-frame-filter 1\ncall 3\nchunk 9\n";
    }
    const auto plan = AgcDriverTools::ReadFrameCaptureFilterPlan(path);
    check(plan.calls == std::set<std::uint64_t>{3} && plan.chunks == std::set<std::uint64_t>{9}, "plan reader changed its call or chunk indexes");
    {
        std::ofstream output(path);
        output << "aps5-frame-filter 1\ncall nope\n";
    }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted a non-numeric index");
}


void testOutOfRangeChunk(const std::filesystem::path& root) {
    const auto source = root / "source";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    plan.chunks.insert(99);
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, root / "bad-chunk", plan); }, "filtering accepted an out-of-range chunk index");
}


void testNestedDestination(const std::filesystem::path& root) {
    const auto source = root / "source";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, source / "nested", plan); }, "filtering accepted an output nested under its source");
}


void testParentDestination(const std::filesystem::path& root) {
    const auto source = root / "source";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, root, plan); }, "filtering accepted an output parent of its source");
}


void testEmptyPlan(const std::filesystem::path& root) {
    const auto source = root / "source";
    const auto output = root / "empty-plan";
    writeCapture(source);
    AgcDriverTools::FilterFrameCapturePlan plan;
    AgcDriverTools::FilterFrameCapture(source, output, plan);
    const AgcDriver::FrameCaptureReader original(source);
    const AgcDriver::FrameCaptureReader filtered(output);
    check(filtered.Header() == original.Header(), "empty plan changed capture header");
    check(filtered.Calls().size() == original.Calls().size(), "empty plan changed call count");
    check(filtered.Chunks().size() == original.Chunks().size(), "empty plan changed chunk count");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("aps5-frame-capture-filter-test-" + std::to_string(getpid()));
    testOutOfRangeChunk(root);
    testNestedDestination(root);
    testParentDestination(root);
    testEmptyPlan(root);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    testCallFilter(root);
    testChunkFilter(root);
    testInvalidPlan(root);
    testPlanReader(root);
    std::filesystem::remove_all(root);
    std::puts("frame capture filter tests passed");
    return 0;
}
