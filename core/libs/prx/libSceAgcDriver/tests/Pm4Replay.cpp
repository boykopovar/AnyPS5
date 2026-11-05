#include "prx/libSceAgc/Patch/include/Rewind.hpp"
#include "prx/libSceAgc/DcbFlow/include/Sync.hpp"
#include "prx/libSceAgc/DcbFlow/include/Memory.hpp"
#include "prx/libSceAgc/Cb/include/Sync.hpp"
#include "prx/libSceAgc/Acb/include/Sync.hpp"
#include "prx/libSceAgc/Acb/include/Memory.hpp"
#include "prx/libSceAgc/Acb/include/Control.hpp"
#include "../tools/Pm4Analysis.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver.hpp"
#include "prx/libSceAgcDriver/Execution/include/Driver/Synchronization/LabelStores.hpp"
#include "prx/libSceAgcDriver/Execution/include/SubmissionCapture.hpp"
#include "prx/libSceAgcDriver/Submit/include/Acb.hpp"
#include "prx/libSceAgcDriver/Submit/include/Dcb.hpp"
#include "prx/libc/include/Shutdown.hpp"
#include "SceTypes.hpp"
#include "tests/TemporaryFile.hpp"
#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using AgcDriver::CapturedSubmission;
using AgcDriver::SubmissionCapture;
using AgcDriver::SubmissionKind;
using AgcDriver::Pm4Analysis::WaitReport;
using AgcDriver::Pm4Analysis::WriterMatch;

constexpr std::uint8_t GraphicsMemory = 0x0a;
constexpr std::uint8_t ComputeMemory = 5;
constexpr std::uint8_t Equal = 3;
constexpr std::uint8_t Bits32 = 0;
constexpr std::uint8_t Bits64 = 1;
constexpr std::uint8_t ReleaseEvent = 0x14;
constexpr std::uint8_t Data32 = 1;
constexpr std::uint8_t Data64 = 2;
constexpr std::uint8_t DmaMemory = 0;
constexpr std::uint8_t DmaFill = 2;
constexpr std::uint32_t WrittenValue = 0x11;
constexpr std::uint64_t ReleasedValue = 0x0000123400000007ull;
constexpr std::uint32_t FillValue = 0x5a5a5a5au;
constexpr std::uint32_t HeldValue = 9;
constexpr std::uint32_t CpuValue = 5;
constexpr std::uint32_t One = 1;
constexpr std::uint32_t Zero = 0;
constexpr std::uint32_t BeforeRewindValue = 1;
constexpr std::uint32_t AfterRewindValue = 2;
constexpr std::uint32_t WriteDataWords = 5;

alignas(8) std::uint32_t writtenLabel = 0;
alignas(8) Label releasedLabel{};
alignas(16) std::array<std::uint32_t, 4> filled{};
alignas(8) std::uint32_t heldLabel = 0;
alignas(8) std::uint32_t cpuLabel = 0;
alignas(8) std::uint32_t reusedLabel = 0;
alignas(8) std::uint32_t conditionLabel = 0;
alignas(8) std::uint32_t guardedLabel = 0;
alignas(8) std::uint32_t beforeRewind = 0;
alignas(8) std::uint32_t afterRewind = 0;
alignas(8) std::uint32_t syntheticLabel = 0;
alignas(8) std::uint32_t otherLabel = 0;

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::uint64_t at(const volatile void* pointer) {
    return static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(pointer));
}

std::string hex(const volatile void* pointer) {
    char text[32];
    std::snprintf(text, sizeof(text), "0x%llx", static_cast<unsigned long long>(at(pointer)));
    return text;
}

class Commands {
public:
    Commands() = default;
    Commands(const Commands&) = delete;
    Commands& operator=(const Commands&) = delete;
    CommandBuffer* Buffer() { return &buffer; }
    std::vector<std::uint32_t> Words(std::size_t from = 0) const { return {words.begin() + static_cast<std::ptrdiff_t>(from), words.begin() + (buffer.cursor_up - words.data())}; }
    std::size_t Offset(const std::uint32_t* packet) const { return static_cast<std::size_t>(packet - words.data()); }
    std::size_t Size() const { return static_cast<std::size_t>(buffer.cursor_up - words.data()); }
    void Submit(std::uint32_t queue) {
        Packet packet{words.data(), static_cast<std::uint32_t>(buffer.cursor_up - words.data()), 0, {}};
        check((queue == 0 ? sceAgcDriverSubmitDcb(&packet) : sceAgcDriverSubmitAcb(queue, &packet)) == 0, "submission failed");
    }

private:
    std::array<std::uint32_t, 160> words{};
    CommandBuffer buffer{words.data(), words.data() + words.size(), words.data(), words.data() + words.size(), nullptr, nullptr, 0};
};

std::size_t waitAt(const Commands& commands, const std::uint32_t* firstTag) {
    return commands.Offset(firstTag) + 4;
}

std::vector<std::string> names(CapturedSubmission submission) {
    std::vector<std::string> result;
    for (const auto& packet : AgcDriver::Pm4Analysis::Decode(submission)) result.push_back(packet.name);
    return result;
}

const WaitReport& waitOn(const AgcDriver::Pm4Analysis::Analysis& analysis, const volatile void* address, std::size_t skip = 0) {
    for (const auto& report : analysis.waits) {
        if (report.wait.kind != AgcDriver::Pm4Analysis::MemoryWait::Kind::Memory || report.wait.fields.address != at(address)) continue;
        if (skip-- == 0) return report;
    }
    throw std::runtime_error("no wait on " + hex(address) + " in the analysis");
}

bool listed(const WaitReport& report, WriterMatch::Effect effect, std::uint64_t ordinal) {
    return std::any_of(report.writers.begin(), report.writers.end(), [&](const WriterMatch& writer) { return writer.effect == effect && writer.at.ordinal == ordinal; });
}

std::string verdict(const WaitReport& report) {
    return std::to_string(static_cast<int>(report.verdict)) + " route " + std::to_string(static_cast<int>(report.route)) + ": " + report.reason;
}

std::pair<int, std::string> report(const std::filesystem::path& directory, bool full = false) {
    std::FILE* output = TemporaryFile::Open();
    check(output != nullptr, "cannot create a temporary file for the report");
    const auto code = AgcDriver::Pm4Analysis::Report(directory, full, output);
    std::string text;
    std::rewind(output);
    for (int character; (character = std::fgetc(output)) != EOF;) text.push_back(static_cast<char>(character));
    std::fclose(output);
    return {code, text};
}

std::vector<std::string> lines(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    std::vector<std::string> result;
    for (std::string line; std::getline(input, line);) result.push_back(line);
    return result;
}

