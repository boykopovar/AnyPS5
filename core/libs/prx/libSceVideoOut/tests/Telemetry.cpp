#include "prx/libSceVideoOut/include/DisplayWindow.hpp"
#include "prx/libSceVideoOut/include/Telemetry.hpp"

#include <chrono>
#include <cstdio>
#ifdef _WIN32
#include <process.h>
#define getpid _getpid
#else
#include <unistd.h>
#endif
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <thread>
#include <string>
#include <vector>

#undef main

namespace {

void check(bool condition, const std::string& reason) {
    if (!condition) throw std::runtime_error(reason);
}

std::vector<std::string> readLines(const std::filesystem::path& path) {
    std::ifstream input(path);
    std::vector<std::string> lines;
    for (std::string line; std::getline(input, line);) lines.push_back(line);
    return lines;
}

long long field(const std::string& line, const std::string& name) {
    const auto position = line.find("\"" + name + "\":");
    check(position != std::string::npos, "missing field " + name + " in " + line);
    return std::stoll(line.substr(position + name.size() + 3));
}

std::filesystem::path freshDirectory(const char* name) {
    auto directory = std::filesystem::temp_directory_path() / (std::string("aps5-videoout-telemetry-") + name + "-" + std::to_string(::getpid()));
    std::filesystem::remove_all(directory);
    std::filesystem::create_directories(directory);
    return directory;
}

void padChangeIgnoresSensors() {
    VideoOutTelemetry::PadChangeFilter filter;
    PadInputState state;
    check(!filter.Changed(state), "idle state must not be a change");
    state.hasMotion = true;
    state.accel = {1.0f, 2.0f, 3.0f};
    state.gyro = {0.5f, 0.5f, 0.5f};
    state.touch[0] = {true, 100, 200, 1};
    state.touchLeft = true;
    check(!filter.Changed(state), "motion and touch must not be a change");
    state.buttons = 0x4000;
    check(filter.Changed(state), "a button must be a change");
    check(filter.Changed(state), "an uncommitted change must stay a change");
    filter.Commit(state);
    check(!filter.Changed(state), "a repeated state must not be a change");
    state.sticks[2] = 200;
    check(filter.Changed(state), "a stick must be a change");
    filter.Commit(state);
    state.analogButtonsR2 = 10;
    check(filter.Changed(state), "a trigger must be a change");
}

void padChangeSurvivesFullQueue() {
    const auto directory = freshDirectory("fullqueue");
    {
        VideoOutTelemetry::Writer writer(directory.string(), 1, "test");
        VideoOutTelemetry::PadChangeFilter filter;
        PadInputState state;
        state.buttons = 0x4000;
        check(!VideoOutTelemetry::EmitPadIfChanged(writer, filter, 5, state), "a full queue must refuse the pad line");
        check(filter.Changed(state), "a refused pad change must stay pending");
        check(writer.Dropped() == 1, "the refused pad line must be counted");
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        check(VideoOutTelemetry::EmitPadIfChanged(writer, filter, 6, state), "the retry must succeed once the queue drains");
        check(!filter.Changed(state), "a written pad change must be committed");
    }
    const auto lines = readLines(directory / "videoout.jsonl");
    bool found = false;
    for (const auto& line : lines) found = found || (line.find("\"ev\":\"pad\"") != std::string::npos && field(line, "n") == 6 && field(line, "buttons") == 0x4000);
    check(found, "the retried pad line must be in the file");
    bool dropsLine = false;
    for (const auto& line : lines) dropsLine = dropsLine || (line.find("\"ev\":\"drops\"") != std::string::npos && field(line, "dropped") == 1);
    check(dropsLine, "the writer must report the refused line in a drops line");
    std::filesystem::remove_all(directory);
}

void noEnvironmentOpensNothing() {
    const auto directory = freshDirectory("noenv");
    check(VideoOutTelemetry::OpenFromEnvironment(nullptr, "test") == nullptr, "unset variable must not open a writer");
    check(VideoOutTelemetry::OpenFromEnvironment("", "test") == nullptr, "empty variable must not open a writer");
    check(std::filesystem::is_empty(directory), "no file may exist without the variable");
    std::filesystem::remove_all(directory);
}

void queueOverflowIsCounted() {
    VideoOutTelemetry::BoundedLineQueue queue(2);
    check(queue.TryPush("a"), "first line must fit");
    check(queue.TryPush("b"), "second line must fit");
    check(!queue.TryPush("c"), "third line must overflow");
    check(!queue.TryPush("d"), "fourth line must overflow");
    check(queue.Dropped() == 2, "two overflows must be counted");
    check(queue.Drain().size() == 2, "the stored lines must survive");
    check(queue.TryPush("e"), "a drained queue must accept lines");
}

void contractPadAndPresentLines() {
    const auto directory = freshDirectory("contract");
    {
        auto writer = VideoOutTelemetry::OpenFromEnvironment(directory.string().c_str(), "abc123");
        check(writer != nullptr, "variable must open a writer");
        PadInputState state;
        state.buttons = 0x4000;
        writer->EmitPad(7, state);
        writer->EmitPresent(30, 59.94);
        writer->EmitCompletedFlip(4, 31, 1800);
    }
    const auto lines = readLines(directory / "videoout.jsonl");
    check(lines.size() == 3, "expected contract, pad and present, got " + std::to_string(lines.size()));
    check(lines[0].find("\"ev\":\"contract\"") != std::string::npos, "contract must be first");
    check(lines[0].find("\"contract\":\"v1-pad\"") != std::string::npos, "contract must name the level");
    check(lines[0].find("\"commit\":\"abc123\"") != std::string::npos, "contract must carry the commit");
    check(lines[0].find("\"prx\":\"libSceVideoOut\"") != std::string::npos, "contract must name the prx");
    check(field(lines[0], "pid") > 0, "contract must carry the pid");
    check(lines[1].find("\"ev\":\"pad\"") != std::string::npos && field(lines[1], "n") == 7 && field(lines[1], "buttons") == 0x4000, "pad line");
    check(lines[1].find("\"sticks\":[128,128,128,128]") != std::string::npos, "pad sticks");
    check(lines[2].find("\"ev\":\"present\"") != std::string::npos && field(lines[2], "n") == 30, "present line");
    std::filesystem::remove_all(directory);
}

void flipTimingWindowRequiresValidRange() {
    using VideoOutTelemetry::ParseFlipTimingWindow;
    check(!ParseFlipTimingWindow(nullptr), "unset flip timing must be disabled");
    check(!ParseFlipTimingWindow(""), "empty flip timing must be disabled");
    const auto ordinary = ParseFlipTimingWindow("9:100000");
    check(ordinary && ordinary->first == 9 && ordinary->count == 100000, "flip timing must parse the requested window");
    const auto last = ParseFlipTimingWindow("18446744073709551614:2");
    check(last && last->first == std::numeric_limits<std::uint64_t>::max() - 1 && last->count == 2,
        "flip timing must accept a window ending at the largest sequence");
    for (const char* value : {"1", "1:", ":2", "0:2", "1:1", "1:100001", "-1:2", " 1:2", "1:2 ", "1:2:3",
             "18446744073709551615:2", "18446744073709551616:2", "1:18446744073709551616"}) {
        bool rejected = false;
        try {
            ParseFlipTimingWindow(value);
        } catch (const std::exception&) {
            rejected = true;
        }
        check(rejected, std::string("invalid flip timing must fail: ") + value);
    }
}

template <typename Emit>
std::vector<std::string> captureFlipLines(VideoOutTelemetry::FlipTimingWindow window, Emit emit) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        const auto directory = freshDirectory("flip-capture");
        bool dropped = false;
        {
            VideoOutTelemetry::Writer writer(directory.string(), 16, "test", window);
            emit(writer);
            dropped = writer.Dropped() != 0;
        }
        auto lines = readLines(directory / "videoout.jsonl");
        std::filesystem::remove_all(directory);
        if (!dropped) return lines;
    }
    throw std::runtime_error("each completed flip capture encountered queue contention");
}

