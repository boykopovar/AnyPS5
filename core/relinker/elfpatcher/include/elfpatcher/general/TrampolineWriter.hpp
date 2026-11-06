#ifndef ELFPATCHER_TRAMPOLINEWRITER_HPP
#define ELFPATCHER_TRAMPOLINEWRITER_HPP

#include <codegen/CodegenTypes.hpp>
#include <cstdint>
#include <functional>
#include <vector>

namespace Elfpatcher {

void AppendTrampoline(std::vector<std::uint8_t>& bytes, const Codegen::TrampolineSite& site, std::uint64_t imageEnd, const std::function<std::uint64_t(std::uint64_t)>& addressOfOffset);

}

#endif