std::vector<std::string> fields(const std::string& line) {
    std::istringstream input(line);
    std::vector<std::string> result;
    for (std::string field; input >> field;) result.push_back(field);
    return result;
}

struct Planned {
    std::uint32_t queue = 0;
    std::uint64_t serial = 0;
    SubmissionKind kind = SubmissionKind::Submit;
    std::vector<std::uint32_t> commands;
    std::set<std::size_t> held;
    std::map<std::size_t, std::size_t> conditional;
};

Planned planned(std::uint32_t queue, std::uint64_t serial, const Commands& commands, std::set<std::size_t> held = {}) {
    return {queue, serial, SubmissionKind::Submit, commands.Words(), std::move(held), {}};
}

void writeCapture(const std::filesystem::path& directory, const std::vector<Planned>& submissions) {
    SubmissionCapture capture(directory);
    for (const auto& submission : submissions) {
        auto captured = SubmissionCapture::Prepare(submission.queue, submission.kind, submission.commands, submission.conditional);
        capture.Take(captured, submission.serial, submission.serial * 1000, submission.held);
        capture.Write(captured);
    }
}

void testDriverCapture(const std::filesystem::path& directory) {
    heldLabel = HeldValue;
    conditionLabel = One;

    Commands graphicsWrites;
    sceAgcDcbWriteData(graphicsWrites.Buffer(), GraphicsMemory, 0, at(&writtenLabel), &WrittenValue, 1, 0, 1);
    sceAgcDcbDmaData(graphicsWrites.Buffer(), 0, DmaMemory, 0, at(filled.data()), DmaFill, 0, FillValue, sizeof(filled), 0, 1, 0);
    graphicsWrites.Submit(0);

    Commands computeRelease;
    sceAgcAcbWaitRegMem(computeRelease.Buffer(), Bits32, Equal, 0, &writtenLabel, WrittenValue, 0xffffffffu, 0);
    sceAgcCbReleaseMem(computeRelease.Buffer(), ReleaseEvent, 0, 0, 0, &releasedLabel, Data64, ReleasedValue, 0, 0, 0, 0);
    computeRelease.Submit(0x20);

    AgcDriverSuspendPoint_nid_postfix();

    Commands graphicsWaits;
    sceAgcDcbWaitRegMem(graphicsWaits.Buffer(), Bits64, Equal, 0, 0, &releasedLabel, ReleasedValue, ~0ull, 0);
    sceAgcDcbWaitRegMem(graphicsWaits.Buffer(), Bits32, Equal, 0, 0, &filled[2], FillValue, 0xffffffffu, 0);
    const auto heldWait = waitAt(graphicsWaits, sceAgcDcbWaitRegMem(graphicsWaits.Buffer(), Bits32, Equal, 0, 0, &heldLabel, HeldValue, 0xffffffffu, 0));
    const auto cpuWait = waitAt(graphicsWaits, sceAgcDcbWaitRegMem(graphicsWaits.Buffer(), Bits32, Equal, 0, 0, &cpuLabel, CpuValue, 0xffffffffu, 0));
    sceAgcDcbWriteData(graphicsWaits.Buffer(), GraphicsMemory, 0, at(&reusedLabel), &One, 1, 0, 1);
    sceAgcDcbWriteData(graphicsWaits.Buffer(), GraphicsMemory, 0, at(&reusedLabel), &Zero, 1, 0, 1);
    sceAgcDcbWaitRegMem(graphicsWaits.Buffer(), Bits32, Equal, 0, 0, &reusedLabel, One, 0xffffffffu, 0);
    const auto condition = graphicsWaits.Offset(sceAgcDcbCondExec(graphicsWaits.Buffer(), &conditionLabel, WriteDataWords));
    sceAgcDcbWriteData(graphicsWaits.Buffer(), GraphicsMemory, 0, at(&guardedLabel), &One, 1, 0, 1);
    sceAgcDcbWaitRegMem(graphicsWaits.Buffer(), Bits32, Equal, 0, 0, &guardedLabel, One, 0xffffffffu, 0);
    graphicsWaits.Submit(0);
    std::atomic_ref<std::uint32_t>(cpuLabel).store(CpuValue);

    Commands rewound;
    sceAgcAcbWriteData(rewound.Buffer(), ComputeMemory, 0, at(&beforeRewind), &BeforeRewindValue, 1, 0, 1);
    auto* rewind = sceAgcAcbRewind(rewound.Buffer(), 0);
    const auto tail = rewound.Offset(rewind) + 2;
    sceAgcAcbWriteData(rewound.Buffer(), ComputeMemory, 0, at(&afterRewind), &AfterRewindValue, 1, 0, 1);
    const auto submittedRewound = rewound.Words();
    rewound.Submit(0x21);
    sceAgcAsyncRewindPatchSetRewindState(rewind, 1);
    AgcDriverWaitIdle_nid_postfix();
    check(std::atomic_ref<std::uint32_t>(beforeRewind).load() == BeforeRewindValue && std::atomic_ref<std::uint32_t>(afterRewind).load() == AfterRewindValue, "the driver did not execute the captured submissions");
    check(std::atomic_ref<std::uint32_t>(guardedLabel).load() == One, "the driver did not execute the WRITE_DATA its COND_EXEC guards");

    const auto index = lines(directory / SubmissionCapture::IndexName);
    check(index.size() == 8 && index[0] == SubmissionCapture::Magic && index[1] == SubmissionCapture::Columns, "the index does not hold its two header lines and one line per submission");
    const auto computeLine = fields(index[3]);
    check(computeLine.size() == 10 && computeLine[0] == "1" && computeLine[1] == "0x20" && computeLine[2] == "compute" && computeLine[6] == "submit" && computeLine[7] == "submission_000001.bin" && computeLine[9] == "-", "the index line of the compute submission is " + index[3]);
    check(index[4].starts_with("2 0x0 graphics ") && index[4].ends_with(" 0 suspend - - -"), "the index line of the suspend point is " + index[4]);
    const auto waitsLine = fields(index[5]);
    check(waitsLine.size() == 10 && waitsLine[0] == "3", "the index line of the graphics waits is " + index[5]);
    const auto conditionalEnd = condition + 5 + WriteDataWords;
    check(waitsLine[9] == std::to_string(condition) + ":" + std::to_string(conditionalEnd), "the index does not list the COND_EXEC range: " + index[5]);

    const auto captured = SubmissionCapture::Read(directory);
    check(captured.size() == 6, "the capture does not hold the four submissions, the suspend point and the rewind tail");
    const std::vector<std::uint32_t> queues{0, 0x20, 0, 0, 0x21, 0x21};
    const std::vector<SubmissionKind> kinds{SubmissionKind::Submit, SubmissionKind::Submit, SubmissionKind::Suspend, SubmissionKind::Submit, SubmissionKind::Submit, SubmissionKind::Rewind};
    for (std::size_t i = 0; i < captured.size(); ++i) {
        check(captured[i].ordinal == i && captured[i].queue == queues[i] && captured[i].kind == kinds[i], "captured submission " + std::to_string(i) + " has the wrong ordinal, queue or kind");
        check(i == 0 || captured[i].timeUs >= captured[i - 1].timeUs, "capture times go backwards");
        check(i == 0 || i == 5 || captured[i].serial > captured[i - 1].serial, "capture serials are not in submission order");
    }
    const auto& held = captured[3].heldAtSubmit;
    check(std::find(held.begin(), held.end(), heldWait) != held.end(), "the wait on a label that held its value at submit is not marked held: " + index[5]);
    check(std::find(held.begin(), held.end(), cpuWait) == held.end(), "the wait on a label the CPU stores after the submit is marked held: " + index[5]);
    check(captured[5].serial == captured[4].serial, "the rewind tail does not carry the serial of its submission");
    check(captured[0].commands == graphicsWrites.Words() && captured[1].commands == computeRelease.Words() && captured[2].commands.empty() && captured[3].commands == graphicsWaits.Words(), "captured dwords differ from the submitted ones");
    check(captured[4].commands == std::vector<std::uint32_t>(submittedRewound.begin(), submittedRewound.begin() + static_cast<std::ptrdiff_t>(tail)), "captured dwords up to the REWIND differ from the submitted ones, whose valid bit was still clear");
    check(captured[5].commands == rewound.Words(tail), "captured dwords of the rewind tail differ from the submitted ones");

    check(names(captured[0]) == std::vector<std::string>{"WRITE_DATA", "DMA_DATA"}, "decoded packets of the graphics writes");
    check(names(captured[1]) == std::vector<std::string>{"SET_UCONFIG_REG", "WAIT_REG_MEM", "SET_UCONFIG_REG", "RELEASE_MEM"}, "decoded packets of the compute release");
    check(names(captured[4]) == std::vector<std::string>{"WRITE_DATA", "REWIND"} && names(captured[5]) == std::vector<std::string>{"WRITE_DATA"}, "decoded packets of the rewound submission");
    const auto release = AgcDriver::Pm4Analysis::Decode(captured[1]).back();
    check(release.operands.find("address " + hex(&releasedLabel)) != std::string::npos, "RELEASE_MEM operands do not name its label: " + release.operands);
    check(release.stores.size() == 1 && release.stores[0].store.address == at(&releasedLabel) && release.stores[0].store.bytes == 8 && release.stores[0].label, "RELEASE_MEM does not decode to its 8-byte label store");
    const auto fill = AgcDriver::Pm4Analysis::Decode(captured[0]).back();
    check(fill.stores.size() == 1 && fill.stores[0].store.address == at(filled.data()) && fill.stores[0].store.bytes == sizeof(filled) && fill.stores[0].store.value == AgcDriver::Pm4::PacketStore::Value::Pattern && !fill.stores[0].label, "DMA_DATA does not decode to its fill");
    const auto waits = AgcDriver::Pm4Analysis::Decode(captured[3]);
    const auto guarded = std::find_if(waits.begin(), waits.end(), [](const auto& packet) { return !packet.stores.empty() && packet.stores[0].store.address == at(&guardedLabel); });
    check(guarded != waits.end() && guarded->conditional && guarded->stores[0].conditional, "the WRITE_DATA inside the COND_EXEC range is not conditional");

    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    check(analysis.waits.size() == 8, "expected eight waits, got " + std::to_string(analysis.waits.size()));
    for (const auto* label : {static_cast<const volatile void*>(&writtenLabel), static_cast<const volatile void*>(&releasedLabel)}) {
        const auto& crossQueue = waitOn(analysis, label);
        check((crossQueue.verdict == WaitReport::Verdict::Released && crossQueue.route == WaitReport::Route::HeldAtSubmit) || (crossQueue.verdict == WaitReport::Verdict::Unknown && crossQueue.crossQueue),
              "a wait on a store of another queue is decided by something else than its value at submit: " + verdict(crossQueue));
    }
    check(waitOn(analysis, &releasedLabel).wait.fields.bytes == 8, "the wait on the RELEASE_MEM label does not await 8 bytes");
    const auto& filledWait = waitOn(analysis, &filled[2]);
    check(filledWait.verdict == WaitReport::Verdict::Released && (filledWait.route == WaitReport::Route::HeldAtSubmit || (filledWait.route == WaitReport::Route::CurrentValue && listed(filledWait, WriterMatch::Effect::CurrentValue, 0))),
          "the wait inside the fill is not released by the earlier DMA_DATA on its queue: " + verdict(filledWait));
    const auto& heldReport = waitOn(analysis, &heldLabel);
    check(heldReport.verdict == WaitReport::Verdict::Released && heldReport.route == WaitReport::Route::HeldAtSubmit, "the wait whose value held at submit is not released by it: " + verdict(heldReport));
    const auto& cpu = waitOn(analysis, &cpuLabel);
    check(cpu.verdict == WaitReport::Verdict::Unreleased && cpu.writerCount == 0 && cpu.reason.starts_with("unreleased by the GPU; a CPU store could release it"), "the wait on a label the CPU stores after the submit is not unreleased: " + verdict(cpu));
    const auto& reused = waitOn(analysis, &reusedLabel);
    check(reused.verdict == WaitReport::Verdict::Released && reused.route == WaitReport::Route::LabelHistory && listed(reused, WriterMatch::Effect::LabelHistory, 3),
          "the wait after a store of 1 and a store of 0 in its own submission is not released by the label history, as the driver releases it: " + verdict(reused));
    const auto& conditional = waitOn(analysis, &guardedLabel);
    check(conditional.verdict == WaitReport::Verdict::Unknown && !conditional.crossQueue && listed(conditional, WriterMatch::Effect::Undecided, 3) && conditional.reason.find("is conditional") != std::string::npos,
          "the wait on a store inside a COND_EXEC range is not unknown: " + verdict(conditional));
    const auto rewindWait = std::find_if(analysis.waits.begin(), analysis.waits.end(), [](const auto& report) { return report.wait.kind == AgcDriver::Pm4Analysis::MemoryWait::Kind::Rewind; });
    check(rewindWait != analysis.waits.end() && rewindWait->verdict == WaitReport::Verdict::Released && rewindWait->route == WaitReport::Route::RewindTail && rewindWait->at.ordinal == 4, "the REWIND is not released by its captured tail");
    check(analysis.Count(WaitReport::Verdict::Unreleased) == 1 && analysis.Count(WaitReport::Verdict::Released) + analysis.Count(WaitReport::Verdict::Unknown) == 7, "expected one unreleased wait and seven released or unknown");
    check(report(directory).first == 1, "the report does not fail on an unreleased wait");
}

