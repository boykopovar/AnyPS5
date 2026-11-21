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

constexpr std::size_t kModelSize = 0x120;

struct Storage {
    alignas(8) unsigned char bytes[kModelSize];
};

void Fill(Storage& storage) {
    for (std::size_t index = 0; index < kModelSize; ++index) {
        storage.bytes[index] = static_cast<unsigned char>(index);
    }
}

void RequireGetOfferResponseBody() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody12offerIdIsSetEv(model) == static_cast<unsigned char>(0x50));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody16rulesetNameIsSetEv(model) == static_cast<unsigned char>(0x78));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody12playersIsSetEv(model) == static_cast<unsigned char>(0x98));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody13locationIsSetEv(model) == static_cast<unsigned char>(0xc0));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody11statusIsSetEv(model) == static_cast<unsigned char>(0xa0));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody9getStatusEv(model) == 0x9f9e9d9c);
}

void RequireGetTicketResponseBody() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody13ticketIdIsSetEv(model) == static_cast<unsigned char>(0x50));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody16rulesetNameIsSetEv(model) == static_cast<unsigned char>(0x78));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody21ticketAttributesIsSetEv(model) == static_cast<unsigned char>(0x98));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody12playersIsSetEv(model) == static_cast<unsigned char>(0xb8));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody11statusIsSetEv(model) == static_cast<unsigned char>(0xc0));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody14submitterIsSetEv(model) == static_cast<unsigned char>(0x108));
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody9getStatusEv(model) == 0xbfbebdbc);
}

void RequireSubmitTicketResponseBody() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V124SubmitTicketResponseBody13ticketIdIsSetEv(model) == static_cast<unsigned char>(0x50));
}

void RequireStatusTracksMemory() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    storage.bytes[0x9c] = 0;
    storage.bytes[0x9d] = 0;
    storage.bytes[0x9e] = 0;
    storage.bytes[0x9f] = 0x01;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody9getStatusEv(model) == 0x01000000);
    storage.bytes[0xa0] = 0;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody11statusIsSetEv(model) == 0);
}

}  // namespace

int main() {
    RequireGetOfferResponseBody();
    RequireGetTicketResponseBody();
    RequireSubmitTicketResponseBody();
    RequireStatusTracksMemory();
    return 0;
}
