#ifndef ELFPATCHER_MACOS_CODE_SIGNATURE_HPP
#define ELFPATCHER_MACOS_CODE_SIGNATURE_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace Elfpatcher::MacOs {

std::array<std::uint8_t, 32> Sha256(const std::uint8_t* data, std::size_t size);

std::size_t CodeSignatureSize(std::uint64_t codeLimit, const std::string& identifier);

void AppendCodeSignature(std::vector<std::uint8_t>& file, const std::string& identifier, std::uint64_t executableSegmentOffset, std::uint64_t executableSegmentSize, bool mainBinary);

}

#endif
