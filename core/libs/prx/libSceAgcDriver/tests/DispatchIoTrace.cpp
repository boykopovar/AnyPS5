#include "prx/libSceAgcDriver/Execution/include/DispatchIoTrace.hpp"
#include <iostream>
#include <mutex>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

namespace {

using AgcDriver::DispatchIoTrace;

std::mutex g_mutex;
std::vector<std::string> g_lines;

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<std::string> Take() {
    std::lock_guard lock(g_mutex);
    return std::exchange(g_lines, {});
}

void CheckOneLinePerDispatch() {
    {
        const DispatchIoTrace trace("queue=0x1 submission=7 shader 0x1000");
        DispatchIoTrace::Resources("8x8x1", " buffer 0x2000+0x100 nz=0.50");
    }
    const auto lines = Take();
    Require(lines.size() == 1 && lines[0] == "[dispatch-io] queue=0x1 submission=7 shader 0x1000 8x8x1: buffer 0x2000+0x100 nz=0.50\n", "a dispatch with resources is not one line");
}

void CheckDispatchWithoutResourcesKeepsItsHeader() {
    {
        const DispatchIoTrace trace("queue=0x2 submission=8 shader 0x3000 00000001 00000002");
    }
    const auto lines = Take();
    Require(lines.size() == 1 && lines[0] == "[dispatch-io] queue=0x2 submission=8 shader 0x3000 00000001 00000002\n", "a dispatch that recorded nothing lost its line");
}

void CheckResourcesWithoutDispatch() {
    DispatchIoTrace::Resources("indirect", " data+0x40 nz=1.00");
    const auto lines = Take();
    Require(lines.size() == 1 && lines[0] == "[dispatch-io] indirect: data+0x40 nz=1.00\n", "resources recorded outside a traced dispatch changed their line");
}

void CheckRepeatedDispatchIsOneLine() {
    {
        const DispatchIoTrace first("queue=0x1 submission=9 shader 0x1000");
        {
            const DispatchIoTrace again("queue=0x1 submission=9 shader 0x1000");
            DispatchIoTrace::Resources("4x4x1", " buffer(ro) 0x2000+0x10 nz=0.00");
        }
    }
    const auto lines = Take();
    Require(lines.size() == 1 && lines[0] == "[dispatch-io] queue=0x1 submission=9 shader 0x1000 4x4x1: buffer(ro) 0x2000+0x10 nz=0.00\n", "a dispatch that started over printed more than its one line");
}

void CheckNestedOtherDispatchKeepsBothLines() {
    {
        const DispatchIoTrace outer("queue=0x1 submission=10 shader 0x1000");
        {
            const DispatchIoTrace inner("queue=0x0 submission=4 shader 0x5000");
            DispatchIoTrace::Resources("1x1x1", " data+0x8 nz=1.00");
        }
        DispatchIoTrace::Resources("2x2x1", " data+0x10 nz=1.00");
    }
    {
        const DispatchIoTrace outer("queue=0x1 submission=12 shader 0x1000");
        const DispatchIoTrace inner("queue=0x0 submission=5 shader 0x5000");
        DispatchIoTrace::Resources("1x1x1", " data+0x8 nz=1.00");
    }
    const auto lines = Take();
    Require(lines.size() == 4, "nested dispatches printed " + std::to_string(lines.size()) + " lines");
    Require(lines[0] == "[dispatch-io] queue=0x0 submission=4 shader 0x5000 1x1x1: data+0x8 nz=1.00\n", "the nested dispatch changed its line");
    Require(lines[1] == "[dispatch-io] queue=0x1 submission=10 shader 0x1000 2x2x1: data+0x10 nz=1.00\n", "the outer dispatch lost its header after the nested one");
    Require(lines[2] == "[dispatch-io] queue=0x0 submission=5 shader 0x5000 1x1x1: data+0x8 nz=1.00\n", "the second nested dispatch changed its line");
    Require(lines[3] == "[dispatch-io] queue=0x1 submission=12 shader 0x1000\n", "an outer dispatch that recorded nothing lost its line");
}

void CheckHandOverToAnotherThread() {
    {
        DispatchIoTrace trace("queue=0x0 submission=11 shader 0x7000");
        std::thread committer([header = trace.Detach()] {
            const DispatchIoTrace commit(header);
            DispatchIoTrace::Resources("16x16x1", " storage 0x9000+0x4000(64x64 f10 t27) nz=0.25");
        });
        committer.join();
    }
    const auto lines = Take();
    Require(lines.size() == 1 && lines[0] == "[dispatch-io] queue=0x0 submission=11 shader 0x7000 16x16x1: storage 0x9000+0x4000(64x64 f10 t27) nz=0.25\n", "a dispatch committed on another thread is not one line");
}

void CheckThreadsKeepTheirOwnDispatch() {
    constexpr int Threads = 8;
    constexpr int Dispatches = 200;
    std::vector<std::thread> threads;
    for (int thread = 0; thread < Threads; ++thread) {
        threads.emplace_back([thread] {
            for (int dispatch = 0; dispatch < Dispatches; ++dispatch) {
                const DispatchIoTrace trace("queue=0x" + std::to_string(thread) + " submission=" + std::to_string(dispatch) + " shader 0x1000");
                DispatchIoTrace::Resources("8x8x1", " buffer 0x" + std::to_string(thread) + "000+0x100 nz=0.50");
            }
        });
    }
    for (auto& thread : threads) thread.join();
    const auto lines = Take();
    Require(lines.size() == static_cast<std::size_t>(Threads) * Dispatches, "threads printed " + std::to_string(lines.size()) + " lines");
    std::vector<int> next(Threads, 0);
    for (const auto& line : lines) {
        bool matched = false;
        for (int thread = 0; thread < Threads && !matched; ++thread) {
            const auto expected = "[dispatch-io] queue=0x" + std::to_string(thread) + " submission=" + std::to_string(next[thread]) + " shader 0x1000 8x8x1: buffer 0x" + std::to_string(thread) + "000+0x100 nz=0.50\n";
            if (line != expected) continue;
            ++next[thread];
            matched = true;
        }
        Require(matched, "a line mixes two threads' dispatches or is out of order: " + line);
    }
}

}

int main() {
    try {
        DispatchIoTrace::SetWriter([](std::string_view line) {
            std::lock_guard lock(g_mutex);
            g_lines.emplace_back(line);
        });
        CheckOneLinePerDispatch();
        CheckDispatchWithoutResourcesKeepsItsHeader();
        CheckResourcesWithoutDispatch();
        CheckRepeatedDispatchIsOneLine();
        CheckNestedOtherDispatchKeepsBothLines();
        CheckHandOverToAnotherThread();
        CheckThreadsKeepTheirOwnDispatch();
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "dispatch io trace tests passed\n";
    return 0;
}