void testWriterPresent(const std::filesystem::path& source, const std::filesystem::path& directory) {
    std::vector<Planned> submissions;
    for (const auto& captured : SubmissionCapture::Read(source)) {
        Planned copy{captured.queue, captured.serial, captured.kind, captured.commands, {captured.heldAtSubmit.begin(), captured.heldAtSubmit.end()}, {captured.conditionalRanges.begin(), captured.conditionalRanges.end()}};
        submissions.push_back(std::move(copy));
    }
    Commands cpuWriter;
    sceAgcAcbWriteData(cpuWriter.Buffer(), ComputeMemory, 0, at(&cpuLabel), &CpuValue, 1, 0, 1);
    submissions.push_back(planned(0x22, submissions.back().serial + 1, cpuWriter));
    writeCapture(directory, submissions);
    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    check(analysis.submissions.size() == submissions.size() && analysis.waits.size() == 8, "the rewritten capture does not hold the submissions and their waits");
    check(analysis.Count(WaitReport::Verdict::Unreleased) == 0, "a wait is unreleased although a store in the capture may release it");
    const auto& cpu = waitOn(analysis, &cpuLabel);
    check(cpu.verdict == WaitReport::Verdict::Unknown && cpu.crossQueue && cpu.reason.starts_with("unknown: depends on cross-queue timing (stores on queue 0x22 at #6)") && cpu.writers.front().at.queue == 0x22 && cpu.writers.front().order == WriterMatch::Order::OtherQueue,
          "the wait on a label another queue stores is not unknown by cross-queue timing: " + verdict(cpu));
    check(report(directory).first == 0, "the report fails without an unreleased wait");
}

