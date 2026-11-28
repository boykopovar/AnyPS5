#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_DIAGNOSTICSINK_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_DIAGNOSTICSINK_HPP

#include <atomic>
#include <cstddef>
#include <cstdio>
#include <stdexcept>
#include <string>

namespace ShaderRecompiler {

struct DiagnosticSink {
    bool (*enabled)() = nullptr;
    void (*shader)(int what) = nullptr;
    void (*unsupportedInstruction)(const char* encoding, const char* opcode, long long opcodeValue, const char* file, int line) = nullptr;
    void (*imageFormat)(unsigned format, const char* name, int status, int usage) = nullptr;
    void (*descriptor)(int type, int status, int usage) = nullptr;
    void (*compileTime)(long long nanoseconds) = nullptr;
    void (*profileLine)(const char* text) = nullptr;
};

inline std::atomic<const DiagnosticSink*>& DiagnosticSinkSlot() {
    static std::atomic<const DiagnosticSink*> slot{nullptr};
    return slot;
}

inline void SetDiagnosticSink(const DiagnosticSink* sink) {
    DiagnosticSinkSlot().store(sink, std::memory_order_release);
}

inline const DiagnosticSink* Diagnostics() {
    return DiagnosticSinkSlot().load(std::memory_order_acquire);
}

template<typename... TArgs>
void ProfileLine(const char* format, TArgs... args) {
    const auto size = std::snprintf(nullptr, 0, format, args...);
    if (size < 0) throw std::runtime_error("profile line formatting failed");
    std::string text(static_cast<std::size_t>(size), '\0');
    std::snprintf(text.data(), text.size() + 1, format, args...);
    if (const auto* sink = Diagnostics(); sink != nullptr && sink->profileLine != nullptr) {
        sink->profileLine(text.c_str());
        return;
    }
    std::fputs(text.c_str(), stderr);
}

}

#endif
