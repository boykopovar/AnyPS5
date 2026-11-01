#ifndef DOMAIN_IPROGRESS_HPP
#define DOMAIN_IPROGRESS_HPP

#include <cstdint>
#include <string>
#include <string_view>

namespace Domain {

enum class ProgressStage { Executable, Modules, Syscalls, Decode, Lowering, Imports, Image, Write };

struct ProgressEvent {
    ProgressStage Stage;
    std::string Input;
    std::uint64_t Completed;
    std::uint64_t Total;
};

class IProgress {
public:
    virtual ~IProgress() = default;
    virtual void Report(const ProgressEvent& event) = 0;
    virtual void Message(std::string_view message) = 0;
};

}

#endif
