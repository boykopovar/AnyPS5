#include "prx/libfreetype/libfreetype.hpp"
#include "prx/libfreetype/include/GuestStream.hpp"

#include <array>
#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace {

constexpr int UnimplementedFeature = 0x07;

int allocations = 0;

void Require(bool condition) {
    if (!condition) std::abort();
}

void* APS5_VABI Allocate(FreeType::GuestMemoryRec*, std::int64_t size) {
    ++allocations;
    return std::malloc(static_cast<std::size_t>(size));
}

void APS5_VABI Release(FreeType::GuestMemoryRec*, void* block) {
    std::free(block);
}

void* APS5_VABI Reallocate(FreeType::GuestMemoryRec*, std::int64_t, std::int64_t size, void* block) {
    return std::realloc(block, static_cast<std::size_t>(size));
}

}

int main() {
    std::array<unsigned char, 10> compressed{'B', 'Z', 'h', '9', 0x17, 0x72, 0x45, 0x38, 0x50, 0x90};
    FreeType::GuestMemoryRec memory{nullptr, Allocate, Release, Reallocate};
    LegacyFreeType::GuestStreamRec source{};
    source.base = compressed.data();
    source.size = compressed.size();
    source.memory = &memory;
    LegacyFreeType::GuestStreamRec output{};
    std::memset(&output, 0x5a, sizeof(output));
    LegacyFreeType::GuestStreamRec untouched{};
    std::memcpy(&untouched, &output, sizeof(output));
    Require(FT_Stream_OpenBzip2_nid_postfix(&output, &source) == UnimplementedFeature);
    Require(std::memcmp(&output, &untouched, sizeof(output)) == 0);
    Require(source.pos == 0 && allocations == 0);
    Require(FT_Stream_OpenBzip2_nid_postfix(nullptr, &source) == UnimplementedFeature);
    Require(FT_Stream_OpenBzip2_nid_postfix(&output, nullptr) == UnimplementedFeature);
    Require(FT_Stream_OpenBzip2_nid_postfix(nullptr, nullptr) == UnimplementedFeature);
    LegacyFreeType::GuestStreamRec original{};
    std::memcpy(&original, &source, sizeof(source));
    Require(FT_Stream_OpenBzip2_nid_postfix(&source, &source) == UnimplementedFeature);
    Require(std::memcmp(&source, &original, sizeof(source)) == 0);
    source.memory = nullptr;
    Require(FT_Stream_OpenBzip2_nid_postfix(&output, &source) == UnimplementedFeature);
    Require(std::memcmp(&output, &untouched, sizeof(output)) == 0 && source.pos == 0 && allocations == 0);
    errno = 71;
    Require(FT_Stream_OpenBzip2_nid_postfix(reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10), &source) == UnimplementedFeature);
    Require(errno == 71);
    errno = 71;
    Require(FT_Stream_OpenBzip2_nid_postfix(&output, reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10)) == UnimplementedFeature);
    Require(errno == 71);
    errno = 71;
    Require(FT_Stream_OpenBzip2_nid_postfix(reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10), reinterpret_cast<LegacyFreeType::GuestStreamRec*>(0x10)) == UnimplementedFeature);
    Require(errno == 71);
}
