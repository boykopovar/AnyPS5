#include <elfpatcher/windows/WindowsStubEmitter.hpp>
#include <domain/Types.hpp>
#include <io/BufferUtils.hpp>
#include <limits>
#include <utility>

namespace Elfpatcher::Windows {

WindowsStubEmitter::WindowsStubEmitter(const std::uint32_t codeRva) : codeRva(codeRva) {}

void WindowsStubEmitter::Emit(const std::initializer_list<std::uint8_t> values) {
    bytes.insert(bytes.end(), values.begin(), values.end());
}

void WindowsStubEmitter::U32(const std::uint32_t value) {
    Io::AppendU32(bytes, value);
}

void WindowsStubEmitter::U64(const std::uint64_t value) {
    Io::AppendU64(bytes, value);
}

void WindowsStubEmitter::Rip(const std::initializer_list<std::uint8_t> opcode, const std::uint32_t targetRva) {
    PatchBranch(Branch(opcode), targetRva);
}

std::size_t WindowsStubEmitter::Branch(const std::initializer_list<std::uint8_t> opcode) {
    Emit(opcode);
    const auto offset = bytes.size();
    U32(0);
    return offset;
}

void WindowsStubEmitter::PatchBranch(const std::size_t offset, const std::uint32_t targetRva) {
    if (offset > bytes.size() || bytes.size() - offset < 4)
        throw Domain::RelinkerException("Invalid bootstrap branch offset", offset);
    const auto displacement = static_cast<std::int64_t>(targetRva) - (static_cast<std::int64_t>(codeRva) + static_cast<std::int64_t>(offset) + 4);
    if (displacement < std::numeric_limits<std::int32_t>::min() || displacement > std::numeric_limits<std::int32_t>::max())
        throw Domain::RelinkerException("Bootstrap branch exceeds rel32 range", targetRva);
    Io::WriteU32(bytes, offset, static_cast<std::uint32_t>(displacement));
}

std::uint32_t WindowsStubEmitter::GetRva() const {
    return CheckedRva(codeRva + bytes.size());
}

std::vector<std::uint8_t> WindowsStubEmitter::TakeBytes() {
    return std::move(bytes);
}

}
