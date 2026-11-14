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

#ifndef _WIN32
void testDanglingSymlinkDestination(const std::filesystem::path& root) {
    const auto source = root / "source";
    const auto destination = root / "dangling-output";
    const auto missingTarget = root / "missing-output-target";
    writeCapture(source);
    std::filesystem::create_symlink(missingTarget, destination);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, destination, plan); }, "filtering accepted a dangling output symlink");
    std::error_code error;
    const auto status = std::filesystem::symlink_status(destination, error);
    check(!error && status.type() == std::filesystem::file_type::symlink, "filtering removed a pre-existing dangling output symlink");
    check(!std::filesystem::exists(missingTarget), "filtering created the dangling symlink target");
}
#endif

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
    AgcDriverTools::FrameCaptureFilterPlan plan;
    AgcDriverTools::FilterFrameCapture(source, output, plan);
    const AgcDriver::FrameCaptureReader original(source);
    const AgcDriver::FrameCaptureReader filtered(output);
    check(filtered.Header() == original.Header(), "empty plan changed capture header");
    check(filtered.Calls().size() == original.Calls().size(), "empty plan changed call count");
    check(filtered.Chunks().size() == original.Chunks().size(), "empty plan changed chunk count");
}


void testReferenceChunk(const std::filesystem::path& root) {
    const auto source = root / "reference-source";
    const auto output = root / "reference-output";
    AgcDriver::FrameCaptureWriter writer(source);
    const std::vector<std::byte> reference(32, std::byte{0x63});
    writer.WriteReference(4, 0x20000, reference);
    writer.Finish("{}\n");
    AgcDriverTools::FrameCaptureFilterPlan plan;
    AgcDriverTools::FilterFrameCapture(source, output, plan);
    const AgcDriver::FrameCaptureReader filtered(output);
    check(filtered.Chunks().size() == 1 && filtered.Chunks()[0].kind == AgcDriver::FrameMemoryKind::Reference, "filtering dropped a reference chunk");
    check(filtered.Read(filtered.Chunks()[0]) == reference, "filtering changed reference bytes");
}


void testSharedMemoryGroup(const std::filesystem::path& root) {
    const auto source = root / "shared-source";
    const auto output = root / "shared-output";
    AgcDriver::FrameCaptureWriter writer(source);
    AgcDriver::FrameCall first; first.kind = AgcDriver::FrameCallKind::Memory; first.values = {7, 0}; writer.WriteCall(first);
    AgcDriver::FrameCall second; second.kind = AgcDriver::FrameCallKind::Memory; second.values = {7, 1}; writer.WriteCall(second);
    const std::vector<std::byte> bytes(8, std::byte{0x42}); writer.WriteDelta(7, 1, 0x30000, bytes); writer.Finish("{}\n");
    AgcDriverTools::FrameCaptureFilterPlan plan; plan.calls.insert(0);
    AgcDriverTools::FilterFrameCapture(source, output, plan);
    const AgcDriver::FrameCaptureReader filtered(output);
    check(filtered.Calls().size() == 1 && filtered.Chunks().size() == 1 && filtered.Chunks()[0].kind == AgcDriver::FrameMemoryKind::Delta, "removing one call discarded a shared memory group");
}


void testAllMemoryGroupCallsRemoved(const std::filesystem::path& root) {
    const auto source = root / "all-group-source";
    const auto output = root / "all-group-output";
    AgcDriver::FrameCaptureWriter writer(source);
    for (std::uint64_t value = 0; value < 2; ++value) { AgcDriver::FrameCall call; call.kind = AgcDriver::FrameCallKind::Memory; call.values = {9, value}; writer.WriteCall(call); }
    const std::vector<std::byte> bytes(8, std::byte{0x42}); writer.WriteDelta(9, 1, 0x30000, bytes); writer.Finish("{}\n");
    AgcDriverTools::FrameCaptureFilterPlan plan; plan.calls = {0, 1};
    AgcDriverTools::FilterFrameCapture(source, output, plan);
    const AgcDriver::FrameCaptureReader filtered(output);
    check(filtered.Calls().empty() && filtered.Chunks().empty(), "removing every call in a memory group retained its deltas");
}


void testMalformedPlanHeader(const std::filesystem::path& root) {
    const auto path = root / "bad-header.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 2\n"; }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted an unsupported version");
}


