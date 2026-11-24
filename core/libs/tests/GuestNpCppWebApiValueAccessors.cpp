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

constexpr std::size_t kModelSize = 0x200;

struct Storage {
    alignas(8) unsigned char bytes[kModelSize];
};

struct Object {
    alignas(8) unsigned char bytes[0x40];
};

void Fill(Storage& storage, unsigned char value) {
    for (std::size_t index = 0; index < kModelSize; ++index) {
        storage.bytes[index] = value;
    }
}

template <typename Reader>
void RequireMemberCopy(Reader reader, std::size_t memberOffset, std::size_t referenceCountOffset) {
    Storage storage{};
    Fill(storage, 0);
    Object object{};
    *reinterpret_cast<std::int32_t*>(object.bytes + referenceCountOffset) = 1;
    auto* member = reinterpret_cast<IntrusivePtr*>(storage.bytes + memberOffset);
    member->object = object.bytes;
    member->deleter = nullptr;
    member->libContext = storage.bytes + 0x1f0;
    IntrusivePtr out{};
    Require(reader(&out, storage.bytes) == &out);
    Require(out.object == object.bytes);
    Require(out.deleter == nullptr);
    Require(out.libContext == storage.bytes + 0x1f0);
    Require(*reinterpret_cast<const std::int32_t*>(object.bytes + referenceCountOffset) == 2);

    member->object = nullptr;
    IntrusivePtr cleared{};
    Require(reader(&cleared, storage.bytes) == &cleared);
    Require(cleared.object == nullptr);
    Require(*reinterpret_cast<const std::int32_t*>(object.bytes + referenceCountOffset) == 2);
}

template <typename Reader>
void RequireOnlineIdAt(Reader reader, std::size_t base) {
    Storage storage{};
    Fill(storage, 0xa5);
    storage.bytes[base - 1] = 0x11;
    for (std::size_t index = 0; index < 0x14; ++index) {
        storage.bytes[base + index] = 0x5a;
    }
    unsigned char out[0x20];
    for (std::size_t index = 0; index < sizeof(out); ++index) {
        out[index] = 0;
    }
    Require(reader(out, storage.bytes) == out);
    Require(out[0] == 0x5a);
    Require(out[0x13] == 0x5a);
    Require(out[0x14] == 0);
}

template <typename Reader>
void RequireFlag(Reader reader, std::size_t offset) {
    Storage storage{};
    Fill(storage, 0xa5);
    Require(reader(storage.bytes));
    storage.bytes[offset] = 0;
    Require(!reader(storage.bytes));
    storage.bytes[offset] = 1;
    Require(reader(storage.bytes));
}

void RequireVectorMembers() {
    RequireMemberCopy(_ZN3sce2Np9CppWebApi10Activities2V114UserActivities14UserActivities13getActivitiesEv, 0x30, 0x30);
    RequireMemberCopy(_ZN3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead19getPlayerAttributesEv, 0x68, 0x30);
    RequireMemberCopy(_ZN3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody10getPlayersEv, 0x80, 0x30);
    RequireMemberCopy(_ZN3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody10getPlayersEv, 0xa0, 0x30);
    RequireMemberCopy(_ZN3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody19getTicketAttributesEv, 0x80, 0x30);
    RequireMemberCopy(_ZN3sce2Np9CppWebApi14SessionManager2V154GetUsersAccountIdPlayerSessionsInvitationsResponseBody14getInvitationsEv, 0x30, 0x30);
}

void RequireFlags() {
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V154GetUsersAccountIdPlayerSessionsInvitationsResponseBody16invitationsIsSetEv, 0x48);
}

void RequireScalarMembers() {
    RequireMemberCopy(_ZNK3sce2Np9CppWebApi11Matchmaking2V120GetOfferResponseBody11getLocationEv, 0xa8, 0x00);
    RequireMemberCopy(_ZNK3sce2Np9CppWebApi11Matchmaking2V121GetTicketResponseBody12getSubmitterEv, 0xf0, 0x00);
    RequireMemberCopy(_ZNK3sce2Np9CppWebApi17TitleCloudStorage2V110DataStatus8getOwnerEv, 0x98, 0x00);
}

void RequireOnlineIds() {
    RequireOnlineIdAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V113PlayerForRead11getOnlineIdEv, 0x19);
    RequireOnlineIdAt(_ZNK3sce2Np9CppWebApi11Matchmaking2V118PlayerForOfferRead11getOnlineIdEv, 0x19);
}

void RequireValues() {
    Storage storage{};
    Fill(storage, 0);
    void* model = storage.bytes;

    const std::uint64_t updated = 0x1122334455667788ull;
    for (std::size_t index = 0; index < sizeof(updated); ++index) {
        storage.bytes[0x50 + index] = static_cast<unsigned char>(updated >> (index * 8));
    }
    storage.bytes[0x4f] = 0xff;
    storage.bytes[0x58] = 0xff;
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V122GetRankingResponseBody22getLastUpdatedDateTimeEv(model) == updated);

}

void RequireAtomicOperationId() {
    Storage storage{};
    Fill(storage, 0);
    const char* stored = "operation";
    *reinterpret_cast<const char**>(storage.bytes + 0x10) = stored;
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V19RecordApi26RecordScoreResponseHeaders24getXPsnAtomicOperationIdEv(storage.bytes) == stored);
    storage.bytes[0x0f] = 0xff;
    storage.bytes[0x18] = 0xff;
    Require(_ZNK3sce2Np9CppWebApi12Leaderboards2V19RecordApi26RecordScoreResponseHeaders24getXPsnAtomicOperationIdEv(storage.bytes) == stored);

    *reinterpret_cast<const char**>(storage.bytes + 0x10) = nullptr;
    const char* fallback = _ZNK3sce2Np9CppWebApi12Leaderboards2V19RecordApi26RecordScoreResponseHeaders24getXPsnAtomicOperationIdEv(storage.bytes);
    Require(fallback != nullptr);
    Require(fallback[0] == '\0');
}

}

int main() {
    RequireVectorMembers();
    RequireFlags();
    RequireScalarMembers();
    RequireOnlineIds();
    RequireValues();
    RequireAtomicOperationId();
    return 0;
}