void testOverwrittenAcrossSubmissions(const std::filesystem::path& directory) {
    Commands storeOne, storeZero, waitOne;
    sceAgcDcbWriteData(storeOne.Buffer(), GraphicsMemory, 0, at(&syntheticLabel), &One, 1, 0, 1);
    sceAgcDcbWriteData(storeZero.Buffer(), GraphicsMemory, 0, at(&syntheticLabel), &Zero, 1, 0, 1);
    sceAgcDcbWaitRegMem(waitOne.Buffer(), Bits32, Equal, 0, 0, &syntheticLabel, One, 0xffffffffu, 0);
    writeCapture(directory, {planned(0, 1, storeOne), planned(0, 2, storeZero), planned(0, 3, waitOne)});
    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    const auto& wait = waitOn(analysis, &syntheticLabel);
    check(wait.verdict == WaitReport::Verdict::Unknown && !wait.crossQueue && listed(wait, WriterMatch::Effect::Rejected, 1) && listed(wait, WriterMatch::Effect::Overwritten, 0) && wait.reason.find("depends on when it ran") != std::string::npos,
          "a store of 1 in an earlier submission, overwritten by a store of 0, does not leave the wait unknown (the label history holds it only if it ran after the waiting submission entered the driver): " + verdict(wait));
}

void testLabelHistory(const std::filesystem::path& directory, std::size_t newer) {
    constexpr std::array<std::uint32_t, 8> Newer{2, 3, 4, 5, 6, 7, 8, 9};
    Commands graphics;
    sceAgcDcbWriteData(graphics.Buffer(), GraphicsMemory, 0, at(&syntheticLabel), &One, 1, 0, 1);
    for (std::size_t i = 0; i < newer; ++i) sceAgcDcbWriteData(graphics.Buffer(), GraphicsMemory, 0, at(&syntheticLabel), &Newer.at(i), 1, 0, 1);
    sceAgcDcbWaitRegMem(graphics.Buffer(), Bits32, Equal, 0, 0, &syntheticLabel, One, 0xffffffffu, 0);
    writeCapture(directory, {planned(0, 1, graphics)});
    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    const auto& wait = waitOn(analysis, &syntheticLabel);
    if (newer < AgcDriver::DriverDetail::LabelStoreHistory) {
        check(wait.verdict == WaitReport::Verdict::Released && wait.route == WaitReport::Route::LabelHistory && listed(wait, WriterMatch::Effect::LabelHistory, 0), "a store kept in the label history does not release the wait: " + verdict(wait));
    } else {
        check(wait.verdict == WaitReport::Verdict::Unknown && !wait.crossQueue && listed(wait, WriterMatch::Effect::Evicted, 0), "a store pushed out of the label history releases the wait: " + verdict(wait));
    }
}

void testUnreleasable(const std::filesystem::path& directory) {
    constexpr std::uint32_t StoredValue = 3;
    Commands waitFirst, writeAfter, releaseOther, waitOther, rewindForever;
    sceAgcDcbWaitRegMem(waitFirst.Buffer(), Bits32, Equal, 0, 0, &syntheticLabel, One, 0xffffffffu, 0);
    sceAgcDcbWriteData(writeAfter.Buffer(), GraphicsMemory, 0, at(&syntheticLabel), &One, 1, 0, 1);
    sceAgcCbReleaseMem(releaseOther.Buffer(), ReleaseEvent, 0, 0, 0, reinterpret_cast<const volatile Label*>(&otherLabel), Data32, StoredValue, 0, 0, 0, 0);
    sceAgcDcbWaitRegMem(waitOther.Buffer(), Bits32, Equal, 0, 0, &otherLabel, StoredValue + 1, 0xffffffffu, 0);
    sceAgcAcbRewind(rewindForever.Buffer(), 0);
    writeCapture(directory, {planned(0, 1, waitFirst), planned(0, 2, writeAfter), planned(0x20, 3, releaseOther), planned(0, 4, waitOther), planned(0x23, 5, rewindForever)});
    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    check(analysis.waits.size() == 3 && analysis.Count(WaitReport::Verdict::Unreleased) == 3, "expected three unreleased waits");
    const auto& later = waitOn(analysis, &syntheticLabel);
    check(later.writerCount == 1 && listed(later, WriterMatch::Effect::After, 1) && later.writers.front().order == WriterMatch::Order::LaterOnQueue && later.reason.find("are later on its queue and run after it") != std::string::npos,
          "a store later on the waiting queue releases the wait: " + verdict(later));
    const auto& rejected = waitOn(analysis, &otherLabel);
    check(rejected.writerCount == 1 && listed(rejected, WriterMatch::Effect::OtherQueue, 2) && !rejected.writers.front().accepted && rejected.writers.front().stored == StoredValue && !rejected.crossQueue,
          "a store of another queue the compare rejects does not leave the wait unreleased: " + verdict(rejected));
    check(analysis.waits.back().verdict == WaitReport::Verdict::Unreleased && analysis.waits.back().wait.kind == AgcDriver::Pm4Analysis::MemoryWait::Kind::Rewind, "a REWIND without a tail is not reported as unreleased");

    const auto [code, text] = report(directory);
    const auto newest = text.find("UNRELEASED #4 ");
    const auto middle = text.find("UNRELEASED #3 ");
    const auto oldest = text.find("UNRELEASED #0 ");
    check(code == 1 && newest != std::string::npos && middle != std::string::npos && oldest != std::string::npos && newest < middle && middle < oldest, "the report does not list unreleased waits newest first:\n" + text);
}

