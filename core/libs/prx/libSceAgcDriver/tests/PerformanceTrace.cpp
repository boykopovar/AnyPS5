#include "prx/libSceAgcDriver/Execution/include/PerformanceTrace.hpp"
#include "prx/libSceAgcDriver/Execution/include/TraceFile.hpp"
#include "zlib.h"
#include <atomic>
#include <condition_variable>
#include <fstream>
#include <iostream>
#include <iterator>
#include <mutex>

namespace {

using AgcDriver::TraceFormat;
using AgcDriver::TraceThread;
using AgcDriver::TraceWindow;
using Clock = std::chrono::steady_clock;
using Edge = TraceWindow::Edge;
using std::chrono::microseconds;
using std::chrono::milliseconds;

void Check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

void WindowByFrames() {
    auto window = TraceWindow::Parse("3:2", nullptr).value();
    std::vector<Edge> edges;
    for (std::uint64_t present = 0; present < 8; ++present) edges.push_back(window.Present(present, std::chrono::seconds(present)));
    Check(edges == std::vector<Edge>{Edge::None, Edge::None, Edge::None, Edge::Open, Edge::None, Edge::Close, Edge::None, Edge::None}, "frame window opened or closed at the wrong present");
    Check(window.Describe() == "frames 3:2", "frame window description changed");
}

void WindowBySeconds() {
    auto window = TraceWindow::Parse(nullptr, "2:1").value();
    std::vector<Edge> edges;
    for (const auto elapsed : {milliseconds(0), milliseconds(1999), milliseconds(2000), milliseconds(2999), milliseconds(3000), milliseconds(5000)})
        edges.push_back(window.Present(0, elapsed));
    Check(edges == std::vector<Edge>{Edge::None, Edge::None, Edge::Open, Edge::None, Edge::Close, Edge::None}, "seconds window opened or closed at the wrong present");
    Check(window.Describe() == "seconds 2:1", "seconds window description changed");
}

void WindowRejects() {
    Check(!TraceWindow::Parse(nullptr, nullptr).has_value(), "a window was made without a switch");
    for (const auto* text : {"5", "a:1", "1:0", "1:", ":1", "1:2:3", "-1:2"}) {
        bool rejected = false;
        try { TraceWindow::Parse(text, nullptr); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "a malformed window was accepted");
    }
    bool rejected = false;
    try { TraceWindow::Parse("1:1", "1:1"); }
    catch (const std::invalid_argument&) { rejected = true; }
    Check(rejected, "frames and seconds were accepted together");
}

void BufferCap() {
    std::atomic<std::uint64_t> presents{1};
    TraceThread thread("queue worker 0x0", presents, 2);
    const auto origin = Clock::now();
    for (int index = 0; index < 3; ++index) thread.Add("Driver.Packet", nullptr, origin + milliseconds(index), origin + milliseconds(index) + microseconds(30));
    const auto snapshot = thread.Collect();
    Check(snapshot.slices.size() == 2 && snapshot.dropped, "a full buffer did not stop and set the dropped flag");
    Check(snapshot.eventsLost == 1 && snapshot.eventsFullAt == origin + milliseconds(2) && !snapshot.totalsFull, "a full buffer did not report the lost slice and its time apart from the totals");
    Check(snapshot.totals.size() == 1 && snapshot.totals[0].count == 3 && snapshot.totals[0].length == microseconds(90), "totals missed a slice past the cap");
}

void LostEventsCount() {
    std::atomic<std::uint64_t> presents{1};
    TraceThread thread("queue worker 0x0", presents, 2);
    const auto origin = Clock::now();
    Check(!thread.Collect().eventsFullAt && thread.Collect().eventsLost == 0, "a thread that lost nothing reported a loss");
    for (int index = 0; index < 10; ++index) thread.Add("Driver.Packet", nullptr, origin + milliseconds(index), origin + milliseconds(index) + microseconds(30));
    const auto snapshot = thread.Collect();
    Check(snapshot.eventsLost == 8 && snapshot.eventsFullAt == origin + milliseconds(2), "the lost slices were not counted from the first one");
}

void TotalsOverflow() {
    static char scopes[600];
    std::atomic<std::uint64_t> presents{1};
    TraceThread thread("presenter", presents, 4096);
    const auto origin = Clock::now();
    for (const auto& scope : scopes) thread.Add(&scope, nullptr, origin, origin + microseconds(30));
    const auto snapshot = thread.Collect();
    Check(snapshot.totalsFull && snapshot.dropped && snapshot.eventsLost == 0 && !snapshot.eventsFullAt, "a full totals table was not reported apart from the event buffer");
    Check(snapshot.totals.size() == 512 && snapshot.slices.size() == 600, "a full totals table stopped the event buffer or kept more than its entries");
}

void CapacitySettings() {
    Check(TraceThread::ParseCapacity(nullptr) == TraceThread::DefaultCapacity, "the default event capacity changed");
    Check(TraceThread::ParseCapacity("1") == 1, "the smallest event capacity was rejected");
    Check(TraceThread::ParseCapacity("3145728") == 3145728, "a larger event capacity was rejected");
    Check(TraceThread::ParseCapacity("4194304") == TraceThread::MaximumCapacity, "the maximum event capacity was rejected");
    std::atomic<std::uint64_t> presents{1};
    const auto requested = std::to_string(TraceThread::DefaultCapacity + 1);
    TraceThread thread("queue worker 0x0", presents, TraceThread::ParseCapacity(requested.c_str()));
    const auto origin = Clock::now();
    for (std::size_t index = 0; index <= TraceThread::DefaultCapacity; ++index)
        thread.Add("Driver.Packet", nullptr, origin, origin + microseconds(30));
    auto snapshot = thread.Collect();
    Check(snapshot.slices.size() == TraceThread::DefaultCapacity + 1 && !snapshot.dropped, "the configured buffer stopped at the default capacity");
    thread.Add("Driver.Packet", nullptr, origin, origin + microseconds(30));
    snapshot = thread.Collect();
    Check(snapshot.slices.size() == TraceThread::DefaultCapacity + 1 && snapshot.dropped, "the configured buffer grew beyond its capacity");
    Check(snapshot.totals.size() == 1 && snapshot.totals[0].count == TraceThread::DefaultCapacity + 2, "totals lost the event past the configured capacity");
    for (const auto* text : {"", "0", "-1", "+1", "1.5", " 1", "1 ", "1x", "4194305", "18446744073709551616"}) {
        bool rejected = false;
        try { TraceThread::ParseCapacity(text); }
        catch (const std::invalid_argument& error) { rejected = std::string_view(error.what()).starts_with("APS5_TRACE_EVENTS="); }
        Check(rejected, "an invalid event capacity did not report its setting");
    }
}

void FoldShortSlices() {
    std::atomic<std::uint64_t> presents{1};
    TraceThread thread("queue worker 0x0", presents, 16);
    const auto origin = Clock::now();
    thread.Add("Driver.Packet", nullptr, origin, origin + microseconds(5));
    thread.Add("Driver.Packet", "draw", origin + microseconds(5), origin + microseconds(10));
    thread.Add("Driver.Draw", nullptr, origin + microseconds(10), origin + microseconds(40));
    presents.store(2);
    thread.Add("Driver.Completion", nullptr, origin + microseconds(50), origin + microseconds(90));
    const auto snapshot = thread.Collect();
    Check(snapshot.slices.size() == 3 && !snapshot.dropped, "short slices were written or long ones lost");
    Check(std::string_view(snapshot.slices[0].scope) == "Driver.Draw", "the first long slice is missing");
    Check(snapshot.slices[1].scope == nullptr && snapshot.slices[1].begin == origin && snapshot.slices[1].length == microseconds(10), "the folded sample of the previous present is wrong");
    Check(std::string_view(snapshot.slices[2].scope) == "Driver.Completion", "the slice after the present is missing");
    Check(snapshot.totals.size() == 4, "totals did not keep scope and stage apart");
    for (const auto& total : snapshot.totals) Check(total.count == 1, "a total counted a slice twice");
}

void EscapeStrings() {
    std::string output;
    TraceFormat::AppendString(output, "a\"b\\c\n\xe9");
    Check(output == "\"a\\\"b\\\\c\\u000a\\u00e9\"", "string escaping is not valid JSON");
}

void Utf8Strings() {
    std::string output;
    TraceFormat::AppendString(output, "Ghost of Y\xc5\x8dtei \xe2\x84\xa2 \xf0\x9f\x8e\xae");
    Check(output == "\"Ghost of Y\xc5\x8dtei \xe2\x84\xa2 \xf0\x9f\x8e\xae\"", "a UTF-8 title was not written as its own characters");
    output.clear();
    TraceFormat::AppendString(output, "\xc0\xaf\xed\xa0\x80\xf4\x90\x80\x80\xe2\x84\x7f\xc5");
    Check(output == "\"\\u00c0\\u00af\\u00ed\\u00a0\\u0080\\u00f4\\u0090\\u0080\\u0080\\u00e2\\u0084\\u007f\\u00c5\"", "a byte outside a valid UTF-8 sequence was not escaped");
    AgcDriver::TraceSummary summary;
    summary.title = AgcDriver::TraceTitle{"PPSA26344", "Ghost of Y\xc5\x8dtei", "01.000.000"};
    const auto origin = Clock::now();
    const std::vector<Clock::time_point> presents{origin, origin + milliseconds(16)};
    Check(TraceFormat::Summary(summary, {}, presents).find("\"title\":\"Ghost of Y\xc5\x8dtei\"") != std::string::npos, "the summary line did not keep a UTF-8 title");
}

void EventsShape() {
    const auto origin = Clock::now();
    const std::vector<TraceThread::Slice> slices{
        {"Driver.Packet", nullptr, origin + microseconds(10), microseconds(30)},
        {"Driver.Packet", "draw", origin + microseconds(12), microseconds(25)},
        {nullptr, nullptr, origin, milliseconds(2)},
    };
    const std::vector<TraceThread::Snapshot> threads{{"queue worker 0x0", slices, {}, false}};
    const std::vector<Clock::time_point> presents{origin, origin + milliseconds(16)};
    const auto json = TraceFormat::Events(7, origin, threads, 0, presents, 5);
    const std::string expected =
        "{\"traceEvents\":[\n"
        "{\"name\":\"process_name\",\"ph\":\"M\",\"ts\":0.000,\"pid\":7,\"tid\":0,\"args\":{\"name\":\"AnyPS5\"}},\n"
        "{\"name\":\"thread_name\",\"ph\":\"M\",\"ts\":0.000,\"pid\":7,\"tid\":1,\"args\":{\"name\":\"queue worker 0x0\"}},\n"
        "{\"name\":\"queue worker 0x0 folded\",\"ph\":\"C\",\"ts\":0.000,\"pid\":7,\"tid\":1,\"args\":{\"ms\":2.000}},\n"
        "{\"name\":\"Driver.Packet\",\"ph\":\"X\",\"ts\":10.000,\"pid\":7,\"tid\":1,\"dur\":30.000},\n"
        "{\"name\":\"Driver.Packet.draw\",\"ph\":\"X\",\"ts\":12.000,\"pid\":7,\"tid\":1,\"dur\":25.000},\n"
        "{\"name\":\"present\",\"ph\":\"i\",\"ts\":0.000,\"pid\":7,\"tid\":1,\"s\":\"g\",\"args\":{\"present\":5}}\n"
        "]}\n";
    Check(json == expected, "trace events changed shape");
}

void BatchIds() {
    const auto origin = Clock::now();
    const std::vector<TraceThread::Slice> slices{{"Driver.Packet", "dispatch", origin + microseconds(10), microseconds(30), 42}};
    const std::vector<TraceThread::Snapshot> threads{{"queue worker 0x0", slices, {}, false}};
    const std::vector<Clock::time_point> presents{origin, origin + milliseconds(16)};
    const auto json = TraceFormat::Events(7, origin, threads, 0, presents, 5);
    Check(json.find("{\"name\":\"Driver.Packet.dispatch\",\"ph\":\"X\",\"ts\":10.000,\"pid\":7,\"tid\":1,\"dur\":30.000,\"args\":{\"batch\":42}}") != std::string::npos, "a slice's batch id was not written");
    std::atomic<std::uint64_t> count{1};
    TraceThread thread("queue worker 0x0", count, 4);
    AgcDriver::TraceBatchTag = 9;
    thread.Add("Driver.Packet", nullptr, origin, origin + microseconds(30));
    AgcDriver::TraceBatchTag = 0;
    thread.Add("Driver.Packet", nullptr, origin + milliseconds(1), origin + milliseconds(1) + microseconds(30));
    const auto snapshot = thread.Collect();
    Check(snapshot.slices.size() == 2 && snapshot.slices[0].batch == 9 && snapshot.slices[1].batch == 0, "a slice did not take its thread's batch id");
    const AgcDriver::TraceClockSample first{1000.0, origin + microseconds(10)};
    const AgcDriver::TraceClockSample last{41000.0, origin + microseconds(30)};
    const std::vector<AgcDriver::TraceGpuRange> ranges{{"batch", 5000.0, 9000.0, 3}};
    Check(TraceFormat::GpuSlices(ranges, first, last, origin)[0].batch == 3, "a GPU range lost its batch id");
}

void FullBufferEvents() {
    const auto origin = Clock::now();
    const std::vector<TraceThread::Slice> slices{{"Driver.Packet", nullptr, origin + milliseconds(1), microseconds(30)}};
    TraceThread::Snapshot snapshot{"queue worker 0x0", slices, {}, true, 7, origin + milliseconds(3), false};
    const std::vector<TraceThread::Snapshot> threads{snapshot};
    const std::vector<Clock::time_point> presents{origin, origin + milliseconds(16)};
    const auto json = TraceFormat::Events(7, origin, threads, 0, presents, 5);
    const std::string expected =
        "{\"name\":\"trace buffer full\",\"ph\":\"i\",\"ts\":3000.000,\"pid\":7,\"tid\":1,\"s\":\"t\",\"args\":{\"events_lost\":7}},\n"
        "{\"name\":\"present\"";
    Check(json.find(expected) != std::string::npos, "the cut-off instant event is missing or has the wrong shape");
    snapshot.eventsLost = 0;
    snapshot.eventsFullAt.reset();
    Check(TraceFormat::Events(7, origin, std::vector<TraceThread::Snapshot>{snapshot}, 0, presents, 5).find("trace buffer full") == std::string::npos, "a thread that lost nothing got a cut-off event");
}

void SummaryFields() {
    const auto origin = Clock::now();
    AgcDriver::TraceSummary summary;
    summary.commit = "abc";
    summary.dirty = false;
    summary.environment = {{"APS5_TRACE_FRAMES", "3:2"}};
    summary.gpu = "GPU";
    summary.driver = "Driver 1";
    summary.os = "OS 1";
    summary.width = 1920;
    summary.height = 1080;
    summary.window = "frames 3:2";
    summary.firstPresent = 3;
    summary.gpuBusyMs = 20;
    summary.trace = "aps5-trace-abc-7.json.gz";
    const std::vector<TraceThread::Snapshot> threads{
        {"queue worker 0x0", {}, {{"Driver.Packet", nullptr, milliseconds(12), 4}, {"Driver.WorkerWait", nullptr, milliseconds(8), 4}, {"Driver.Completion", nullptr, milliseconds(2), 4}, {"Driver.Packet", "draw", milliseconds(10), 2}}, false},
        {"presenter", {}, {{"Driver.Present", nullptr, milliseconds(6), 2}, {"Driver.Present", "inflight_wait", milliseconds(4), 2}}, false},
    };
    const std::vector<Clock::time_point> presents{origin, origin + milliseconds(16), origin + milliseconds(40)};
    const auto line = TraceFormat::Summary(summary, threads, presents);
    const std::string expected =
        "{\"commit\":\"abc\",\"dirty\":false,\"env\":{\"APS5_TRACE_FRAMES\":\"3:2\"},\"gpu\":\"GPU\",\"driver\":\"Driver 1\",\"os\":\"OS 1\","
        "\"resolution\":\"1920x1080\",\"window\":{\"spec\":\"frames 3:2\",\"first_present\":3,\"ms\":40.000},\"presents\":2,"
        "\"frame_ms\":{\"p50\":24.000,\"p90\":24.000,\"p99\":24.000,\"max\":24.000},\"gpu_busy_ms_per_present\":10.000,"
        "\"worker_busy_pct\":{\"queue worker 0x0\":35.000},"
        "\"top_phases_ms_per_frame\":{\"Driver.Packet\":6.000,\"Driver.Packet.draw\":5.000,\"Driver.Present\":3.000,\"Driver.Present.inflight_wait\":2.000,\"Driver.Completion\":1.000},"
        "\"trace_dropped\":false,\"trace\":\"aps5-trace-abc-7.json.gz\"}\n";
    Check(line == expected, "summary line changed");
    auto dropped = threads;
    dropped[1].dropped = true;
    Check(TraceFormat::Summary(summary, dropped, presents).find("\"trace_dropped\":true") != std::string::npos, "a dropped thread did not mark the summary");
    Check(line.find("trace_dropped_threads") == std::string::npos, "a summary without drops listed threads");
    dropped[0].dropped = true;
    dropped[0].eventsLost = 2240000;
    dropped[0].eventsFullAt = presents.front() + milliseconds(32);
    dropped[1].totalsFull = true;
    const auto detailed = TraceFormat::Summary(summary, dropped, presents);
    Check(detailed.find("\"trace_dropped\":true,\"trace_dropped_threads\":{\"queue worker 0x0\":{\"events_full_after_s\":0.032,\"events_lost\":2240000,\"totals_full\":false},\"presenter\":{\"totals_full\":true}},\"trace\":") != std::string::npos, "the dropped threads were not listed with their reasons");
    auto shared = threads;
    shared.push_back({"shader prepare", {}, {}, true});
    shared.push_back({"shader prepare", {}, {}, true});
    shared[2].eventsLost = 5;
    shared[2].eventsFullAt = presents.front() + milliseconds(30);
    shared[3].eventsLost = 7;
    shared[3].eventsFullAt = presents.front() + milliseconds(10);
    shared[3].totalsFull = true;
    const auto merged = TraceFormat::Summary(summary, shared, presents);
    Check(merged.find("\"trace_dropped_threads\":{\"shader prepare\":{\"events_full_after_s\":0.010,\"events_lost\":12,\"totals_full\":true}},\"trace\":") != std::string::npos, "threads with one name were not merged into one entry");
    Check(merged.find("shader prepare") == merged.rfind("shader prepare"), "threads with one name were listed twice");
}

void GpuClock() {
    const auto origin = Clock::now();
    const AgcDriver::TraceClockSample first{1000.0, origin + microseconds(10)};
    const AgcDriver::TraceClockSample last{41000.0, origin + microseconds(30)};
    const std::vector<AgcDriver::TraceGpuRange> ranges{{"batch", 5000.0, 9000.0}, {"draw", 6000.0, 7000.0}, {"batch", -39000.0, -30000.0}, {"copy", 8000.0, 8000.0}};
    const auto slices = TraceFormat::GpuSlices(ranges, first, last, origin);
    Check(slices.size() == 2, "a GPU range before the window or without length was kept");
    Check(std::string_view(slices[0].scope) == "batch" && slices[0].stage == nullptr && slices[0].begin == origin + microseconds(12) && slices[0].length == microseconds(2), "a GPU range was not mapped between the two calibrations");
    Check(std::string_view(slices[1].scope) == "draw" && slices[1].begin == origin + std::chrono::nanoseconds(12500) && slices[1].length == std::chrono::nanoseconds(500), "a nested GPU range was not mapped");
    const AgcDriver::TraceClockSample same{1000.0, origin + microseconds(10)};
    Check(TraceFormat::GpuSlices(ranges, same, same, origin)[0].begin == origin + microseconds(14), "one calibration does not map at the CPU rate");
}

void IntervalDue() {
    Check(!AgcDriver::TraceInterval::Parse(nullptr).has_value(), "an interval was made without a switch");
    for (const auto* text : {"", "0", "-5", "1.5", "a", "5s", "1:2", "99999999999"}) {
        bool rejected = false;
        try { AgcDriver::TraceInterval::Parse(text); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "a malformed interval was accepted");
    }
    auto interval = AgcDriver::TraceInterval::Parse("30").value();
    Check(interval.Describe() == "summary_every 30", "interval description changed");
    Check(!interval.Due(milliseconds(29999)) && interval.Due(milliseconds(30000)), "the interval was due at the wrong time");
    interval.Restart(milliseconds(30500));
    Check(interval.Begun() == milliseconds(30500) && !interval.Due(milliseconds(60499)) && interval.Due(milliseconds(60500)), "a restarted interval did not count from its restart");
}

void SummaryOnlyThread() {
    std::atomic<std::uint64_t> presents{1};
    std::atomic<bool> buffering{false};
    TraceThread thread("queue worker 0x0", presents, 0, buffering);
    const auto origin = Clock::now();
    thread.Add("Driver.Packet", nullptr, origin, origin + milliseconds(2));
    thread.Add("Driver.Packet", nullptr, origin + milliseconds(3), origin + milliseconds(4));
    presents.store(2);
    thread.Add("Driver.Completion", nullptr, origin + milliseconds(5), origin + microseconds(5005));
    auto snapshot = thread.Collect();
    Check(snapshot.slices.empty() && !snapshot.dropped, "a thread that is not buffering kept a slice");
    Check(snapshot.totals.size() == 2 && snapshot.totals[0].count + snapshot.totals[1].count == 3, "a thread that is not buffering missed a total");
    buffering.store(true);
    thread.Add("Driver.Packet", nullptr, origin + milliseconds(6), origin + milliseconds(7));
    Check(thread.Collect().slices.empty(), "a zero capacity buffer held a slice");
}

void TotalsSince() {
    const std::vector<TraceThread::Total> before{{"Driver.Packet", nullptr, milliseconds(4), 2}, {"Driver.Present", nullptr, milliseconds(1), 1}};
    const std::vector<TraceThread::Total> now{{"Driver.Packet", nullptr, milliseconds(10), 5}, {"Driver.Present", nullptr, milliseconds(1), 1}, {"Driver.Completion", nullptr, milliseconds(3), 2}};
    const auto delta = TraceFormat::Since(now, before);
    Check(delta.size() == 2, "a total without a new slice was kept or a new one lost");
    Check(delta[0].length == milliseconds(6) && delta[0].count == 3, "a total was not reduced by the previous interval");
    Check(std::string_view(delta[1].scope) == "Driver.Completion" && delta[1].length == milliseconds(3) && delta[1].count == 2, "a total without a previous value changed");
    Check(TraceFormat::Since(now, {}).size() == 3, "the first interval dropped a total");
}

void IntervalSummaryFields() {
    const auto origin = Clock::now();
    AgcDriver::TraceSummary summary;
    summary.commit = "abc";
    summary.dirty = true;
    summary.environment = {{"APS5_TRACE_SUMMARY_EVERY", "30"}};
    summary.gpu = "GPU";
    summary.driver = "Driver 1";
    summary.os = "OS 1";
    summary.width = 1920;
    summary.height = 1080;
    summary.window = "summary_every 30";
    summary.firstPresent = 120;
    summary.gpuBusyMs = 20;
    summary.interval = std::make_pair(30.5, 60.25);
    const std::vector<TraceThread::Snapshot> threads{
        {"queue worker 0x0", {}, {{"Driver.Packet", nullptr, milliseconds(12), 4}}, false},
    };
    const std::vector<Clock::time_point> presents{origin, origin + milliseconds(16), origin + milliseconds(40)};
    const auto line = TraceFormat::Summary(summary, threads, presents);
    const std::string expected =
        "{\"commit\":\"abc\",\"dirty\":true,\"env\":{\"APS5_TRACE_SUMMARY_EVERY\":\"30\"},\"gpu\":\"GPU\",\"driver\":\"Driver 1\",\"os\":\"OS 1\","
        "\"resolution\":\"1920x1080\",\"window\":{\"spec\":\"summary_every 30\",\"first_present\":120,\"ms\":40.000},\"interval_start_s\":30.500,\"interval_end_s\":60.250,\"presents\":2,"
        "\"frame_ms\":{\"p50\":24.000,\"p90\":24.000,\"p99\":24.000,\"max\":24.000},\"gpu_busy_ms_per_present\":10.000,"
        "\"worker_busy_pct\":{\"queue worker 0x0\":30.000},"
        "\"top_phases_ms_per_frame\":{\"Driver.Packet\":6.000},"
        "\"trace_dropped\":false,\"trace\":\"\"}\n";
    Check(line == expected, "interval summary line changed");
}

void IntervalDroppedThreads() {
    const auto origin = Clock::now();
    AgcDriver::TraceSummary summary;
    summary.interval = std::make_pair(30.0, 60.0);
    std::vector<TraceThread::Snapshot> threads{{"queue worker 0x0", {}, {}, true, 5}};
    const std::vector<Clock::time_point> presents{origin + milliseconds(100), origin + milliseconds(116)};
    threads[0].eventsFullAt = origin + milliseconds(40);
    auto line = TraceFormat::Summary(summary, threads, presents);
    Check(line.find("\"queue worker 0x0\":{\"events_lost\":5,\"totals_full\":false}") != std::string::npos, "a loss before the interval printed its time");
    threads[0].eventsFullAt = origin + milliseconds(104);
    line = TraceFormat::Summary(summary, threads, presents);
    Check(line.find("{\"events_full_after_s\":0.004,\"events_lost\":5,") != std::string::npos, "a loss inside the interval lost its time");
    threads[0].eventsFullAt = origin + milliseconds(120);
    line = TraceFormat::Summary(summary, threads, presents);
    Check(line.find("\"queue worker 0x0\":{\"events_lost\":5,\"totals_full\":false}") != std::string::npos && line.find("events_full_after_s") == std::string::npos, "a loss after the last present printed its time");
    line = TraceFormat::Summary(summary, threads, {});
    Check(line.find("\"queue worker 0x0\":{\"events_lost\":5,\"totals_full\":false}") != std::string::npos && line.find("events_full_after_s") == std::string::npos, "an interval without presents printed a loss time");
}

void SummaryMachineFields() {
    const auto origin = Clock::now();
    AgcDriver::TraceSummary summary;
    summary.commit = "abc";
    summary.dirty = false;
    summary.gpu = "GPU";
    summary.driver = "Driver 1";
    summary.os = "OS 1";
    summary.width = 1920;
    summary.height = 1080;
    summary.window = "summary_every 30";
    summary.host = AgcDriver::TraceHost{"CPU \"X\"", 16, 32000};
    summary.title = AgcDriver::TraceTitle{"PPSA00000", "Example", "01.002.000"};
    const std::vector<Clock::time_point> presents{origin, origin + milliseconds(16)};
    const auto line = TraceFormat::Summary(summary, {}, presents);
    Check(line.find("\"os\":\"OS 1\",\"cpu\":\"CPU \\\"X\\\"\",\"cpu_threads\":16,\"memory_mib\":32000,\"title_id\":\"PPSA00000\",\"title\":\"Example\",\"content_version\":\"01.002.000\",\"resolution\":\"1920x1080\"") != std::string::npos, "the summary line did not name the machine and title");
    summary.title.reset();
    Check(TraceFormat::Summary(summary, {}, presents).find("title_id") == std::string::npos, "a summary without param.json named a title");
    summary.host.reset();
    const auto bare = TraceFormat::Summary(summary, {}, presents);
    Check(bare.find("\"os\":\"OS 1\",\"resolution\":\"1920x1080\"") != std::string::npos && bare.find("cpu") == std::string::npos, "a summary on a host without a CPU name changed its fields");
}

void FrameMicroseconds() {
    const auto origin = Clock::now();
    const std::vector<Clock::time_point> presents{origin, origin + microseconds(16667), origin + microseconds(50000), origin + microseconds(50000)};
    Check(TraceFormat::FrameMicroseconds(presents) == std::vector<std::uint32_t>{16667, 33333, 0}, "frame intervals in microseconds changed");
    Check(TraceFormat::FrameMicroseconds({}).empty(), "no presents gave frame intervals");
}

void BenchFormat() {
    AgcDriver::TraceSummary summary;
    summary.commit = "abc";
    summary.environment = {{"APS5_TRACE_BENCH", "1"}};
    summary.gpu = "GPU";
    summary.driver = "Driver 1";
    summary.os = "OS 1";
    summary.host = AgcDriver::TraceHost{"CPU", 8, 16000};
    const AgcDriver::TraceBenchIntervals intervals{
        std::make_shared<const AgcDriver::TraceBenchInterval>(AgcDriver::TraceBenchInterval{0, 30.5, 0, 1920, 1080, 120.25, {16667, 16666}}),
        std::make_shared<const AgcDriver::TraceBenchInterval>(AgcDriver::TraceBenchInterval{30.5, 61, 1830, 2560, 1440, 98.5, {}}),
    };
    const std::string expected =
        "{\"format\":\"aps5-bench/1\",\"commit\":\"abc\",\"dirty\":null,\"env\":{\"APS5_TRACE_BENCH\":\"1\"},\"gpu\":\"GPU\",\"driver\":\"Driver 1\",\"os\":\"OS 1\","
        "\"cpu\":\"CPU\",\"cpu_threads\":8,\"memory_mib\":16000,\"intervals_dropped\":3,\"intervals\":[\n"
        "{\"start_s\":0.000,\"end_s\":30.500,\"first_present\":0,\"resolution\":\"1920x1080\",\"gpu_busy_ms\":120.250,\"frame_us\":[16667,16666]},\n"
        "{\"start_s\":30.500,\"end_s\":61.000,\"first_present\":1830,\"resolution\":\"2560x1440\",\"gpu_busy_ms\":98.500,\"frame_us\":[]}\n]}\n";
    Check(TraceFormat::Bench(summary, intervals, 3) == expected, "benchmark file changed");
}

void BenchRotation() {
    AgcDriver::TraceBenchIntervals kept;
    std::uint64_t dropped = 0;
    for (std::size_t index = 0; index < AgcDriver::TraceBench::MaximumIntervals; ++index) AgcDriver::TraceBench::Keep(kept, std::make_shared<const AgcDriver::TraceBenchInterval>(AgcDriver::TraceBenchInterval{static_cast<double>(index)}), dropped);
    Check(kept.size() == AgcDriver::TraceBench::MaximumIntervals && dropped == 0, "intervals were dropped before the file was full");
    for (std::size_t index = 0; index < 5; ++index) AgcDriver::TraceBench::Keep(kept, std::make_shared<const AgcDriver::TraceBenchInterval>(AgcDriver::TraceBenchInterval{static_cast<double>(AgcDriver::TraceBench::MaximumIntervals + index)}), dropped);
    Check(kept.size() == AgcDriver::TraceBench::MaximumIntervals && dropped == 5, "a full file did not drop one interval per new one");
    Check(kept.front()->startS == 5 && kept.back()->startS == static_cast<double>(AgcDriver::TraceBench::MaximumIntervals + 4), "the file did not keep the newest intervals");
    const auto file = TraceFormat::Bench({}, kept, dropped);
    Check(file.find("\"intervals_dropped\":5,") != std::string::npos && file.find("{\"start_s\":5.000,") != std::string::npos && file.find("{\"start_s\":4.000,") == std::string::npos, "the file does not match the kept intervals");
}

void BenchWriterKeepsNewest() {
    std::mutex mutex;
    std::condition_variable changed;
    bool entered = false;
    bool release = false;
    std::vector<std::uint64_t> written;
    AgcDriver::TraceBenchWriter writer([&](const AgcDriver::TraceBenchSnapshot& snapshot) {
        std::unique_lock lock(mutex);
        written.push_back(snapshot.dropped);
        entered = true;
        changed.notify_all();
        changed.wait(lock, [&] { return release; });
    });
    writer.Write({{}, {}, 1});
    {
        std::unique_lock lock(mutex);
        changed.wait(lock, [&] { return entered; });
    }
    writer.Write({{}, {}, 2});
    writer.Write({{}, {}, 3});
    writer.Write({{}, {}, 4});
    {
        std::lock_guard lock(mutex);
        release = true;
    }
    changed.notify_all();
    writer.Flush();
    writer.Stop();
    Check(written == std::vector<std::uint64_t>{1, 4}, "a benchmark snapshot was written after a newer one replaced it");
    Check(writer.Superseded() == 2, "replaced benchmark snapshots were not counted");
}

void BenchWriterStopWrites() {
    std::vector<std::uint64_t> written;
    {
        AgcDriver::TraceBenchWriter writer([&](const AgcDriver::TraceBenchSnapshot& snapshot) { written.push_back(snapshot.dropped); });
        writer.Write({{}, {}, 7});
    }
    Check(written == std::vector<std::uint64_t>{7}, "stopping the benchmark writer lost its pending snapshot");
}

std::string ReadGzip(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    Check(bytes.size() > 18, "the gzip file is truncated");
    std::size_t size = 0;
    for (unsigned index = 0; index < 4; ++index) size |= static_cast<std::size_t>(bytes[bytes.size() - 4 + index]) << (8 * index);
    z_stream stream{};
    Check(inflateInit2(&stream, -MAX_WBITS) == Z_OK, "inflateInit2 failed");
    std::string inflated(size, 0);
    stream.next_in = const_cast<Bytef*>(bytes.data() + 10);
    stream.avail_in = static_cast<uInt>(bytes.size() - 18);
    stream.next_out = reinterpret_cast<Bytef*>(inflated.data());
    stream.avail_out = static_cast<uInt>(inflated.size());
    const int status = inflate(&stream, Z_FINISH);
    inflateEnd(&stream);
    Check(status == Z_STREAM_END && stream.avail_out == 0, "the gzip file does not inflate to its size");
    return inflated;
}

void ReplaceGzipFile() {
    const auto directory = std::filesystem::temp_directory_path() / ("aps5-bench-test-" + std::to_string(Clock::now().time_since_epoch().count()));
    std::filesystem::create_directories(directory);
    const auto path = directory / "aps5-bench.json.gz";
    auto temporary = path;
    temporary += ".tmp";
    AgcDriver::TraceFile::ReplaceGzip(path, "{\"first\":1}\n");
    AgcDriver::TraceFile::ReplaceGzip(path, "{\"second\":2}\n");
    Check(ReadGzip(path) == "{\"second\":2}\n" && !std::filesystem::exists(temporary), "the benchmark file was not replaced");
    const auto blocked = directory / "aps5-bench-blocked.json.gz";
    auto blockedTemporary = blocked;
    blockedTemporary += ".tmp";
    std::filesystem::create_directory(blocked);
    bool failed = false;
    try { AgcDriver::TraceFile::ReplaceGzip(blocked, "{\"third\":3}\n"); } catch (const std::system_error&) { failed = true; }
    const bool blockedKept = std::filesystem::is_directory(blocked);
    const bool temporaryLeft = std::filesystem::exists(blockedTemporary);
    const auto kept = ReadGzip(path);
#ifndef _WIN32
    // A directory at <name>.tmp makes the write itself fail, which is the case ReplaceGzip exists
    // for: the previous file must survive, so an in-place rewrite fails this check. The failure is
    // a std::ios_base::failure thrown inside libstdc++'s ofstream, which MinGW builds could not
    // unwind to this catch, so the case runs on the other platforms only.
    std::filesystem::create_directory(temporary);
    bool writeFailed = false;
    try { AgcDriver::TraceFile::ReplaceGzip(path, "{\"third\":3}\n"); } catch (const std::exception&) { writeFailed = true; }
    const auto keptAfterWrite = ReadGzip(path);
#endif
#ifdef __linux__
    bool fullFailed = false;
    bool fullLeft = false;
    if (std::filesystem::exists("/dev/full")) {
        const auto fullPath = directory / "aps5-bench-full.json.gz";
        auto fullTemporary = fullPath;
        fullTemporary += ".tmp";
        std::filesystem::create_symlink("/dev/full", fullTemporary);
        try { AgcDriver::TraceFile::ReplaceGzip(fullPath, "{\"full\":1}\n"); } catch (const std::exception&) { fullFailed = true; }
        std::error_code linkError;
        fullLeft = std::filesystem::symlink_status(fullTemporary, linkError).type() != std::filesystem::file_type::not_found;
    } else {
        fullFailed = true;
    }
#endif
    std::filesystem::remove_all(directory);
    Check(failed && blockedKept && !temporaryLeft, "a replace that could not move its file over the destination did not fail cleanly or left its temporary file");
    Check(kept == "{\"second\":2}\n", "a failed replace of another file damaged the benchmark file");
#ifdef __linux__
    Check(fullFailed && !fullLeft, "a replace whose write failed left its temporary file behind");
#endif
#ifndef _WIN32
    Check(writeFailed && keptAfterWrite == "{\"second\":2}\n", "a failed rewrite damaged the previous benchmark file");
#endif
}

void BenchSwitch() {
    Check(!AgcDriver::TraceBench::Parse(nullptr, false), "the benchmark file was enabled without its switch");
    Check(AgcDriver::TraceBench::Parse("1", true), "APS5_TRACE_BENCH=1 was not accepted");
    for (const auto& [text, every] : {std::pair{"1", false}, std::pair{"0", true}, std::pair{"yes", true}, std::pair{"", true}}) {
        bool rejected = false;
        try { AgcDriver::TraceBench::Parse(text, every); }
        catch (const std::invalid_argument&) { rejected = true; }
        Check(rejected, "a malformed or unsupported APS5_TRACE_BENCH was accepted");
    }
}

void GzipFile() {
    std::string text;
    for (int index = 0; index < 20000; ++index) text += "{\"name\":\"Driver.Packet\",\"ph\":\"X\",\"ts\":" + std::to_string(index) + "}\n";
    const auto stem = "aps5-trace-test-" + std::to_string(Clock::now().time_since_epoch().count());
    const auto path = std::filesystem::temp_directory_path() / (stem + ".json.gz");
    AgcDriver::TraceFile::WriteGzip(path, text);
    std::ifstream input(path, std::ios::binary);
    const std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    std::filesystem::remove(path);
    Check(bytes.size() > 18 && bytes[0] == 0x1f && bytes[1] == 0x8b && bytes[2] == 8 && bytes[3] == 0, "the gzip header is wrong");
    z_stream stream{};
    Check(inflateInit2(&stream, -MAX_WBITS) == Z_OK, "inflateInit2 failed");
    std::string inflated(text.size() + 1, 0);
    stream.next_in = const_cast<Bytef*>(bytes.data() + 10);
    stream.avail_in = static_cast<uInt>(bytes.size() - 18);
    stream.next_out = reinterpret_cast<Bytef*>(inflated.data());
    stream.avail_out = static_cast<uInt>(inflated.size());
    const int status = inflate(&stream, Z_FINISH);
    inflated.resize(inflated.size() - stream.avail_out);
    const bool consumed = stream.avail_in == 0;
    inflateEnd(&stream);
    Check(status == Z_STREAM_END && consumed, "the deflate stream does not end at the trailer");
    Check(inflated == text, "the gzip file does not hold the text");
    const auto word = [&](std::size_t at) {
        std::uint32_t value = 0;
        for (unsigned index = 0; index < 4; ++index) value |= static_cast<std::uint32_t>(bytes[at + index]) << (8 * index);
        return value;
    };
    Check(word(bytes.size() - 8) == crc32(0, reinterpret_cast<const Bytef*>(text.data()), static_cast<uInt>(text.size())), "the CRC-32 is wrong");
    Check(word(bytes.size() - 4) == text.size(), "the size is wrong");
}

void AppendLines() {
    const auto path = std::filesystem::temp_directory_path() / ("aps5-summary-test-" + std::to_string(Clock::now().time_since_epoch().count()) + ".jsonl");
    AgcDriver::TraceFile::AppendLine(path, "{\"presents\":1}\n");
    AgcDriver::TraceFile::AppendLine(path, "{\"presents\":2}\n");
    std::ifstream input(path, std::ios::binary);
    const std::string written((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    input.close();
    std::filesystem::remove(path);
    Check(written == "{\"presents\":1}\n{\"presents\":2}\n", "summary lines were not appended");
}

}

int main() {
    try {
        WindowByFrames();
        WindowBySeconds();
        WindowRejects();
        BufferCap();
        LostEventsCount();
        TotalsOverflow();
        CapacitySettings();
        FullBufferEvents();
        FoldShortSlices();
        EscapeStrings();
        Utf8Strings();
        EventsShape();
        BatchIds();
        SummaryFields();
        GpuClock();
        IntervalDue();
        SummaryOnlyThread();
        TotalsSince();
        IntervalSummaryFields();
        IntervalDroppedThreads();
        SummaryMachineFields();
        FrameMicroseconds();
        BenchFormat();
        BenchRotation();
        BenchWriterKeepsNewest();
        BenchWriterStopWrites();
        BenchSwitch();
        GzipFile();
        ReplaceGzipFile();
        AppendLines();
        std::cout << "performance trace tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
