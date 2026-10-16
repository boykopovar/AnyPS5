#ifndef CORE_WASM_RUNTIME_SRC_PROGRAM_HPP
#define CORE_WASM_RUNTIME_SRC_PROGRAM_HPP

#include "Metadata.hpp"
#include <cstdint>
#include <vector>

namespace WasmRuntime {

struct TlsLayout {
    std::vector<std::int64_t> offsets;
    std::uint64_t staticSize = 0;
    std::uint64_t alignment = 16;
};

struct Program {
    std::vector<std::uint8_t> metadataBytes;
    Metadata metadata;
    std::vector<std::uint64_t> bases;
    std::uint64_t stubBase = 0;
    std::uint64_t addrMap = 0;
    TlsLayout tls;
};

Program& LoadProgram(std::vector<std::uint8_t> metadataBytes);
Program& CurrentProgram();
bool ProgramLoaded();

std::uint64_t ExportAddress(const Program& program, std::size_t image, const std::string& nid);
void InitializeImages(Program& program);

struct ThreadTls {
    std::uint64_t block = 0;
    std::uint64_t bytes = 0;
    std::uint64_t threadPointer = 0;
};

ThreadTls CreateThreadTls(const Program& program);

}  // namespace WasmRuntime

#endif
