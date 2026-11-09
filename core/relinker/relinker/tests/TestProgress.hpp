#pragma once

#include <domain/IProgress.hpp>
#include <vector>

class TestProgress final : public Domain::IProgress {
public:
    std::vector<Domain::ProgressEvent> Events;
    std::string Messages;

    void Report(const Domain::ProgressEvent& event) override { Events.push_back(event); }
    void Message(std::string_view message) override { Messages += message; }
};

inline TestProgress testProgress;
