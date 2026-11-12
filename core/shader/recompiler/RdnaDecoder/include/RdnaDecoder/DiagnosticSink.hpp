#ifndef CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_DIAGNOSTICSINK_HPP
#define CORE_SHADER_RECOMPILIER_RDNADECODER_INCLUDE_RDNADECODER_DIAGNOSTICSINK_HPP

#include <atomic>

namespace ShaderRecompiler {

struct DiagnosticSink {
    bool (*enabled)() = nullptr;
    void (*shader)(int what) = nullptr;
    void (*unsupportedInstruction)(const char* encoding, const char* opcode, long long opcodeValue, const char* file, int line) = nullptr;
    void (*imageFormat)(unsigned format, const char* name, int status, int usage) = nullptr;
    void (*descriptor)(int type, int status, int usage) = nullptr;
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

}

#endif
