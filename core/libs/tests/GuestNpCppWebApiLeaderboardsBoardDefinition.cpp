#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <source_location>

namespace {

void Require(bool value, std::source_location at = std::source_location::current()) {
    if (!value) {
        std::fprintf(stderr, "requirement failed at %s:%u\n", at.file_name(), at.line());
        std::abort();
    }
}

constexpr std::size_t kModelSize = 0x90;

struct Storage {
    alignas(8) unsigned char bytes[kModelSize];
};

void Put32(Storage& storage, std::size_t offset, std::uint32_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        storage.bytes[offset + index] = static_cast<unsigned char>(value >> (index * 8));
    }
}

void Put64(Storage& storage, std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        storage.bytes[offset + index] = static_cast<unsigned char>(value >> (index * 8));
    }
}

void Fill(Storage& storage) {
    for (std::size_t index = 0; index < kModelSize; ++index) {
        storage.bytes[index] = 0xa5;
    }
}

void RequireLimits() {
    Storage storage{};
    Fill(storage);
    const void* model = storage.bytes;
    Put32(storage, 0x30, 0xdeadbeef);
    storage.bytes[0x34] = 1;
    Put32(storage, 0x38, 0x11223344);
    storage.bytes[0x3c] = 0;
    Put64(storage, 0x40, 0x0123456789abcdefull);
    storage.bytes[0x48] = 1;
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody13getEntryLimitEv(model) == 0xdeadbeef);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody15entryLimitIsSetEv(model));
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody20getLargeDataNumLimitEv(model) == 0x11223344);
    Require(!_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody22largeDataNumLimitIsSetEv(model));
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody21getLargeDataSizeLimitEv(model) == 0x0123456789abcdefull);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody23largeDataSizeLimitIsSetEv(model));
}

void RequireScoreLimits() {
    Storage storage{};
    Fill(storage);
    const void* model = storage.bytes;
    Put64(storage, 0x50, 0xfffffffffffffffeull);
    storage.bytes[0x58] = 1;
    Put64(storage, 0x60, 0xfffffffffffffffdull);
    storage.bytes[0x68] = 0;
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody16getMaxScoreLimitEv(model) == -2);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody18maxScoreLimitIsSetEv(model));
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody16getMinScoreLimitEv(model) == -3);
    Require(!_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody18minScoreLimitIsSetEv(model));
}

void RequireWidthsDoNotOverlap() {
    Storage storage{};
    Fill(storage);
    const void* model = storage.bytes;
    Put32(storage, 0x30, 0x00000001);
    Put32(storage, 0x34, 0xffffffff);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody13getEntryLimitEv(model) == 1);
    Put32(storage, 0x38, 0x00000002);
    Put32(storage, 0x3c, 0xffffffff);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody20getLargeDataNumLimitEv(model) == 2);
    Put64(storage, 0x40, 0x0000000000000004ull);
    Put64(storage, 0x48, 0xffffffffffffffffull);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody21getLargeDataSizeLimitEv(model) == 4);
    Put64(storage, 0x50, 0x0000000000000007ull);
    Put64(storage, 0x58, 0xffffffffffffffffull);
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody16getMaxScoreLimitEv(model) == 7);
}

void RequireFlagsTrackMemory() {
    Storage storage{};
    Fill(storage);
    const void* model = storage.bytes;
    storage.bytes[0x34] = 0;
    Require(!_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody15entryLimitIsSetEv(model));
    storage.bytes[0x34] = 0x80;
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V130GetBoardDefinitionResponseBody15entryLimitIsSetEv(model));
}

}  // namespace

int main() {
    RequireLimits();
    RequireScoreLimits();
    RequireWidthsDoNotOverlap();
    RequireFlagsTrackMemory();
    return 0;
}
