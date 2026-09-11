#ifndef ELFPATCHER_WINDOWS_STUBEMITTER_HPP
#define ELFPATCHER_WINDOWS_STUBEMITTER_HPP

#include <elfpatcher/windows/WindowsPeFormat.hpp>
#include <initializer_list>

namespace Elfpatcher::Windows {

class WindowsStubEmitter {
public:
    explicit WindowsStubEmitter(std::uint32_t codeRva);
    void Emit(std::initializer_list<std::uint8_t> bytes);
    void U32(std::uint32_t value);
    void U64(std::uint64_t value);
    void Rip(std::initializer_list<std::uint8_t> opcode, std::uint32_t targetRva);
    std::size_t Branch(std::initializer_list<std::uint8_t> opcode);
    void PatchBranch(std::size_t offset, std::uint32_t targetRva);
    std::uint32_t GetRva() const;
    std::vector<std::uint8_t> TakeBytes();

private:
    std::uint32_t codeRva;
    std::vector<std::uint8_t> bytes;
};

}

#endif
