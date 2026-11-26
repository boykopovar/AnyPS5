#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "NpManager value class guest pointer check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Function>
static bool FaultsAsNotImplemented(Function function, const char* name) {
    try {
        static_cast<void>(function());
    } catch (const std::runtime_error& error) {
        return std::string(error.what()) == std::string(name) + " not implemented";
    }
    return false;
}

static unsigned char* MapPage() {
#ifdef _WIN32
    void* page = VirtualAlloc(nullptr, 4096, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    Require(page != nullptr);
#else
    void* page = mmap(nullptr, 4096, PROT_READ | PROT_WRITE, MAP_ANON | MAP_PRIVATE, -1, 0);
    Require(page != MAP_FAILED);
#endif
    return static_cast<unsigned char*>(page);
}

static void ProtectReadOnly(void* page) {
#ifdef _WIN32
    DWORD previous = 0;
    Require(VirtualProtect(page, 4096, PAGE_READONLY, &previous) != 0);
#else
    Require(mprotect(page, 4096, PROT_READ) == 0);
#endif
}

static void UnmapPage(void* page) {
#ifdef _WIN32
    Require(VirtualFree(page, 0, MEM_RELEASE) != 0);
#else
    Require(munmap(page, 4096) == 0);
#endif
}

template <typename T>
static bool PaddingKept(const T& object, std::size_t fieldBytes) {
    const auto* bytes = reinterpret_cast<const unsigned char*>(&object);
    for (std::size_t index = fieldBytes; index < sizeof(T); ++index) {
        if (bytes[index] != 0xa5) return false;
    }
    return true;
}

int main() {
    NpOnlineIdObject online;
    std::memset(&online, 0xa5, sizeof(online));
    _ZN3sce2np10NpOnlineIdC1Ev(&online);
    Require(online.vtable != nullptr && online.data[0] == 0 && online.data[15] == 0 && online.term == 0);
    Require(PaddingKept(online, offsetof(NpOnlineIdObject, term) + sizeof(online.term)));
    Require(_ZNK3sce2np10NpOnlineId7IsEmptyEv(&online));
    SceNpOnlineId plain{};
    std::memcpy(plain.data, "player-one", 10);
    NpOnlineIdObject fromPlain;
    std::memset(&fromPlain, 0xa5, sizeof(fromPlain));
    _ZN3sce2np10NpOnlineIdC2ERK13SceNpOnlineId(&fromPlain, &plain);
    Require(fromPlain.vtable == online.vtable && std::memcmp(fromPlain.data, "player-one", 10) == 0 && fromPlain.data[10] == 0 && fromPlain.term == 0);
    Require(!_ZNK3sce2np10NpOnlineId7IsEmptyEv(&fromPlain));
    NpOnlineIdObject copied;
    std::memset(&copied, 0xa5, sizeof(copied));
    _ZN3sce2np10NpOnlineIdC1ERKS1_(&copied, &fromPlain);
    Require(copied.vtable == online.vtable && std::memcmp(copied.data, fromPlain.data, sizeof(copied.data)) == 0);
    Require(PaddingKept(copied, offsetof(NpOnlineIdObject, term) + sizeof(copied.term)));
    Require(_ZN3sce2npeqERKNS0_10NpOnlineIdES3_(&copied, &fromPlain) && !_ZN3sce2npneERKNS0_10NpOnlineIdES3_(&copied, &fromPlain));
    Require(_ZN3sce2npeqERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(&plain, &copied) && !_ZN3sce2npneERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(&plain, &copied));
    Require(_ZN3sce2npeqERKNS0_10NpOnlineIdERK13SceNpOnlineId(&copied, &plain) && !_ZN3sce2npneERKNS0_10NpOnlineIdERK13SceNpOnlineId(&copied, &plain));
    Require(!_ZN3sce2npeqERKNS0_10NpOnlineIdES3_(&copied, &online) && _ZN3sce2npneERKNS0_10NpOnlineIdES3_(&copied, &online));
    _ZN3sce2np10NpOnlineId5ClearEv(&copied);
    Require(copied.data[0] == 0 && copied.term == 0 && copied.vtable == online.vtable && _ZNK3sce2np10NpOnlineId7IsEmptyEv(&copied));

    NpIdObject npId;
    std::memset(&npId, 0xa5, sizeof(npId));
    _ZN3sce2np4NpIdC1Ev(&npId);
    Require(npId.vtable != nullptr && npId.id.opt[0] == 0 && npId.id.reserved[7] == 0);
    Require(PaddingKept(npId, offsetof(NpIdObject, id) + sizeof(npId.id)));
    SceNpId plainId{};
    plainId.opt[3] = 7;
    NpIdObject fromPlainId;
    std::memset(&fromPlainId, 0xa5, sizeof(fromPlainId));
    _ZN3sce2np4NpIdC2ERK7SceNpId(&fromPlainId, &plainId);
    Require(fromPlainId.vtable == npId.vtable && fromPlainId.id.opt[3] == 7);
    NpIdObject copiedId;
    _ZN3sce2np4NpIdC2ERKS1_(&copiedId, &fromPlainId);
    Require(copiedId.vtable == npId.vtable && copiedId.id.opt[3] == 7);
    _ZN3sce2np4NpId5ClearEv(&copiedId);
    Require(copiedId.id.opt[3] == 0 && copiedId.vtable == npId.vtable);

    UserObject user;
    std::memset(&user, 0xa5, sizeof(user));
    _ZN3sce2np4UserC1Ev(&user);
    Require(user.vtable != nullptr && user.userId == USER_SERVICE_USER_ID_INVALID);
    Require(PaddingKept(user, offsetof(UserObject, userId) + sizeof(user.userId)));
    UserObject someone;
    _ZN3sce2np4UserC2Ei(&someone, 1000);
    Require(someone.vtable == user.vtable && someone.userId == 1000);
    UserObject sameone;
    _ZN3sce2np4UserC1ERKS1_(&sameone, &someone);
    Require(sameone.vtable == user.vtable && sameone.userId == 1000);
    const int thousand = 1000;
    Require(_ZN3sce2npeqERKNS0_4UserES3_(&sameone, &someone) && !_ZN3sce2npneERKNS0_4UserES3_(&sameone, &someone));
    Require(_ZN3sce2npeqERKNS0_4UserERKi(&sameone, &thousand) && !_ZN3sce2npneERKNS0_4UserERKi(&sameone, &thousand));
    Require(!_ZN3sce2npeqERKNS0_4UserES3_(&user, &someone) && _ZN3sce2npneERKNS0_4UserERKi(&user, &thousand));

    auto* title = new NpTitleTokenObject;
    std::memset(title, 0xa5, sizeof(*title));
    _ZN3sce2np12NpTitleTokenC1Ev(title);
    Require(title->vtable != nullptr && title->data[0] == 0 && title->data[sizeof(title->data) - 1] == 0);
    std::memcpy(title->data, "token", 6);
    auto* titleCopy = new NpTitleTokenObject;
    _ZN3sce2np12NpTitleTokenC2ERKS1_(titleCopy, title);
    Require(titleCopy->vtable == title->vtable && std::strcmp(_ZNK3sce2np12NpTitleToken6GetStrEv(titleCopy), "token") == 0);
    _ZN3sce2np12NpTitleToken5ClearEv(titleCopy);
    Require(titleCopy->data[0] == 0 && titleCopy->vtable == title->vtable);

    auto* access = new NpAccessTokenObject;
    std::memset(access, 0xa5, sizeof(*access));
    _ZN3sce2np13NpAccessTokenC1Ev(access);
    Require(access->vtable != nullptr && _ZNK3sce2np13NpAccessToken7IsEmptyEv(access));
    SceNpAccessToken plainToken{};
    plainToken.data[5] = 9;
    auto* fromToken = new NpAccessTokenObject;
    _ZN3sce2np13NpAccessTokenC2ERK16SceNpAccessToken(fromToken, &plainToken);
    Require(fromToken->vtable == access->vtable && fromToken->data[5] == 9 && !_ZNK3sce2np13NpAccessToken7IsEmptyEv(fromToken));
    auto* accessCopy = new NpAccessTokenObject;
    _ZN3sce2np13NpAccessTokenC1ERKS1_(accessCopy, fromToken);
    Require(accessCopy->vtable == access->vtable && accessCopy->data[5] == 9);
    _ZN3sce2np13NpAccessToken5ClearEv(accessCopy);
    Require(_ZNK3sce2np13NpAccessToken7IsEmptyEv(accessCopy));

    unsigned char ticketBytes[4] = {1, 2, 3, 4};
    NpTicketObject ticket{};
    ticket.data = ticketBytes;
    ticket.size = sizeof(ticketBytes);
    Require(_ZNK3sce2np8NpTicket13GetTicketDataEv(&ticket) == ticketBytes && _ZNK3sce2np8NpTicket13GetTicketSizeEv(&ticket) == 4);

    auto* badOnline = reinterpret_cast<NpOnlineIdObject*>(std::uintptr_t{0x10});
    auto* badPlain = reinterpret_cast<const SceNpOnlineId*>(std::uintptr_t{0x10});
    auto* badNpId = reinterpret_cast<NpIdObject*>(std::uintptr_t{0x10});
    auto* badPlainId = reinterpret_cast<const SceNpId*>(std::uintptr_t{0x10});
    auto* badUser = reinterpret_cast<UserObject*>(std::uintptr_t{0x10});
    auto* badInt = reinterpret_cast<const int*>(std::uintptr_t{0x10});
    auto* badTitle = reinterpret_cast<NpTitleTokenObject*>(std::uintptr_t{0x10});
    auto* badAccess = reinterpret_cast<NpAccessTokenObject*>(std::uintptr_t{0x10});
    auto* badPlainToken = reinterpret_cast<const SceNpAccessToken*>(std::uintptr_t{0x10});
    auto* badTicket = reinterpret_cast<const NpTicketObject*>(std::uintptr_t{0x10});

    Require(FaultsAsNotImplemented([&] { _ZN3sce2np10NpOnlineIdC1Ev(badOnline); return 0; }, "_ZN3sce2np10NpOnlineIdC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np10NpOnlineIdC1ERKS1_(&copied, badOnline); return 0; }, "_ZN3sce2np10NpOnlineIdC1ERKS1_"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np10NpOnlineIdC1ERKS1_(badOnline, &fromPlain); return 0; }, "_ZN3sce2np10NpOnlineIdC1ERKS1_"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np10NpOnlineIdC1ERK13SceNpOnlineId(&copied, badPlain); return 0; }, "_ZN3sce2np10NpOnlineIdC1ERK13SceNpOnlineId"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np10NpOnlineId5ClearEv(badOnline); return 0; }, "_ZN3sce2np10NpOnlineId5ClearEv"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npeqERKNS0_10NpOnlineIdES3_(badOnline, &online); }, "_ZN3sce2npeqERKNS0_10NpOnlineIdES3_"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npeqERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(badPlain, &online); }, "_ZN3sce2npeqERK13SceNpOnlineIdRKNS0_10NpOnlineIdE"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npeqERKNS0_10NpOnlineIdERK13SceNpOnlineId(&online, badPlain); }, "_ZN3sce2npeqERKNS0_10NpOnlineIdERK13SceNpOnlineId"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npneERKNS0_10NpOnlineIdES3_(&online, badOnline); }, "_ZN3sce2npneERKNS0_10NpOnlineIdES3_"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npneERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(&plain, badOnline); }, "_ZN3sce2npneERK13SceNpOnlineIdRKNS0_10NpOnlineIdE"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npneERKNS0_10NpOnlineIdERK13SceNpOnlineId(badOnline, &plain); }, "_ZN3sce2npneERKNS0_10NpOnlineIdERK13SceNpOnlineId"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4NpIdC1Ev(badNpId); return 0; }, "_ZN3sce2np4NpIdC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4NpIdC1ERKS1_(&copiedId, badNpId); return 0; }, "_ZN3sce2np4NpIdC1ERKS1_"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4NpIdC1ERK7SceNpId(&copiedId, badPlainId); return 0; }, "_ZN3sce2np4NpIdC1ERK7SceNpId"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4NpId5ClearEv(badNpId); return 0; }, "_ZN3sce2np4NpId5ClearEv"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4UserC1Ev(badUser); return 0; }, "_ZN3sce2np4UserC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4UserC1Ei(badUser, 1); return 0; }, "_ZN3sce2np4UserC1Ei"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4UserC1ERKS1_(&sameone, badUser); return 0; }, "_ZN3sce2np4UserC1ERKS1_"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npeqERKNS0_4UserES3_(badUser, &someone); }, "_ZN3sce2npeqERKNS0_4UserES3_"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npeqERKNS0_4UserERKi(&someone, badInt); }, "_ZN3sce2npeqERKNS0_4UserERKi"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npneERKNS0_4UserES3_(&someone, badUser); }, "_ZN3sce2npneERKNS0_4UserES3_"));
    Require(FaultsAsNotImplemented([&] { return _ZN3sce2npneERKNS0_4UserERKi(badUser, &thousand); }, "_ZN3sce2npneERKNS0_4UserERKi"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np12NpTitleTokenC1Ev(badTitle); return 0; }, "_ZN3sce2np12NpTitleTokenC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np12NpTitleTokenC1ERKS1_(titleCopy, badTitle); return 0; }, "_ZN3sce2np12NpTitleTokenC1ERKS1_"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np12NpTitleToken5ClearEv(badTitle); return 0; }, "_ZN3sce2np12NpTitleToken5ClearEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3sce2np12NpTitleToken6GetStrEv(badTitle); }, "_ZNK3sce2np12NpTitleToken6GetStrEv"));
    Require(_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(nullptr, "abc") == static_cast<int>(0x80550003));
    Require(_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(badOnline, "abc") == static_cast<int>(0x80550003));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4User5ClearEv(badUser); return 0; }, "_ZN3sce2np4User5ClearEv"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np13NpAccessTokenC1Ev(badAccess); return 0; }, "_ZN3sce2np13NpAccessTokenC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np13NpAccessTokenC1ERKS1_(accessCopy, badAccess); return 0; }, "_ZN3sce2np13NpAccessTokenC1ERKS1_"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken(accessCopy, badPlainToken); return 0; }, "_ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np13NpAccessToken5ClearEv(badAccess); return 0; }, "_ZN3sce2np13NpAccessToken5ClearEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3sce2np13NpAccessToken7IsEmptyEv(badAccess); }, "_ZNK3sce2np13NpAccessToken7IsEmptyEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3sce2np8NpTicket13GetTicketDataEv(badTicket); }, "_ZNK3sce2np8NpTicket13GetTicketDataEv"));
    Require(FaultsAsNotImplemented([&] { return _ZNK3sce2np8NpTicket13GetTicketSizeEv(badTicket); }, "_ZNK3sce2np8NpTicket13GetTicketSizeEv"));

    unsigned char* page = MapPage();
    std::memset(page, 0x5a, 4096);
    ProtectReadOnly(page);
    auto* readOnlyOnline = reinterpret_cast<NpOnlineIdObject*>(page);
    auto* readOnlyUser = reinterpret_cast<UserObject*>(page + 64);
    auto* readOnlyAccess = reinterpret_cast<NpAccessTokenObject*>(page + 128);
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np10NpOnlineIdC1Ev(readOnlyOnline); return 0; }, "_ZN3sce2np10NpOnlineIdC1Ev"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np10NpOnlineId5ClearEv(readOnlyOnline); return 0; }, "_ZN3sce2np10NpOnlineId5ClearEv"));
    Require(_ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(readOnlyOnline, "abc") == static_cast<int>(0x80550003));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4User5ClearEv(readOnlyUser); return 0; }, "_ZN3sce2np4User5ClearEv"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np4UserC1ERKS1_(readOnlyUser, &someone); return 0; }, "_ZN3sce2np4UserC1ERKS1_"));
    Require(FaultsAsNotImplemented([&] { _ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken(readOnlyAccess, &plainToken); return 0; }, "_ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken"));
    for (std::size_t index = 0; index < 4096; ++index) Require(page[index] == 0x5a);
    Require(!_ZNK3sce2np10NpOnlineId7IsEmptyEv(readOnlyOnline) && !_ZNK3sce2np13NpAccessToken7IsEmptyEv(readOnlyAccess));
    UnmapPage(page);

    delete title;
    delete titleCopy;
    delete access;
    delete fromToken;
    delete accessCopy;
    return 0;
}
