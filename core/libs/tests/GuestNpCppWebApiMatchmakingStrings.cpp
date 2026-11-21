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

void Fill(Storage& storage, unsigned char value) {
    for (std::size_t index = 0; index < kModelSize; ++index) {
        storage.bytes[index] = value;
    }
}

void RequireAt(const CommonString* value, const Storage& storage, std::size_t offset) {
    Require(reinterpret_cast<const unsigned char*>(value) == storage.bytes + offset);
}

void RequirePlayerForRead() {
    Storage storage{};
    Fill(storage, 0xa5);
    void* model = storage.bytes;
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getTeamNameEv(model), storage, 0x38);
}

void RequirePlayerForOfferRead() {
    Storage storage{};
    Fill(storage, 0xa5);
    void* model = storage.bytes;
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getTeamNameEv(model), storage, 0x38);
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getTicketIdEv(model), storage, 0x60);
}

void RequireGetOfferResponseBody() {
    Storage storage{};
    Fill(storage, 0xa5);
    void* model = storage.bytes;
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody10getOfferIdEv(model), storage, 0x30);
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody14getRulesetNameEv(model), storage, 0x58);
}

void RequireGetTicketResponseBody() {
    Storage storage{};
    Fill(storage, 0xa5);
    void* model = storage.bytes;
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody11getTicketIdEv(model), storage, 0x30);
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody14getRulesetNameEv(model), storage, 0x58);
}

void RequireLocation() {
    Storage storage{};
    Fill(storage, 0xa5);
    void* model = storage.bytes;
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V18Location16getGameSessionIdEv(model), storage, 0x10);
}

void RequireAttribute() {
    Storage storage{};
    Fill(storage, 0xa5);
    void* model = storage.bytes;
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V19Attribute7getNameEv(model), storage, 0x10);
    RequireAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V19Attribute8getValueEv(model), storage, 0x38);
}

void RequireTargetsAreReadable() {
    Storage storage{};
    Fill(storage, 0);
    void* model = storage.bytes;
    storage.bytes[0x38] = 7;
    storage.bytes[0x39] = 0;
    storage.bytes[0x3a] = 0;
    storage.bytes[0x3b] = 0;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getTeamNameEv(model)->referenceCount == 7);
    storage.bytes[0x10] = 9;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V18Location16getGameSessionIdEv(model)->referenceCount == 9);
    storage.bytes[0x60] = 11;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getTicketIdEv(model)->referenceCount == 11);
}

}  // namespace

int main() {
    RequirePlayerForRead();
    RequirePlayerForOfferRead();
    RequireGetOfferResponseBody();
    RequireGetTicketResponseBody();
    RequireLocation();
    RequireAttribute();
    RequireTargetsAreReadable();
    return 0;
}