std::vector<std::uint32_t> rawWait(std::uint64_t address, std::uint32_t reference) {
    return {0xc0053c00u, 0x13u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), reference, 0xffffffffu, 0};
}

std::vector<std::uint32_t> rawWrite(std::uint64_t address, std::uint32_t value) {
    return {0xc0033700u, 0x100500u, static_cast<std::uint32_t>(address), static_cast<std::uint32_t>(address >> 32u), value};
}

std::vector<std::uint32_t> joined(std::vector<std::uint32_t> first, const std::vector<std::uint32_t>& second) {
    first.insert(first.end(), second.begin(), second.end());
    return first;
}

std::vector<std::uint32_t> repeated(std::size_t count, const std::vector<std::uint32_t>& packet) {
    std::vector<std::uint32_t> result;
    for (std::size_t i = 0; i < count; ++i) result = joined(std::move(result), packet);
    return result;
}

Planned raw(std::uint32_t queue, std::uint64_t serial, std::vector<std::uint32_t> commands, std::set<std::size_t> held = {}) {
    return {queue, serial, SubmissionKind::Submit, std::move(commands), std::move(held), {}};
}

const WaitReport& waitAtAddress(const AgcDriver::Pm4Analysis::Analysis& analysis, std::uint64_t address) {
    for (const auto& report : analysis.waits) {
        if (report.wait.kind == AgcDriver::Pm4Analysis::MemoryWait::Kind::Memory && report.wait.fields.address == address) return report;
    }
    throw std::runtime_error("no wait on the address in the analysis");
}

void testReviewerCaptures(const std::filesystem::path& directory) {
    constexpr std::uint64_t A = 0x10000100;
    constexpr std::uint64_t B = 0x10000200;
    constexpr std::size_t StaleComputeWait = 5;
    const auto stale = [&](std::set<std::size_t> held) { return std::vector<Planned>{raw(0, 1, rawWrite(B, 1)), raw(0x20, 2, joined(joined(rawWrite(A, 1), rawWait(B, 1)), rawWrite(A, 0)), std::move(held)), raw(0, 3, rawWait(A, 1))}; };
    const auto evict = [&](std::size_t newer) { return std::vector<Planned>{raw(0, 1, joined(rawWait(B, 1), rawWait(A, 1))), raw(0x20, 2, joined(joined(rawWrite(A, 1), repeated(newer, rawWrite(A, 2))), rawWrite(B, 1)))}; };
    const std::vector<std::pair<std::string, std::vector<Planned>>> captures{
        {"deadlock", {raw(0, 1, joined(rawWait(A, 1), rawWrite(B, 1))), raw(0x20, 2, joined(rawWait(B, 1), rawWrite(A, 1)))}},
        {"stale", stale({})},
        {"stale-held", stale({StaleComputeWait})},
        {"evict3", evict(3)},
        {"evict4", evict(4)},
        {"evict-own4", {raw(0, 1, joined(joined(rawWrite(A, 1), rawWait(B, 1)), rawWait(A, 1))), raw(0x20, 2, joined(repeated(4, rawWrite(A, 0)), rawWrite(B, 1)))}},
    };
    for (const auto& [name, submissions] : captures) {
        const auto path = directory / name;
        writeCapture(path, submissions);
        const auto analysis = AgcDriver::Pm4Analysis::Analyze(path);
        check(analysis.waits.size() == 2, "the reviewer capture " + name + " does not hold its two waits");
        for (const auto& wait : analysis.waits) {
            const bool held = wait.wait.heldAtSubmit && wait.verdict == WaitReport::Verdict::Released && wait.route == WaitReport::Route::HeldAtSubmit;
            check(held || (wait.verdict == WaitReport::Verdict::Unknown && wait.crossQueue && wait.reason.starts_with("unknown: depends on cross-queue timing (stores on queue ")),
                  "the reviewer capture " + name + " reports the wait at #" + std::to_string(wait.at.ordinal) + " +" + std::to_string(wait.at.offset) + " as " + verdict(wait));
        }
        check(analysis.Count(WaitReport::Verdict::Released) == (name == "stale-held" ? 1u : 0u), "the reviewer capture " + name + " releases a wait the capture does not prove released");
        check(report(path).first == 0, "the report of the reviewer capture " + name + " claims an unreleased wait");
    }
}

