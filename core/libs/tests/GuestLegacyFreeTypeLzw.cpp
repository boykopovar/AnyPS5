#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libfreetype/include/GuestStream.hpp"

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

struct Counter {
    int allocations = 0;
    int frees = 0;
};

void Require(bool condition) {
    if (!condition) std::abort();
}

void* APS5_VABI Allocate(FreeType::GuestMemoryRec* memory, std::int64_t size) {
    ++static_cast<Counter*>(memory->user)->allocations;
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI Release(FreeType::GuestMemoryRec* memory, void* block) {
    ++static_cast<Counter*>(memory->user)->frees;
    std::free(block);
}

void* APS5_VABI Reallocate(FreeType::GuestMemoryRec*, std::int64_t, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

}

int main() {
    std::array<unsigned char, 7> compressed{0x1f, 0x9d, 0x90, 0x41, 0x84, 0x0c, 0x01};
    Counter counter;
    FreeType::GuestMemoryRec memory{&counter, Allocate, Release, Reallocate};
    LegacyFreeType::GuestStreamRec source{};
    source.base = compressed.data();
    source.size = compressed.size();
    source.memory = &memory;
    LegacyFreeType::GuestStreamRec output{};
    Require(FT_Stream_OpenLZW_nid_postfix(&output, &source) == 0);
    Require(output.read != nullptr && output.close != nullptr);
    std::array<unsigned char, 3> decoded{};
    Require(output.read(&output, 0, decoded.data(), decoded.size()) == decoded.size());
    Require(std::memcmp(decoded.data(), "ABC", decoded.size()) == 0);
    std::array<unsigned char, 2> middle{};
    Require(output.read(&output, 1, middle.data(), middle.size()) == middle.size());
    Require(std::memcmp(middle.data(), "BC", middle.size()) == 0);
    std::array<unsigned char, 4> tail{};
    tail.fill(0x55);
    Require(output.read(&output, 2, tail.data(), tail.size()) == 1);
    Require(tail[0] == 'C' && tail[1] == 0x55);
    Require(output.read(&output, 3, tail.data(), tail.size()) == 0);
    output.close(&output);
    Require(counter.allocations > 0 && counter.allocations == counter.frees);

    LegacyFreeType::GuestStreamRec emptySource{};
    emptySource.memory = &memory;
    LegacyFreeType::GuestStreamRec emptyOutput{};
    Require(FT_Stream_OpenLZW_nid_postfix(&emptyOutput, &emptySource) != 0);

    std::array<unsigned char, 3> corrupt{0x00, 0x00, 0x00};
    LegacyFreeType::GuestStreamRec corruptSource{};
    corruptSource.base = corrupt.data();
    corruptSource.size = corrupt.size();
    corruptSource.memory = &memory;
    LegacyFreeType::GuestStreamRec corruptOutput{};
    Require(FT_Stream_OpenLZW_nid_postfix(&corruptOutput, &corruptSource) != 0);
    Require(counter.allocations == counter.frees);
}
