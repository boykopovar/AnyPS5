#ifndef CODEGEN_X86_AVX2TWINLOWERING_HPP
#define CODEGEN_X86_AVX2TWINLOWERING_HPP

#include <codegen/x86/StubBodyBuilder.hpp>
#include <cstdint>
#include <optional>
#include <vector>

namespace Codegen {

struct Avx2Twin {
    const char* Name;
    std::vector<std::uint8_t> Replacement;
};

[[nodiscard]] std::optional<Avx2Twin> DecodeAvx2Twin(const std::uint8_t* data, std::size_t length);
void EmitAvx2Twin(StubBodyBuilder& body, const Avx2Twin& twin);

}

#endif
