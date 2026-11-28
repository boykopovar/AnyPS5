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

constexpr std::size_t kModelSize = 0x160;

struct Storage {
    alignas(8) unsigned char bytes[kModelSize];
};

void Fill(Storage& storage) {
    for (std::size_t index = 0; index < kModelSize; ++index) {
        storage.bytes[index] = static_cast<unsigned char>(index);
    }
}

void Put64(Storage& storage, std::size_t offset, std::uint64_t value) {
    for (std::size_t index = 0; index < sizeof(value); ++index) {
        storage.bytes[offset + index] = static_cast<unsigned char>(value >> (index * 8));
    }
}

void RequireGetOfferResponseBody() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Put64(storage, 0xe8, 0x1122334455667788ull);
    Put64(storage, 0xf8, 0x99aabbccddeeff00ull);
    storage.bytes[0xf0] = 1;
    storage.bytes[0x100] = 1;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody18getCreatedDateTimeEv(model) == 0x1122334455667788ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody18getUpdatedDateTimeEv(model) == 0x99aabbccddeeff00ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody20createdDateTimeIsSetEv(model) == 1);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody20updatedDateTimeIsSetEv(model) == 1);
}

void RequireGetTicketResponseBody() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Put64(storage, 0x130, 0x0f1e2d3c4b5a6978ull);
    Put64(storage, 0x140, 0xf0e0d0c0b0a09080ull);
    storage.bytes[0x138] = 1;
    storage.bytes[0x148] = 1;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody18getCreatedDateTimeEv(model) == 0x0f1e2d3c4b5a6978ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody18getUpdatedDateTimeEv(model) == 0xf0e0d0c0b0a09080ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody20createdDateTimeIsSetEv(model) == 1);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody20updatedDateTimeIsSetEv(model) == 1);
}

void RequireValuesFollowMemory() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Put64(storage, 0xe8, 0);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody18getCreatedDateTimeEv(model) == 0);
    Put64(storage, 0xe8, 0xffffffff00000001ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody18getCreatedDateTimeEv(model) == 0xffffffff00000001ull);
    storage.bytes[0xf0] = 0;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody20createdDateTimeIsSetEv(model) == 0);
    storage.bytes[0xf0] = 0x80;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody20createdDateTimeIsSetEv(model) == 1);
}

void RequireNeighbouringFieldsDoNotLeak() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Put64(storage, 0xe0, 0xa0a0a0a0a0a0a0a0ull);
    Put64(storage, 0xf0, 0xb0b0b0b0b0b0b0b0ull);
    Put64(storage, 0xe8, 0x0102030405060708ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody18getCreatedDateTimeEv(model) == 0x0102030405060708ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody20createdDateTimeIsSetEv(model) == 1);
    Put64(storage, 0x128, 0xc0c0c0c0c0c0c0c0ull);
    Put64(storage, 0x138, 0xd0d0d0d0d0d0d0d0ull);
    Put64(storage, 0x130, 0x1010101010101010ull);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody18getCreatedDateTimeEv(model) == 0x1010101010101010ull);
}

}  // namespace

int main() {
    RequireGetOfferResponseBody();
    RequireGetTicketResponseBody();
    RequireValuesFollowMemory();
    RequireNeighbouringFieldsDoNotLeak();
    return 0;
}