void testProvableRoutes(const std::filesystem::path& directory) {
    constexpr std::uint64_t A = 0x10000100;
    constexpr std::uint64_t B = 0x10000200;
    constexpr std::uint64_t C = 0x10000300;
    constexpr std::uint32_t AlwaysCompare = 0x10;
    const auto analyzed = [&](const std::string& name, const std::vector<Planned>& submissions) {
        writeCapture(directory / name, submissions);
        return AgcDriver::Pm4Analysis::Analyze(directory / name);
    };
    auto always = rawWait(C, 1);
    always[1] = AlwaysCompare;

    const auto own = analyzed("own-submission", {raw(0, 1, joined(joined(rawWrite(A, 1), rawWait(A, 1)), always))});
    const auto& ownWait = waitAtAddress(own, A);
    check(ownWait.verdict == WaitReport::Verdict::Released && ownWait.route == WaitReport::Route::CurrentValue && listed(ownWait, WriterMatch::Effect::CurrentValue, 0), "a store earlier in the waiting submission does not release the wait: " + verdict(ownWait));
    const auto& alwaysWait = waitAtAddress(own, C);
    check(alwaysWait.verdict == WaitReport::Verdict::Released && alwaysWait.route == WaitReport::Route::Always, "a wait whose compare always passes is not released: " + verdict(alwaysWait));

    const auto earlier = analyzed("earlier-submission", {raw(0, 1, rawWrite(A, 1)), raw(0x20, 2, rawWrite(B, 5)), raw(0, 3, rawWait(A, 1))});
    const auto& earlierWait = waitAtAddress(earlier, A);
    check(earlierWait.verdict == WaitReport::Verdict::Released && earlierWait.route == WaitReport::Route::CurrentValue && listed(earlierWait, WriterMatch::Effect::CurrentValue, 0),
          "a store in an earlier submission on the waiting queue, with no store of another queue to its bytes, does not release the wait: " + verdict(earlierWait));

    for (const auto& [name, submissions, writer] : std::vector<std::tuple<std::string, std::vector<Planned>, std::string>>{
             {"other-queue-after", {raw(0, 1, rawWrite(A, 1)), raw(0x20, 2, rawWrite(A, 0)), raw(0, 3, rawWait(A, 1))}, "queue 0x20 at #1"},
             {"other-queue-before", {raw(0x20, 1, rawWrite(A, 0)), raw(0, 2, rawWrite(A, 1)), raw(0, 3, rawWait(A, 1))}, "queue 0x20 at #0"},
         }) {
        const auto analysis = analyzed(name, submissions);
        const auto& wait = waitAtAddress(analysis, A);
        check(wait.verdict == WaitReport::Verdict::Unknown && wait.crossQueue && wait.reason.starts_with("unknown: depends on cross-queue timing (stores on " + writer + ")"),
              name + ": a store of another queue to the awaited bytes, which can run between the store on the waiting queue and the wait whatever their submit order, leaves the wait other than unknown: " + verdict(wait));
    }

    const auto history = analyzed("history-other-queue", {raw(0, 1, joined(joined(rawWrite(A, 1), rawWait(B, 1)), rawWait(A, 1))), raw(0x20, 2, joined(repeated(AgcDriver::DriverDetail::LabelStoreHistory - 1, rawWrite(A, 0)), rawWrite(B, 1)))});
    const auto& historyWait = waitAtAddress(history, A);
    check(historyWait.verdict == WaitReport::Verdict::Released && historyWait.route == WaitReport::Route::LabelHistory && listed(historyWait, WriterMatch::Effect::LabelHistory, 0),
          "a label earlier in the waiting submission with fewer stores of all queues after it than the label history keeps does not release the wait: " + verdict(historyWait));
    check(waitAtAddress(history, B).verdict == WaitReport::Verdict::Unknown && waitAtAddress(history, B).crossQueue, "the wait on the store of another queue is not unknown: " + verdict(waitAtAddress(history, B)));

    Commands notKept;
    sceAgcDcbDmaData(notKept.Buffer(), 0, DmaMemory, 0, at(&syntheticLabel), DmaFill, 0, One, sizeof(syntheticLabel), 0, 1, 0);
    sceAgcDcbWriteData(notKept.Buffer(), GraphicsMemory, 0, at(&syntheticLabel), &Zero, 1, 0, 1);
    sceAgcDcbWaitRegMem(notKept.Buffer(), Bits32, Equal, 0, 0, &syntheticLabel, One, 0xffffffffu, 0);
    const auto fill = analyzed("fill-overwritten", {planned(0, 1, notKept)});
    const auto& fillWait = waitOn(fill, &syntheticLabel);
    check(fillWait.verdict == WaitReport::Verdict::Unknown && !fillWait.crossQueue && listed(fillWait, WriterMatch::Effect::Overwritten, 0) && fillWait.reason.find("is a RELEASE_MEM or WRITE_DATA label") != std::string::npos,
          "a DMA_DATA fill the label history does not keep, overwritten before the wait, does not leave the wait unknown: " + verdict(fillWait));
}

struct Step {
    std::uint32_t queue = 0;
    std::vector<std::uint32_t> commands;
};

using Outcome = std::tuple<WaitReport::Verdict, WaitReport::Route, bool>;
using Outcomes = std::map<std::pair<std::uint64_t, std::size_t>, Outcome>;

Outcomes outcomes(const AgcDriver::Pm4Analysis::Analysis& analysis) {
    Outcomes result;
    std::map<std::uint64_t, std::size_t> seen;
    for (const auto& report : analysis.waits) {
        const auto address = report.wait.fields.address;
        result.emplace(std::pair{address, seen[address]++}, Outcome{report.verdict, report.route, report.crossQueue});
    }
    return result;
}

std::vector<std::size_t> concatenatedOrder(const std::vector<std::vector<Step>>& groups, bool reversed) {
    std::vector<std::size_t> order;
    for (std::size_t i = 0; i < groups.size(); ++i) {
        const auto group = reversed ? groups.size() - 1 - i : i;
        order.insert(order.end(), groups[group].size(), group);
    }
    return order;
}

std::vector<std::size_t> roundRobinOrder(const std::vector<std::vector<Step>>& groups, std::size_t total, bool reversed) {
    std::vector<std::size_t> order;
    for (std::size_t step = 0; order.size() < total; ++step) {
        for (std::size_t i = 0; i < groups.size(); ++i) {
            const auto group = reversed ? groups.size() - 1 - i : i;
            if (step < groups[group].size()) order.push_back(group);
        }
    }
    return order;
}

std::vector<std::size_t> shuffledOrder(const std::vector<std::vector<Step>>& groups, std::size_t total, std::uint64_t& random) {
    std::vector<std::size_t> left;
    for (const auto& group : groups) left.push_back(group.size());
    std::vector<std::size_t> order;
    while (order.size() < total) {
        random = random * 6364136223846793005ull + 1442695040888963407ull;
        const auto group = static_cast<std::size_t>(random >> 33u) % groups.size();
        if (left[group] == 0) continue;
        --left[group];
        order.push_back(group);
    }
    return order;
}

