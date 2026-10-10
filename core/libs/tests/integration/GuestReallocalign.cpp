#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <Testing/Test.hpp>

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>

extern "C" {
void* APS5_VABI reallocalign_nid_postfix(void*, std::size_t, std::size_t);
}

namespace {

using Testing::Case;
using Testing::Require;
using Testing::RequireEqual;

constexpr unsigned char pattern = 0x5a;

class HeapBlock {
public:
    HeapBlock() {
        void* api[10]{};
        ApplicationHeapRegister_nid_no_patch(api);
    }

    ~HeapBlock() { ApplicationHeapFree_nid_no_patch(pointer); }

    HeapBlock(const HeapBlock&) = delete;
    HeapBlock& operator=(const HeapBlock&) = delete;

    unsigned char* Realign(std::size_t bytes, std::size_t alignment) {
        void* result = reallocalign_nid_postfix(pointer, bytes, alignment);
        pointer = static_cast<unsigned char*>(result);
        return pointer;
    }

    unsigned char* Get() const { return pointer; }

private:
    unsigned char* pointer = nullptr;
};

void RequireAligned(const unsigned char* block, std::size_t alignment, const char* message) {
    Require(block != nullptr, std::string(message) + ": allocation");
    RequireEqual(reinterpret_cast<std::uintptr_t>(block) % alignment, std::uintptr_t{0}, std::string(message) + ": alignment");
}

void RequirePattern(const unsigned char* block, std::size_t bytes) {
    for (std::size_t index = 0; index < bytes; ++index) {
        RequireEqual(block[index], pattern, "byte " + std::to_string(index));
    }
}

void AllocatePattern(HeapBlock& block) {
    RequireAligned(block.Realign(64, 64), 64, "initial block");
    std::memset(block.Get(), pattern, 64);
}

const Case allocate{"Reallocalign_NullPointer_AllocatesAlignedBlock", [] {
    HeapBlock block;
    RequireAligned(block.Realign(64, 64), 64, "allocated block");
}};

const Case grow{"Reallocalign_Grow_KeepsAlignmentAndContents", [] {
    HeapBlock block;
    AllocatePattern(block);
    RequireAligned(block.Realign(256, 64), 64, "grown block");
    RequirePattern(block.Get(), 64);
}};

const Case shrink{"Reallocalign_Shrink_KeepsAlignmentAndPrefix", [] {
    HeapBlock block;
    AllocatePattern(block);
    RequireAligned(block.Realign(256, 64), 64, "grown block");
    RequireAligned(block.Realign(16, 16), 16, "shrunk block");
    RequirePattern(block.Get(), 16);
}};

const Case zeroAlignment{"Reallocalign_ZeroAlignment_ThrowsInvalidArgument", [] {
    HeapBlock block;
    AllocatePattern(block);
    Testing::RequireThrows<std::invalid_argument>([&] { reallocalign_nid_postfix(block.Get(), 16, 0); }, "alignment 0");
}};

const Case oddAlignment{"Reallocalign_NonPowerOfTwoAlignment_ThrowsInvalidArgument", [] {
    HeapBlock block;
    AllocatePattern(block);
    Testing::RequireThrows<std::invalid_argument>([&] { reallocalign_nid_postfix(block.Get(), 16, 3); }, "alignment 3");
}};

const Case zeroSize{"Reallocalign_ZeroSize_FreesAndReturnsNull", [] {
    HeapBlock block;
    AllocatePattern(block);
    Require(block.Realign(0, 16) == nullptr, "zero-size reallocalign returns null");
}};

} // namespace
