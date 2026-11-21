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

void RequireAt(const CommonString* value, const Storage& storage, std::size_t offset) {
    Require(reinterpret_cast<const unsigned char*>(value) == storage.bytes + offset);
}

template <typename Reader>
void RequireFlag(Reader reader, const Storage& storage, std::size_t offset) {
    Storage probed = storage;
    probed.bytes[offset] = 0;
    Require(!reader(probed.bytes));
    probed.bytes[offset] = 1;
    Require(reader(probed.bytes));
    probed.bytes[offset] = 0xa5;
    Require(reader(probed.bytes));
}

template <typename Reader>
void RequireNoFlagEarlier(Reader reader, const Storage& storage, std::size_t offset) {
    Storage probed = storage;
    for (std::size_t index = 0; index < offset; ++index) {
        probed.bytes[index] = 0xff;
    }
    probed.bytes[offset] = 0;
    Require(!reader(probed.bytes));
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

void RequirePlatforms() {
    Storage storage{};
    Fill(storage, 0xa5);
    void* model = storage.bytes;
    RequireAt(_ZNK3sce2Np9CppWebApi14SessionManager2V114Representative11getPlatformEv(model), storage, 0x30);
    RequireAt(_ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer11getPlatformEv(model), storage, 0x30);
    RequireAt(_ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead12getSessionIdEv(model), storage, 0x10);
    RequireAt(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead12getSessionIdEv(model), storage, 0x10);
    RequireAt(_ZNK3sce2Np9CppWebApi14SessionManager2V131ResponsePlayerSessionInvitation15getInvitationIdEv(model), storage, 0x10);
    RequireAt(_ZNK3sce2Np9CppWebApi14SessionManager2V136UsersPlayerSessionsInvitationForRead12getSessionIdEv(model), storage, 0x78);
}

void RequireScalars() {
    Storage storage{};
    Fill(storage, 0);
    void* model = storage.bytes;

    const std::uint64_t accountId = 0x1122334455667788ull;
    for (std::size_t index = 0; index < sizeof(accountId); ++index) {
        storage.bytes[0x10 + index] = static_cast<unsigned char>(accountId >> (index * 8));
    }
    storage.bytes[0x0f] = 0xff;
    storage.bytes[0x18] = 0xff;
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer12getAccountIdEv(model) == accountId);

    const std::uint32_t joinState = 0x8000'0001u;
    for (std::size_t index = 0; index < sizeof(joinState); ++index) {
        storage.bytes[0x78 + index] = static_cast<unsigned char>(joinState >> (index * 8));
    }
    storage.bytes[0x77] = 0xff;
    storage.bytes[0x7c] = 0xff;
    Require(static_cast<std::uint32_t>(_ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer12getJoinStateEv(model)) == joinState);

    const std::uint32_t joinableUserType = 0x0000'0003u;
    storage.bytes[0x134] = 0x03;
    storage.bytes[0x133] = 0xff;
    storage.bytes[0x138] = 0xff;
    Require(static_cast<std::uint32_t>(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead19getJoinableUserTypeEv(model)) == joinableUserType);
}

void RequireFlags() {
    Storage storage{};
    Fill(storage, 0xa5);

    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V117GameSessionPlayer14joinStateIsSetEv, storage, 0x7c);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead11memberIsSetEv, storage, 0x88);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead14sessionIdIsSetEv, storage, 0x30);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead21joinableUserTypeIsSetEv, storage, 0x138);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V124GameSessionMemberForRead12playersIsSetEv, storage, 0x28);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBody16invitationsIsSetEv, storage, 0x48);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V136UsersPlayerSessionsInvitationForRead14sessionIdIsSetEv, storage, 0x98);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V136UsersPlayerSessionsInvitationForRead17invitationIdIsSetEv, storage, 0x30);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V136UsersPlayerSessionsInvitationForRead22receivedTimestampIsSetEv, storage, 0xe0);
    RequireFlag(_ZNK3sce2Np9CppWebApi14SessionManager2V136UsersPlayerSessionsInvitationForRead9fromIsSetEv, storage, 0x50);

    RequireNoFlagEarlier(_ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead11memberIsSetEv, storage, 0x88);
    RequireNoFlagEarlier(_ZNK3sce2Np9CppWebApi14SessionManager2V120PlayerSessionForRead21joinableUserTypeIsSetEv, storage, 0x138);
    RequireNoFlagEarlier(_ZNK3sce2Np9CppWebApi14SessionManager2V136UsersPlayerSessionsInvitationForRead22receivedTimestampIsSetEv, storage, 0xe0);
}

void RequireMembers() {
    RequireMemberCopy(_ZN3sce2Np9CppWebApi14SessionManager2V124GameSessionMemberForRead10getPlayersEv, 0x10, 0x30);
    RequireMemberCopy(_ZN3sce2Np9CppWebApi14SessionManager2V150PostPlayerSessionsSessionIdInvitationsResponseBody14getInvitationsEv, 0x30, 0x30);
    RequireMemberCopy(_ZNK3sce2Np9CppWebApi14SessionManager2V118GameSessionForRead9getMemberEv, 0x70, 0x00);
    RequireMemberCopy(_ZNK3sce2Np9CppWebApi14SessionManager2V136UsersPlayerSessionsInvitationForRead7getFromEv, 0x38, 0x00);
}

void RequireOnlineId() {
    Storage storage{};
    Fill(storage, 0xa5);
    unsigned char out[0x20];
    for (std::size_t index = 0; index < sizeof(out); ++index) {
        out[index] = 0;
    }
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V110FromMember11getOnlineIdEv(out, storage.bytes) == out);
    for (std::size_t index = 0; index < 0x14; ++index) {
        Require(out[index] == 0xa5);
    }
    for (std::size_t index = 0x14; index < sizeof(out); ++index) {
        Require(out[index] == 0);
    }
}

}

int main() {
    RequirePlatforms();
    RequireScalars();
    RequireFlags();
    RequireMembers();
    RequireOnlineId();
    return 0;
}