void testPermutedSubmissions(const std::filesystem::path& directory) {
    constexpr std::uint64_t Base = 0x10001000;
    constexpr std::size_t Submissions = 15;
    constexpr std::size_t Shuffles = 4;
    constexpr std::size_t Full = AgcDriver::DriverDetail::LabelStoreHistory;
    const auto address = [](std::size_t group, std::size_t slot) { return Base + group * 0x100 + slot * 0x10; };
    const std::vector<std::vector<Step>> groups{
        {{0x20, rawWrite(address(0, 1), 1)}, {0x21, joined(joined(rawWrite(address(0, 0), 1), rawWait(address(0, 1), 1)), rawWrite(address(0, 0), 0))}, {0x20, rawWait(address(0, 0), 1)}},
        {{0x22, joined(rawWait(address(1, 0), 1), rawWrite(address(1, 1), 1))}, {0x23, joined(rawWait(address(1, 1), 1), rawWrite(address(1, 0), 1))}},
        {{0x24, rawWrite(address(2, 0), 1)}, {0x25, joined(rawWrite(address(2, 1), 5), rawWait(address(2, 1), 6))}, {0x24, joined(rawWait(address(2, 0), 1), rawWait(address(2, 2), 1))}},
        {{0x26, joined(joined(rawWrite(address(3, 0), 1), rawWrite(address(3, 0), 0)), rawWait(address(3, 0), 1))}, {0x27, rawWrite(address(3, 1), 3)}, {0x26, rawWait(address(3, 1), 4)}},
        {{0x28, joined(joined(rawWrite(address(4, 0), 1), rawWait(address(4, 1), 1)), rawWait(address(4, 0), 1))}, {0x29, joined(repeated(Full - 1, rawWrite(address(4, 0), 0)), rawWrite(address(4, 1), 1))},
         {0x2a, joined(joined(rawWrite(address(4, 2), 1), rawWait(address(4, 3), 1)), rawWait(address(4, 2), 1))}, {0x2b, joined(repeated(Full, rawWrite(address(4, 2), 0)), rawWrite(address(4, 3), 1))}},
    };
    using Verdict = WaitReport::Verdict;
    using Route = WaitReport::Route;
    const Outcome unknown{Verdict::Unknown, Route::None, true};
    const Outcome unreleased{Verdict::Unreleased, Route::None, false};
    const Outcomes expected{
        {{address(0, 1), 0}, unknown}, {{address(0, 0), 0}, unknown},
        {{address(1, 0), 0}, unknown}, {{address(1, 1), 0}, unknown},
        {{address(2, 0), 0}, {Verdict::Released, Route::CurrentValue, false}}, {{address(2, 1), 0}, unreleased}, {{address(2, 2), 0}, unreleased},
        {{address(3, 0), 0}, {Verdict::Released, Route::LabelHistory, false}}, {{address(3, 1), 0}, unreleased},
        {{address(4, 0), 0}, {Verdict::Released, Route::LabelHistory, false}}, {{address(4, 1), 0}, unknown}, {{address(4, 2), 0}, unknown}, {{address(4, 3), 0}, unknown},
    };
    std::vector<std::vector<std::size_t>> orders{concatenatedOrder(groups, false), concatenatedOrder(groups, true), roundRobinOrder(groups, Submissions, false), roundRobinOrder(groups, Submissions, true)};
    std::uint64_t random = 0x2545f4914f6cdd1dull;
    for (std::size_t shuffle = 0; shuffle < Shuffles; ++shuffle) orders.push_back(shuffledOrder(groups, Submissions, random));
    for (std::size_t permutation = 0; permutation < orders.size(); ++permutation) {
        check(orders[permutation].size() == Submissions, "a submission order does not hold the 15 submissions");
        std::vector<std::size_t> next(groups.size(), 0);
        std::vector<Planned> submissions;
        std::string described;
        for (const auto group : orders[permutation]) {
            const auto& step = groups[group][next[group]++];
            submissions.push_back(raw(step.queue, submissions.size() + 1, step.commands));
            described += std::to_string(group);
        }
        const auto path = directory / ("order-" + std::to_string(permutation));
        writeCapture(path, submissions);
        const auto result = outcomes(AgcDriver::Pm4Analysis::Analyze(path));
        check(result.size() == expected.size(), "the capture in submission order " + described + " does not hold the 13 waits");
        for (const auto& [key, outcome] : result) {
            check(expected.contains(key) && expected.at(key) == outcome, "submission order " + described + " gives the wait on " + hex(reinterpret_cast<const void*>(static_cast<std::uintptr_t>(key.first))) + " the verdict " + std::to_string(static_cast<int>(std::get<0>(outcome))) + " route " +
                                                                             std::to_string(static_cast<int>(std::get<1>(outcome))) + " cross-queue " + std::to_string(std::get<2>(outcome)));
        }
    }
}

template <typename TAction>
void expectFailure(TAction action, const std::string& text) {
    try {
        action();
    } catch (const std::runtime_error& error) {
        check(std::string(error.what()).find(text) != std::string::npos, "expected a failure naming '" + text + "', got: " + error.what());
        return;
    }
    throw std::runtime_error("expected a failure naming '" + text + "'");
}

void testFailedWrite(const std::filesystem::path& directory) {
    const auto blocker = directory / "submission_000006.bin";
    std::filesystem::create_directory(blocker);
    Commands store;
    sceAgcDcbWriteData(store.Buffer(), GraphicsMemory, 0, at(&otherLabel), &One, 1, 0, 1);
    expectFailure([&] { store.Submit(0); }, "cannot write");
    expectFailure([&] { store.Submit(0); }, "cannot write");
    std::filesystem::remove(blocker);
    check(lines(directory / SubmissionCapture::IndexName).size() == 8, "the index lists a submission whose file was not written");
}

void testHeldAtSubmit(const std::filesystem::path& directory) {
    Commands waitHeld;
    const auto wait = waitAt(waitHeld, sceAgcDcbWaitRegMem(waitHeld.Buffer(), Bits32, Equal, 0, 0, &syntheticLabel, One, 0xffffffffu, 0));
    writeCapture(directory, {planned(0, 1, waitHeld, {wait})});
    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    const auto& held = waitOn(analysis, &syntheticLabel);
    check(held.verdict == WaitReport::Verdict::Released && held.route == WaitReport::Route::HeldAtSubmit, "a wait whose value held at submit is not released: " + verdict(held));
    check(report(directory).first == 0, "the report fails on a wait released at submit");
}

void testRewindValidWithoutTail(const std::filesystem::path& directory) {
    Commands rewound;
    sceAgcAcbWriteData(rewound.Buffer(), ComputeMemory, 0, at(&beforeRewind), &BeforeRewindValue, 1, 0, 1);
    auto* rewind = sceAgcAcbRewind(rewound.Buffer(), 0);
    sceAgcAsyncRewindPatchSetRewindState(rewind, 1);
    writeCapture(directory, {planned(0x20, 1, rewound)});
    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    const auto rewindWait = std::find_if(analysis.waits.begin(), analysis.waits.end(), [](const auto& report) { return report.wait.kind == AgcDriver::Pm4Analysis::MemoryWait::Kind::Rewind; });
    check(rewindWait != analysis.waits.end(), "the capture of a REWIND valid at submit lists no REWIND");
    check(rewindWait->verdict == WaitReport::Verdict::Unknown && rewindWait->reason.find("reads it again") != std::string::npos,
          "a REWIND valid at submit without a captured tail is not unknown: " + verdict(*rewindWait));
}

void replaceField(const std::filesystem::path& directory, std::size_t field, const std::string& value) {
    auto index = lines(directory / SubmissionCapture::IndexName);
    check(index.size() == 3, "the capture to corrupt does not hold one submission");
    auto values = fields(index[2]);
    values.at(field) = value;
    std::ofstream output(directory / SubmissionCapture::IndexName, std::ios::binary | std::ios::trunc);
    output << index[0] << '\n' << index[1] << '\n';
    for (std::size_t i = 0; i < values.size(); ++i) output << (i == 0 ? "" : " ") << values[i];
    output << '\n';
    check(output.good(), "cannot rewrite the index to corrupt");
}