void completedFlipCaptureRecordsWindow() {
    const auto lines = captureFlipLines({3, 2}, [](VideoOutTelemetry::Writer& writer) {
        writer.EmitCompletedFlip(7, 2, 1700);
        writer.EmitCompletedFlip(7, 3, 1800);
        writer.EmitCompletedFlip(7, 4, 2000);
        writer.EmitCompletedFlip(7, 5, 2200);
    });
    check(lines.size() == 3, "exactly two flips must be recorded after the contract");
    check(field(lines[0], "flip_start") == 3 && field(lines[0], "flip_count") == 2,
        "the contract must declare the expected capture range");
    for (std::size_t index = 1; index < lines.size(); ++index) {
        const auto& line = lines[index];
        check(line.find("\"ev\":\"flip_complete\"") != std::string::npos, "a captured flip must have its own event");
        check(field(line, "generation") == 7 && field(line, "n") == static_cast<long long>(index + 2),
            "captured flips must retain the completion sequence and generation");
        check(field(line, "process_us") == (index == 1 ? 1800 : 2000), "a captured flip must retain its process timestamp");
    }
}

void completedFlipCapturePinsGeneration() {
    for (const bool captureStarted : {false, true}) {
        const auto lines = captureFlipLines({2, 3}, [captureStarted](VideoOutTelemetry::Writer& writer) {
            writer.EmitCompletedFlip(4, 1, 100);
            if (captureStarted) writer.EmitCompletedFlip(4, 2, 110);
            writer.EmitCompletedFlip(5, 1, 200);
            writer.EmitCompletedFlip(5, 2, 210);
            writer.EmitCompletedFlip(5, 3, 220);
            writer.EmitCompletedFlip(5, 4, 230);
        });
        check(lines.size() == (captureStarted ? 2u : 1u), "a reopened output must not extend an incomplete capture");
        check(field(lines[0], "flip_start") == 2 && field(lines[0], "flip_count") == 3,
            "an incomplete capture must retain its original bounds");
        if (captureStarted) {
            check(field(lines[1], "generation") == 4 && field(lines[1], "n") == 2,
                "the valid flip from the original generation must survive");
        }
    }
}

