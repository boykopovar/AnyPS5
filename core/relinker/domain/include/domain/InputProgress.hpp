#pragma once

#include <domain/IProgress.hpp>
#include <utility>

namespace Domain {

class InputProgress final : public IProgress {
public:
    InputProgress(IProgress& progress, std::string input) : progress(progress), input(std::move(input)) {}

    void Report(const ProgressEvent& event) override {
        auto named = event;
        if (named.Input.empty()) named.Input = input;
        progress.Report(named);
    }

    void Message(std::string_view message) override { progress.Message(message); }

private:
    IProgress& progress;
    std::string input;
};

}