void dropHeldColumns(const std::filesystem::path& directory) {
    const auto index = lines(directory / SubmissionCapture::IndexName);
    std::ofstream output(directory / SubmissionCapture::IndexName, std::ios::binary | std::ios::trunc);
    output << index.at(0) << '\n' << SubmissionCapture::ColumnsWithoutHeld << '\n';
    for (std::size_t line = 2; line < index.size(); ++line) {
        const auto values = fields(index[line]);
        check(values.size() == 10, "the index line to rewrite does not have 10 fields: " + index[line]);
        for (std::size_t i = 0; i < 8; ++i) output << (i == 0 ? "" : " ") << values[i];
        output << '\n';
    }
    check(output.good(), "cannot rewrite the index without its held and conditional columns");
}

void testIndexWithoutHeld(const std::filesystem::path& directory) {
    Commands graphics, compute;
    sceAgcDcbWaitRegMem(graphics.Buffer(), Bits32, Equal, 0, 0, &cpuLabel, CpuValue, 0xffffffffu, 0);
    sceAgcDcbWriteData(graphics.Buffer(), GraphicsMemory, 0, at(&otherLabel), &One, 1, 0, 1);
    sceAgcDcbCondExec(graphics.Buffer(), &conditionLabel, WriteDataWords);
    sceAgcDcbWriteData(graphics.Buffer(), GraphicsMemory, 0, at(&guardedLabel), &One, 1, 0, 1);
    sceAgcDcbWaitRegMem(graphics.Buffer(), Bits32, Equal, 0, 0, &guardedLabel, One, 0xffffffffu, 0);
    sceAgcAcbWaitRegMem(compute.Buffer(), Bits32, Equal, 0, &otherLabel, One, 0xffffffffu, 0);
    writeCapture(directory, {planned(0, 1, graphics), planned(0x20, 2, compute)});
    dropHeldColumns(directory);
    const auto captured = SubmissionCapture::ReadIndex(directory);
    check(captured.size() == 2 && !captured[0].heldKnown && !captured[0].conditionalKnown, "an index without the held and conditional columns is read as if it had them");
    const auto analysis = AgcDriver::Pm4Analysis::Analyze(directory);
    check(analysis.waits.size() == 3, "expected three waits in the capture without held waits");
    const auto& cpu = waitOn(analysis, &cpuLabel);
    check(cpu.verdict == WaitReport::Verdict::Unknown && !cpu.crossQueue && cpu.reason.starts_with("unknown: the index has no held column"), "a wait no store releases is not unknown when the index does not say whether it was held at submit: " + verdict(cpu));
    const auto& behind = waitOn(analysis, &otherLabel);
    check(behind.verdict == WaitReport::Verdict::Unknown && behind.crossQueue, "a store of another queue decides a wait offline: " + verdict(behind));
    const auto& guarded = waitOn(analysis, &guardedLabel);
    check(guarded.verdict == WaitReport::Verdict::Unknown && !guarded.crossQueue && guarded.wait.conditional && guarded.reason.find("is conditional") != std::string::npos, "a store after a COND_EXEC whose range the index does not hold is not conditional: " + verdict(guarded));
    const auto [code, text] = report(directory);
    check(code == 0 && text.find("unknown 3 (cross-queue timing 1), unreleased 0\n") != std::string::npos && text.find("the index has no held and conditional columns") != std::string::npos && text.find("UNKNOWN #0 ") != std::string::npos,
          "the report does not show the waits of unknown outcome:\n" + text);
}

void testCorrupt(const std::filesystem::path& directory) {
    constexpr std::size_t HeldField = 8;
    constexpr std::size_t ConditionalField = 9;
    Commands waitOnly;
    sceAgcDcbWaitRegMem(waitOnly.Buffer(), Bits32, Equal, 0, 0, &syntheticLabel, One, 0xffffffffu, 0);
    const auto expectThrow = [&](const std::vector<Planned>& submissions, std::size_t field, const std::string& value, const std::string& what) {
        std::filesystem::remove_all(directory);
        writeCapture(directory, submissions);
        if (!value.empty()) replaceField(directory, field, value);
        bool threw = false;
        try {
            AgcDriver::Pm4Analysis::Analyze(directory);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        check(threw, "the analysis accepts " + what);
    };
    expectThrow({{0, 1, SubmissionKind::Submit, {0xc0044700u, 0x00000528u, static_cast<std::uint32_t>(at(&syntheticLabel)), 0x40000000u, 1, 0}, {}, {}}}, 0, {}, "an EVENT_WRITE_EOP the driver rejects");
    expectThrow({planned(0, 1, waitOnly)}, HeldField, "0", "a wait held at submit that marks no wait packet");
    expectThrow({planned(0, 1, waitOnly)}, ConditionalField, "4:11", "a conditional range that starts at no COND_EXEC");
}

}

int main() {
    std::filesystem::path scratch;
    try {
#ifdef _WIN32
        const wchar_t* captured = _wgetenv(L"APS5_DUMP_SUBMITS");
#else
        const char* captured = std::getenv("APS5_DUMP_SUBMITS");
#endif
        check(captured != nullptr && *captured != 0, "set APS5_DUMP_SUBMITS to the capture directory of this test");
        const std::filesystem::path directory = captured;
        std::filesystem::remove_all(directory);
        scratch = std::filesystem::temp_directory_path() / ("anyps5-pm4-replay-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        testDriverCapture(directory);
        testWriterPresent(directory, scratch / "writer-present");
        testOverwrittenAcrossSubmissions(scratch / "overwritten");
        testLabelHistory(scratch / "history-kept", AgcDriver::DriverDetail::LabelStoreHistory - 1);
        testLabelHistory(scratch / "history-evicted", AgcDriver::DriverDetail::LabelStoreHistory);
        testUnreleasable(scratch / "unreleasable");
        testReviewerCaptures(scratch / "reviewed");
        testProvableRoutes(scratch / "routes");
        testPermutedSubmissions(scratch / "permuted");
        testHeldAtSubmit(scratch / "held");
        testRewindValidWithoutTail(scratch / "rewind-valid");
        testIndexWithoutHeld(scratch / "without-held");
        testCorrupt(scratch / "corrupt");
        std::filesystem::remove_all(scratch);
        testFailedWrite(directory);
        expectFailure([] { LibcRunShutdown_nid_postfix(); }, "cannot write");
        std::puts("PM4 submission capture, decode and wait release tests passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "%s\n", error.what());
        std::error_code ignored;
        if (!scratch.empty()) std::filesystem::remove_all(scratch, ignored);
        try {
            LibcRunShutdown_nid_postfix();
        } catch (const std::exception& shutdown) {
            std::fprintf(stderr, "shutdown: %s\n", shutdown.what());
        }
        return 1;
    }
}
