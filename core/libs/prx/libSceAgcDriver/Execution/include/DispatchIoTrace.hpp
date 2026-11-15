#pragma once
#include "prx/libSceAgcDriver/Execution/include/AsyncLog.hpp"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <string_view>
#include <utility>

namespace AgcDriver {

class DispatchIoTrace {
public:
    using Writer = void (*)(std::string_view line);

    static bool Enabled() {
        static const bool enabled = std::getenv("APS5_TRACE_DISPATCH_IO") != nullptr;
        return enabled;
    }

    static bool Words() {
        static const bool words = [] {
            const char* value = std::getenv("APS5_TRACE_DISPATCH_IO");
            return value != nullptr && value[0] == '2';
        }();
        return words;
    }

    static Writer SetWriter(Writer writer) {
        return std::exchange(Output(), writer);
    }

    static void Resources(const char* groups, const std::string& described) noexcept {
        try {
            std::string line = "[dispatch-io] ";
            if (auto* const trace = Current(); trace != nullptr) {
                line += trace->header;
                line += ' ';
                trace->written = true;
            }
            line += groups;
            line += ':';
            line += described;
            line += '\n';
            Output()(line);
        } catch (...) {
        }
    }

    explicit DispatchIoTrace(std::string text) : header(std::move(text)), outer(Current()) {
        if (outer != nullptr && outer->header == header) outer->written = true;
        Current() = this;
    }

    DispatchIoTrace(const DispatchIoTrace&) = delete;
    DispatchIoTrace& operator=(const DispatchIoTrace&) = delete;

    ~DispatchIoTrace() {
        finish();
        Current() = outer;
    }

    std::string Detach() {
        written = true;
        return header;
    }

private:
    static DispatchIoTrace*& Current() {
        static thread_local DispatchIoTrace* current = nullptr;
        return current;
    }

    static Writer& Output() {
        static Writer writer = [](std::string_view line) { AsyncLog::Get().Write(stderr, line.data(), line.size()); };
        return writer;
    }

    void finish() noexcept {
        if (written) return;
        written = true;
        try {
            Output()("[dispatch-io] " + header + "\n");
        } catch (...) {
        }
    }

    std::string header;
    DispatchIoTrace* outer;
    bool written = false;
};

}
