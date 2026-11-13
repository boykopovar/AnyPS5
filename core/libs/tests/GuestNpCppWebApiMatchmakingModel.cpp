#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

void Fill(Storage& storage) {
    for (std::size_t index = 0; index < kModelSize; ++index) {
        storage.bytes[index] = static_cast<unsigned char>(index);
    }
}

void RequirePlayerForRead() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead14accountIdIsSetEv(model) == 0x18);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead13onlineIdIsSetEv(model) == 0x2d);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getPlatformEv(model) == 0x33323130);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead13platformIsSetEv(model) == 0x34);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead13teamNameIsSetEv(model) == 0x58);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead21playerAttributesIsSetEv(model) == 0x80);

    storage.bytes[0x34] = 0;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead13platformIsSetEv(model) == 0);
    storage.bytes[0x30] = 0x7f;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getPlatformEv(model) == 0x3332317fu);
}

void RequirePlayerForOfferRead() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead14accountIdIsSetEv(model) == 0x18);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13onlineIdIsSetEv(model) == 0x2d);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getPlatformEv(model) == 0x33323130);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13platformIsSetEv(model) == 0x34);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13teamNameIsSetEv(model) == 0x58);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead13ticketIdIsSetEv(model) == 0x80);
}

void RequireAttribute() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V19Attribute7getTypeEv(model) == 0x33323130);
}

void RequireSubmitter() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter14accountIdIsSetEv(model) == 0x18);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter11getPlatformEv(model) == 0x1f1e1d1c);
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V19Submitter13platformIsSetEv(model) == 0x20);
}

void RequireLocation() {
    Storage storage{};
    Fill(storage);
    void* model = storage.bytes;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V18Location18gameSessionIdIsSetEv(model) == 0x30);
    storage.bytes[0x30] = 1;
    Require(_ZNK3sce2Np9CppWebApi11Matchmaking2V18Location18gameSessionIdIsSetEv(model) == 1);
}

}  // namespace

int main() {
    RequirePlayerForRead();
    RequirePlayerForOfferRead();
    RequireAttribute();
    RequireSubmitter();
    RequireLocation();
    return 0;
}