void testUnknownPlanEntry(const std::filesystem::path& root) {
    const auto path = root / "unknown-entry.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 1\nrange 1\n"; }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted an unknown entry kind");
}


void testDuplicateCallPlanIndex(const std::filesystem::path& root) {
    const auto path = root / "duplicate-call.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 1\ncall 2\ncall 2\n"; }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted duplicate call indexes");
}


void testDuplicateChunkPlanIndex(const std::filesystem::path& root) {
    const auto path = root / "duplicate-chunk.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 1\nchunk 2\nchunk 2\n"; }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted duplicate chunk indexes");
}


void testMissingPlanIndex(const std::filesystem::path& root) {
    const auto path = root / "missing-index.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 1\ncall\n"; }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted a missing index");
}


void testNegativePlanIndex(const std::filesystem::path& root) {
    const auto path = root / "negative-index.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 1\ncall -1\n"; }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted a negative index");
}


void testOverflowPlanIndex(const std::filesystem::path& root) {
    const auto path = root / "overflow-index.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 1\nchunk 18446744073709551616\n"; }
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(path); }, "plan reader accepted an overflowing index");
}


void testBlankPlanLines(const std::filesystem::path& root) {
    const auto path = root / "blank-lines.plan";
    { std::ofstream output(path); output << "aps5-frame-filter 1\n\ncall 3\n\n"; }
    const auto plan = AgcDriverTools::ReadFrameCaptureFilterPlan(path);
    check(plan.calls == std::set<std::uint64_t>{3}, "plan reader did not ignore blank lines");
}


void testMissingPlanFile(const std::filesystem::path& root) {
    checkThrows([&] { AgcDriverTools::ReadFrameCaptureFilterPlan(root / "missing.plan"); }, "plan reader accepted a missing file");
}


void testTruncatedCapture(const std::filesystem::path& root) {
    const auto source = root / "truncated";
    writeCapture(source);
    std::filesystem::resize_file(source / "calls.bin", 10);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, root / "truncated-output", plan); }, "filtering accepted a truncated capture");
}


void testNormalizedSourceAlias(const std::filesystem::path& root) {
    const auto source = root / "source";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    checkThrows([&] { AgcDriverTools::FilterFrameCapture(source, root / "other" / ".." / "source", plan); }, "filtering accepted a normalized source path alias");
}


void testRangeMetadata(const std::filesystem::path& root) {
    const auto source = root / "source";
    const auto output = root / "range-copy";
    writeCapture(source);
    AgcDriverTools::FrameCaptureFilterPlan plan;
    AgcDriverTools::FilterFrameCapture(source, output, plan);
    const AgcDriver::FrameCaptureReader original(source); const AgcDriver::FrameCaptureReader filtered(output);
    const auto& before = original.Chunks().front(); const auto& after = filtered.Chunks().front();
    check(after.address == before.address && after.bytes == before.bytes && after.rangeKind == before.rangeKind && after.protection == before.protection && after.access == before.access, "filtering changed range metadata");
}

}

int main() {
    const auto root = std::filesystem::temp_directory_path() / ("aps5-frame-capture-filter-test-" + std::to_string(getpid()));
    testOutOfRangeChunk(root);
    testNestedDestination(root);
    testParentDestination(root);
    testEmptyPlan(root);
    testReferenceChunk(root);
    testSharedMemoryGroup(root);
    testAllMemoryGroupCallsRemoved(root);
    testMalformedPlanHeader(root);
    testUnknownPlanEntry(root);
    testDuplicateCallPlanIndex(root);
    testDuplicateChunkPlanIndex(root);
    testMissingPlanIndex(root);
    testNegativePlanIndex(root);
    testOverflowPlanIndex(root);
    testBlankPlanLines(root);
    testMissingPlanFile(root);
    testTruncatedCapture(root);
    testNormalizedSourceAlias(root);
    testRangeMetadata(root);
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root);
    testCallFilter(root);
    testChunkFilter(root);
    testInvalidPlan(root);
#ifndef _WIN32
    testDanglingSymlinkDestination(root);
#endif
    testPlanReader(root);
    std::filesystem::remove_all(root);
    std::puts("frame capture filter tests passed");
    return 0;
}
