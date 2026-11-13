#include "prx/libSceAgcDriver/Execution/include/FrameCapture.hpp"
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <span>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif

namespace {

using AgcDriver::FrameCall;
using AgcDriver::FrameCallKind;
using AgcDriver::FrameCaptureReader;
using AgcDriver::FrameCaptureWriter;
using AgcDriver::FrameMemoryChunk;
using AgcDriver::FrameMemoryKind;
using AgcDriver::FrameRangeKind;

void check(bool condition, const std::string& message) {
    if (!condition) {
        std::fprintf(stderr, "FAILED: %s\n", message.c_str());
        std::exit(1);
    }
}

template <typename TFunction>
void checkThrowsText(TFunction&& function, const std::string& text, bool prefix, const std::string& message) {
    try {
        function();
    } catch (const std::runtime_error& error) {
        const std::string_view what = error.what();
        check(prefix ? what.starts_with(text) && what.size() > text.size() : what == text, message + ": it threw '" + std::string(what) + "'");
        return;
    }
    check(false, message);
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

bool sameRegisters(const std::vector<ShaderRegister>& left, const std::vector<ShaderRegister>& right) {
    if (left.size() != right.size()) return false;
    for (std::size_t index = 0; index < left.size(); ++index) {
        if (left[index].offset != right[index].offset || left[index].value != right[index].value) return false;
    }
    return true;
}

bool sameCall(const FrameCall& left, const FrameCall& right) {
    return left.kind == right.kind && left.ordinal == right.ordinal && left.timeNs == right.timeNs && left.thread == right.thread && left.accepted == right.accepted && left.completed == right.completed && left.queue == right.queue &&
           left.flags == right.flags && left.cpuFlip == right.cpuFlip && left.address == right.address && left.size == right.size && left.values == right.values && sameRegisters(left.context, right.context) &&
           sameRegisters(left.primitive, right.primitive);
}

std::vector<FrameCall> sampleCalls() {
    std::vector<FrameCall> calls;
    FrameCall shader;
    shader.kind = FrameCallKind::RegisterShader;
    shader.ordinal = 1;
    shader.address = 0x2000010000;
    calls.push_back(shader);
    FrameCall abi;
    abi.kind = FrameCallKind::ResolveGraphicsStagesAbi;
    abi.ordinal = 2;
    abi.values = {0x2000010000, 0x2000020000, 0};
    abi.context = {{0x1a0, 7}, {0x1a1, 0xffffffffu}};
    abi.primitive = {{0x242, 3}};
    calls.push_back(abi);
    FrameCall submit;
    submit.kind = FrameCallKind::Submit;
    submit.ordinal = 3;
    submit.timeNs = 123456789;
    submit.thread = 4242;
    submit.accepted = 20;
    submit.completed = 17;
    submit.queue = 0x21;
    submit.address = 0x2000030000;
    submit.size = 4096;
    calls.push_back(submit);
    FrameCall flip;
    flip.kind = FrameCallKind::Flip;
    flip.ordinal = 4;
    flip.cpuFlip = true;
    flip.values = {1, 2, 0, static_cast<std::uint64_t>(-5)};
    calls.push_back(flip);
    FrameCall unregister;
    unregister.kind = FrameCallKind::UnregisterVideoOutput;
    unregister.ordinal = 5;
    unregister.values = {1};
    calls.push_back(unregister);
    FrameCall end;
    end.kind = FrameCallKind::End;
    end.ordinal = 6;
    calls.push_back(end);
    return calls;
}

std::vector<std::byte> sampleMemory() {
    constexpr std::size_t Block = FrameCaptureWriter::BlockBytes;
    std::vector<std::byte> memory(Block * 150 + 4096);
    for (std::size_t block = 0; block < 150; ++block) {
        if (block % 3 == 1 || (block >= 10 && block < 90)) {
            for (std::size_t at = 0; at < Block; at += 97) memory[block * Block + at] = static_cast<std::byte>((block * 131 + at) & 0xff);
        }
    }
    memory[memory.size() - 1] = std::byte{0x5a};
    return memory;
}

void truncateFile(const std::filesystem::path& path, std::uintmax_t removed) {
    const auto size = std::filesystem::file_size(path);
    std::filesystem::resize_file(path, size - removed);
}

void testRoundTrip(const std::filesystem::path& directory) {
    constexpr std::uint64_t Base = 0x2000000000;
    const auto calls = sampleCalls();
    const auto memory = sampleMemory();
    std::vector<std::byte> delta(8192);
    for (std::size_t at = 0; at < delta.size(); ++at) delta[at] = static_cast<std::byte>(at * 7);
    std::uint64_t stored = 0;
    {
        FrameCaptureWriter writer(directory);
        writer.WriteRange(Base, memory.size(), FrameRangeKind::Watched, 3, AgcDriver::FrameRangeAccess::Readable | AgcDriver::FrameRangeAccess::Gpu);
        stored = writer.WriteSnapshot(Base, memory);
        for (const auto& call : calls) writer.WriteCall(call);
        writer.WriteDelta(1, 17, Base + 0x3000, delta);
        writer.Finish("{\"magic\":\"aps5-frame-capture 1\"}\n");
        check(writer.RawBytes() == stored + delta.size(), "raw byte count does not add the snapshot and the delta");
    }
    std::size_t nonzero = 0;
    for (std::size_t at = 0; at < memory.size(); at += FrameCaptureWriter::BlockBytes) {
        const auto bytes = std::min(FrameCaptureWriter::BlockBytes, memory.size() - at);
        if (!AgcDriver::AllZero(std::span(memory).subspan(at, bytes))) nonzero += bytes;
    }
    check(stored == nonzero, "the snapshot stored zero blocks or lost data blocks");

    const FrameCaptureReader reader(directory);
    check(!reader.Truncated(), "a finished capture reads as truncated");
    check(reader.Version() == 3, "a capture this build wrote reads as calls format " + std::to_string(reader.Version()) + ", not 3");
    check(reader.Header() == "{\"magic\":\"aps5-frame-capture 1\"}\n", "header does not round-trip");
    check(reader.Calls().size() == calls.size(), "call count does not round-trip");
    for (std::size_t index = 0; index < calls.size(); ++index) check(sameCall(reader.Calls()[index], calls[index]), "call " + std::to_string(index) + " does not round-trip");

    std::vector<std::byte> restored(memory.size());
    std::size_t ranges = 0, snapshots = 0, deltas = 0;
    for (const auto& chunk : reader.Chunks()) {
        if (chunk.kind == FrameMemoryKind::Range) {
            ++ranges;
            check(chunk.address == Base && chunk.bytes == memory.size() && chunk.rangeKind == FrameRangeKind::Watched && chunk.protection == 3 &&
                      chunk.access == (AgcDriver::FrameRangeAccess::Readable | AgcDriver::FrameRangeAccess::Gpu), "range does not round-trip");
            continue;
        }
        if (chunk.kind == FrameMemoryKind::Snapshot) {
            ++snapshots;
            check(chunk.bytes <= FrameCaptureWriter::ChunkBytes, "snapshot chunk is larger than the chunk limit");
            check(chunk.address >= Base && chunk.address - Base + chunk.bytes <= restored.size(), "snapshot chunk outside its range");
            reader.Read(chunk, std::span(restored).subspan(chunk.address - Base, chunk.bytes));
            continue;
        }
        ++deltas;
        check(chunk.group == 1 && chunk.completed == 17 && chunk.address == Base + 0x3000, "delta header does not round-trip");
        check(reader.Read(chunk) == delta, "delta bytes do not round-trip");
    }
    check(ranges == 1 && deltas == 1 && snapshots >= 2, "unexpected chunk counts");
    check(restored == memory, "snapshot does not restore the memory");
    std::vector<std::byte> wrong(1);
    checkThrows([&] { reader.Read(reader.Chunks()[1], wrong); }, "a read into a wrongly sized destination does not throw");
}

void testTruncated(const std::filesystem::path& source, const std::filesystem::path& directory) {
    std::filesystem::remove_all(directory);
    std::filesystem::copy(source, directory, std::filesystem::copy_options::recursive);
    truncateFile(directory / FrameCaptureWriter::CallsName, 3);
    truncateFile(directory / FrameCaptureWriter::MemoryName, 10);
    std::filesystem::remove(directory / FrameCaptureWriter::HeaderName);
    const FrameCaptureReader reader(directory);
    const auto calls = sampleCalls();
    check(reader.Truncated(), "a cut capture does not read as truncated");
    check(reader.Calls().size() == calls.size() - 1, "a cut call record is not dropped");
    check(reader.Chunks().back().kind != FrameMemoryKind::Delta, "a cut memory chunk is not dropped");
}

void testOlderVersion(const std::filesystem::path& source, const std::filesystem::path& directory, std::string_view magic, std::uint32_t expected) {
    std::filesystem::remove_all(directory);
    std::filesystem::copy(source, directory, std::filesystem::copy_options::recursive);
    {
        std::fstream file(directory / FrameCaptureWriter::CallsName, std::ios::binary | std::ios::in | std::ios::out);
        file.write(magic.data(), static_cast<std::streamsize>(magic.size()));
        check(file.good(), "cannot write the calls format " + std::to_string(expected) + " magic");
    }
    std::uint32_t version = 0;
    std::vector<FrameCall> readBack;
    try {
        const FrameCaptureReader reader(directory);
        version = reader.Version();
        readBack = reader.Calls();
    } catch (const std::runtime_error& error) {
        check(false, "a calls format " + std::to_string(expected) + " capture does not read: " + error.what());
    }
    check(version == expected, "a calls format " + std::to_string(expected) + " capture reads as format " + std::to_string(version));
    const auto calls = sampleCalls();
    check(readBack.size() == calls.size(), "a calls format " + std::to_string(expected) + " capture reads " + std::to_string(readBack.size()) + " calls, not " + std::to_string(calls.size()));
    for (std::size_t index = 0; index < calls.size(); ++index) check(sameCall(readBack[index], calls[index]), "call " + std::to_string(index) + " of a calls format " + std::to_string(expected) + " capture does not round-trip");
}

void testCorrupt(const std::filesystem::path& source, const std::filesystem::path& directory) {
    std::filesystem::remove_all(directory);
    std::filesystem::copy(source, directory, std::filesystem::copy_options::recursive);
    {
        std::fstream file(directory / FrameCaptureWriter::CallsName, std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(static_cast<std::streamoff>(FrameCaptureWriter::CallsMagic.size()));
        const std::uint32_t kind = 99;
        file.write(reinterpret_cast<const char*>(&kind), sizeof(kind));
    }
    checkThrows([&] { FrameCaptureReader reader(directory); }, "an unknown call kind does not throw");
    std::ofstream(directory / FrameCaptureWriter::CallsName, std::ios::binary | std::ios::trunc) << "not a capture";
    checkThrows([&] { FrameCaptureReader reader(directory); }, "a file without the magic does not throw");
}

void testWriter(const std::filesystem::path& directory) {
    FrameCaptureWriter writer(directory);
    writer.Finish("{}");
    FrameCall call;
    call.kind = FrameCallKind::Suspend;
    checkThrows([&] { writer.WriteCall(call); }, "a call after Finish does not throw");
    checkThrows([&] { writer.Finish("{}"); }, "a second Finish does not throw");
    FrameCall unknown;
    unknown.kind = static_cast<FrameCallKind>(77);
    FrameCaptureWriter other(directory / "other");
    checkThrows([&] { other.WriteCall(unknown); }, "an unknown call kind is written");
}

std::vector<std::byte> guest(64);
std::vector<std::pair<std::uint64_t, std::uint64_t>> reads;
constexpr std::uint64_t GuestBase = 0x10000;

void readFake(std::uint64_t address, std::span<std::byte> destination) {
    reads.emplace_back(address, destination.size());
    std::memcpy(destination.data(), guest.data() + (address - GuestBase), destination.size());
}

void checkReadAround(std::vector<std::pair<std::uint64_t, std::uint64_t>> skipped, std::uint64_t zeroBegin, std::uint64_t zeroEnd, const std::string& what) {
    reads.clear();
    std::vector<std::byte> bytes(48, std::byte{0xcc});
    const auto zeroed = AgcDriver::ReadAroundRanges(GuestBase + 8, bytes, skipped, readFake);
    check(zeroed == zeroEnd - zeroBegin, what + ": the zeroed byte count is wrong");
    for (std::size_t at = 0; at < bytes.size(); ++at) {
        const auto address = GuestBase + 8 + at;
        const bool zero = address >= GuestBase + zeroBegin && address < GuestBase + zeroEnd;
        check(bytes[at] == (zero ? std::byte{0} : guest[address - GuestBase]), what + ": byte " + std::to_string(at) + " is wrong");
    }
    for (const auto& [address, size] : reads) {
        for (const auto& [begin, length] : skipped) check(size == 0 || address + size <= begin || address >= begin + length, what + ": a skipped range was read");
    }
}

void testReadAroundRanges() {
    for (std::size_t at = 0; at < guest.size(); ++at) guest[at] = static_cast<std::byte>(at + 1);
    checkReadAround({{GuestBase + 20, 8}}, 20, 28, "a range in the middle");
    checkReadAround({{GuestBase + 4, 10}}, 8, 14, "a range over the start");
    checkReadAround({{GuestBase + 50, 20}}, 50, 56, "a range over the end");
    checkReadAround({{GuestBase + 16, 8}, {GuestBase + 20, 8}}, 16, 28, "overlapping ranges");
    checkReadAround({{GuestBase, 4}, {GuestBase + 60, 4}}, 0, 0, "ranges outside the block");
    checkReadAround({{GuestBase, 64}}, 8, 56, "a range over the whole block");
}

void testAllZero() {
    std::vector<std::byte> bytes(37);
    check(AgcDriver::AllZero(bytes), "zero bytes are not all zero");
    check(AgcDriver::AllZero({}), "an empty span is not all zero");
    bytes[36] = std::byte{1};
    check(!AgcDriver::AllZero(bytes), "a nonzero tail byte is missed");
    bytes[36] = std::byte{0};
    bytes[3] = std::byte{1};
    check(!AgcDriver::AllZero(bytes), "a nonzero word is missed");
}

void testReusedStreams(const std::filesystem::path& directory) {
    std::vector<std::vector<std::byte>> deltas;
    for (std::size_t index = 0; index < 6; ++index) {
        std::vector<std::byte> delta(index % 2 == 0 ? FrameCaptureWriter::ChunkBytes : 4096 + 8 * index);
        for (std::size_t at = 0; at < delta.size(); ++at) delta[at] = static_cast<std::byte>((((at + 1) * (index + 3) * 2654435761ull) >> 13) & 0xff);
        deltas.push_back(std::move(delta));
    }
    {
        FrameCaptureWriter writer(directory);
        for (std::size_t index = 0; index < deltas.size(); ++index) writer.WriteDelta(index + 1, 0, 0x1000000ull * (index + 1), deltas[index]);
        writer.Finish("{}");
    }
    const auto memoryPath = directory / FrameCaptureWriter::MemoryName;
    {
        const FrameCaptureReader unopened(directory);
        check(unopened.Chunks().size() == deltas.size(), "a delta of at most one chunk was split");
        const auto moved = directory / "memory.moved";
        std::filesystem::rename(memoryPath, moved);
        checkThrowsText([&] { unopened.Read(unopened.Chunks()[1]); }, "frame capture: cannot open " + memoryPath.string(), false, "a read with the memory file gone does not throw the cannot-open error");
        std::filesystem::rename(moved, memoryPath);
        check(unopened.Read(unopened.Chunks()[1]) == deltas[1], "a read after a failed open does not round-trip");
    }
    const FrameCaptureReader reader(directory);
    const auto& chunks = reader.Chunks();
    check(chunks.size() == deltas.size(), "a delta of at most one chunk was split");
    for (std::size_t step = 0; step < 2 * chunks.size(); ++step) {
        const auto index = step < chunks.size() ? step : 2 * chunks.size() - 1 - step;
        check(reader.Read(chunks[index]) == deltas[index], "a chunk read after a larger or a smaller one does not round-trip");
    }
    auto past = chunks[1];
    past.offset = std::filesystem::file_size(memoryPath) - 1;
    checkThrowsText([&] { reader.Read(past); }, "frame capture: cannot read " + memoryPath.string(), false, "a chunk that ends past the memory file does not throw the cannot-read error");
    check(reader.Read(chunks[1]) == deltas[1], "a read after a failed file read does not round-trip");
    auto shifted = chunks[1];
    ++shifted.offset;
    checkThrowsText([&] { reader.Read(shifted); }, "frame capture: zstd: ", true, "a chunk that does not start a zstd frame does not throw the zstd error");
    check(reader.Read(chunks[1]) == deltas[1], "a read after a failed decompression does not round-trip");
    auto bigger = chunks[1];
    ++bigger.bytes;
    checkThrowsText([&] { reader.Read(bigger); }, "frame capture: chunk at " + std::to_string(bigger.address) + " decompressed to the wrong size", false, "a chunk that decompresses to fewer bytes than recorded does not throw the wrong-size error");
    check(reader.Read(chunks[1]) == deltas[1], "a read after a wrong-size decompression does not round-trip");
    constexpr std::size_t Workers = 4;
    std::atomic<std::size_t> started{0};
    std::atomic<std::size_t> reading{0};
    std::atomic<std::size_t> overlapped{0};
    std::atomic<bool> differs{false};
    std::vector<std::thread> workers;
    for (std::size_t worker = 0; worker < Workers; ++worker) {
        workers.emplace_back([&, worker] {
            ++started;
            while (started.load() < Workers) std::this_thread::yield();
            for (std::size_t step = 0; step < 4 * chunks.size(); ++step) {
                const auto index = (worker + step) % chunks.size();
                if (++reading > 1) ++overlapped;
                try {
                    if (reader.Read(chunks[index]) != deltas[index]) differs = true;
                } catch (const std::exception&) {
                    differs = true;
                }
                --reading;
            }
        });
    }
    for (auto& worker : workers) worker.join();
    check(!differs.load(), "chunks read on several threads at once do not round-trip");
    check(overlapped.load() > 0, "no two chunk reads ran at the same time");
    started = 0;
    workers.clear();
    for (std::size_t worker = 0; worker < Workers; ++worker) {
        workers.emplace_back([&, worker] {
            ++started;
            while (started.load() < Workers) std::this_thread::yield();
            for (std::size_t step = 0; step < 2000; ++step) {
                const auto index = 1 + 2 * ((worker + step) % (chunks.size() / 2));
                try {
                    if (reader.Read(chunks[index]) != deltas[index]) differs = true;
                } catch (const std::exception&) {
                    differs = true;
                }
            }
        });
    }
    for (auto& worker : workers) worker.join();
    check(!differs.load(), "small chunks read on several threads at once do not round-trip");
}

}

int main(int argc, char** argv) {
    const auto root = std::filesystem::temp_directory_path() / ("aps5-frame-capture-test-" + std::to_string(getpid()));
    std::filesystem::remove_all(root);
    if (argc > 1) {
        check(argc == 2 && std::string_view(argv[1]) == "reuse", std::string("unknown test ") + argv[1]);
        testReusedStreams(root / "reuse");
        std::filesystem::remove_all(root);
        std::puts("frame capture stream reuse test passed");
        return 0;
    }
    testAllZero();
    testReadAroundRanges();
    testRoundTrip(root / "capture");
    testTruncated(root / "capture", root / "truncated");
    testOlderVersion(root / "capture", root / "version-one", "aps5-frame-calls 1\n", 1);
    testOlderVersion(root / "capture", root / "version-two", "aps5-frame-calls 2\n", 2);
    testCorrupt(root / "capture", root / "corrupt");
    testWriter(root / "writer");
    std::filesystem::remove_all(root);
    std::puts("frame capture tests passed");
    return 0;
}