void completedFlipCaptureCountsLoss() {
    const auto directory = freshDirectory("flip-loss");
    {
        VideoOutTelemetry::Writer writer(directory.string(), 0, "test", VideoOutTelemetry::FlipTimingWindow{2, 2});
        const auto initialDrops = writer.Dropped();
        for (std::uint64_t sequence = 1; sequence <= 4; ++sequence) writer.EmitCompletedFlip(2, sequence, sequence * 100);
        check(writer.Dropped() == initialDrops + 2, "each refused in-window flip must be counted once");
    }
    const auto lines = readLines(directory / "videoout.jsonl");
    check(lines.size() == 1 && lines[0].find("\"ev\":\"drops\"") != std::string::npos && field(lines[0], "dropped") >= 2,
        "the writer must report capture loss");
    std::filesystem::remove_all(directory);
}

int frameNumberMatchesCaption() {
    const auto directory = freshDirectory("caption");
    std::filesystem::create_directories(directory / "app0" / "sce_sys");
    std::ofstream(directory / "app0" / "sce_sys" / "param.json") << R"({"titleId":"PPSA00000","localizedParameters":{"defaultLanguage":"en-US","en-US":{"titleName":"Telemetry Test"}}})";
    std::filesystem::current_path(directory);
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        std::printf("skipped, no display or Vulkan device: SDL video driver: %s\n", SDL_GetError());
        return 77;
    }
    DisplayWindow window;
    try {
        window.Ensure(640, 360);
    } catch (const std::exception& error) {
        std::printf("skipped, no display or Vulkan device: %s\n", error.what());
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
        return 77;
    }
    check(window.FrameNumber() == 0, "no frame before the first title update");
    for (std::uint64_t expected = 1; expected <= 65; ++expected) {
        window.UpdateTitle();
        check(window.FrameNumber() == expected, "FrameNumber must count title updates");
        const std::string title = SDL_GetWindowTitle(window.Handle());
        const auto open = title.rfind('(');
        check(open != std::string::npos && std::stoull(title.substr(open + 1)) == expected, "caption counter differs from FrameNumber: " + title);
    }
    window.Destroy();
    SDL_QuitSubSystem(SDL_INIT_VIDEO);
    std::filesystem::current_path(directory.parent_path());
    std::filesystem::remove_all(directory);
    return 0;
}

}

int main(int argc, char** argv) {
    try {
        if (argc > 1 && std::string(argv[1]) == "caption") {
            const int result = frameNumberMatchesCaption();
            if (result == 0) std::printf("VideoOut caption counter test passed\n");
            return result;
        }
        padChangeIgnoresSensors();
        padChangeSurvivesFullQueue();
        noEnvironmentOpensNothing();
        queueOverflowIsCounted();
        contractPadAndPresentLines();
        flipTimingWindowRequiresValidRange();
        completedFlipCaptureRecordsWindow();
        completedFlipCapturePinsGeneration();
        completedFlipCaptureCountsLoss();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
    std::printf("VideoOut telemetry tests passed\n");
    return 0;
}
