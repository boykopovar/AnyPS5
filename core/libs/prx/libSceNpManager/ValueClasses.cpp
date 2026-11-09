#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>

namespace {

constexpr int NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
constexpr int NP_ERROR_USER_NOT_FOUND = static_cast<int>(0x80550007);
constexpr int NP_ERROR_ONLINE_ID_TOO_LONG = static_cast<int>(0x80559E11);
constexpr int NP_ERROR_USER_INVALID = static_cast<int>(0x80559E2A);
constexpr int NP_ERROR_ACCOUNT_ID_MISSING = static_cast<int>(0x80559E2B);
constexpr std::size_t NP_ONLINE_ID_SIZE = 16;

bool SameOnlineId(const char* lhs, const char* rhs) {
    return std::strncmp(lhs, rhs, NP_ONLINE_ID_SIZE) == 0;
}

void* const NpOnlineIdVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np10NpOnlineIdD1Ev),
    reinterpret_cast<void*>(&_ZN3sce2np10NpOnlineIdD0Ev),
};

void* const NpIdVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np4NpIdD1Ev),
    reinterpret_cast<void*>(&_ZN3sce2np4NpIdD0Ev),
};

void* const UserVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np4UserD1Ev),
    reinterpret_cast<void*>(&_ZN3sce2np4UserD0Ev),
};

void* const NpTitleTokenVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np12NpTitleTokenD1Ev),
    reinterpret_cast<void*>(&_ZN3sce2np12NpTitleTokenD0Ev),
};

void* const NpAccessTokenVtable[] = {
    nullptr, nullptr,
    reinterpret_cast<void*>(&_ZN3sce2np13NpAccessTokenD1Ev),
    reinterpret_cast<void*>(&_ZN3sce2np13NpAccessTokenD0Ev),
};

}

extern "C" {

void APS5_VABI _ZN3sce2np10NpOnlineIdC1Ev(NpOnlineIdObject* self) {
    self->vtable = &NpOnlineIdVtable[2];
    std::memset(self->data, 0, sizeof(self->data));
    self->term = 0;
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC2Ev(NpOnlineIdObject* self) {
    _ZN3sce2np10NpOnlineIdC1Ev(self);
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC1ERKS1_(NpOnlineIdObject* self, const NpOnlineIdObject* other) {
    self->vtable = &NpOnlineIdVtable[2];
    std::memcpy(self->data, other->data, sizeof(self->data));
    self->term = other->term;
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC2ERKS1_(NpOnlineIdObject* self, const NpOnlineIdObject* other) {
    _ZN3sce2np10NpOnlineIdC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC1ERK13SceNpOnlineId(NpOnlineIdObject* self, const SceNpOnlineId* source) {
    self->vtable = &NpOnlineIdVtable[2];
    std::memcpy(self->data, source->data, sizeof(self->data));
    self->term = source->term;
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC2ERK13SceNpOnlineId(NpOnlineIdObject* self, const SceNpOnlineId* source) {
    _ZN3sce2np10NpOnlineIdC1ERK13SceNpOnlineId(self, source);
}

void APS5_VABI _ZN3sce2np10NpOnlineId5ClearEv(NpOnlineIdObject* self) {
    std::memset(self->data, 0, sizeof(self->data));
    self->term = 0;
}

int APS5_VABI _ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(NpOnlineIdObject* self, const char* text) {
    if (text == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    const std::size_t length = std::strlen(text);
    if (length > NP_ONLINE_ID_SIZE) return NP_ERROR_ONLINE_ID_TOO_LONG;
    std::memset(self->data, 0, sizeof(self->data));
    self->term = 0;
    std::memcpy(self->data, text, length);
    return 0;
}

bool APS5_VABI _ZNK3sce2np10NpOnlineId7IsEmptyEv(const NpOnlineIdObject* self) {
    return self->data[0] == '\0';
}

int APS5_VABI _ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEP13SceNpOnlineId(const UserObject* user, SceNpOnlineId* out) {
    if (user == nullptr || out == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    if (user->userId == USER_SERVICE_USER_ID_INVALID) return NP_ERROR_USER_NOT_FOUND;
    return NP_ERROR_SIGNED_OUT;
}

int APS5_VABI _ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEPS1_(const UserObject* user, NpOnlineIdObject* out) {
    if (user == nullptr || out == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    if (user->userId == USER_SERVICE_USER_ID_INVALID) return NP_ERROR_USER_NOT_FOUND;
    return NP_ERROR_SIGNED_OUT;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_10NpOnlineIdES3_(const NpOnlineIdObject* lhs, const NpOnlineIdObject* rhs) {
    return SameOnlineId(lhs->data, rhs->data);
}

bool APS5_VABI _ZN3sce2npeqERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(const SceNpOnlineId* lhs, const NpOnlineIdObject* rhs) {
    return SameOnlineId(lhs->data, rhs->data);
}

bool APS5_VABI _ZN3sce2npeqERKNS0_10NpOnlineIdERK13SceNpOnlineId(const NpOnlineIdObject* lhs, const SceNpOnlineId* rhs) {
    return SameOnlineId(lhs->data, rhs->data);
}

bool APS5_VABI _ZN3sce2npneERKNS0_10NpOnlineIdES3_(const NpOnlineIdObject* lhs, const NpOnlineIdObject* rhs) {
    return !SameOnlineId(lhs->data, rhs->data);
}

bool APS5_VABI _ZN3sce2npneERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(const SceNpOnlineId* lhs, const NpOnlineIdObject* rhs) {
    return !SameOnlineId(lhs->data, rhs->data);
}

bool APS5_VABI _ZN3sce2npneERKNS0_10NpOnlineIdERK13SceNpOnlineId(const NpOnlineIdObject* lhs, const SceNpOnlineId* rhs) {
    return !SameOnlineId(lhs->data, rhs->data);
}

void APS5_VABI _ZN3sce2np4NpIdC1Ev(NpIdObject* self) {
    self->vtable = &NpIdVtable[2];
    std::memset(&self->id, 0, sizeof(self->id));
}

void APS5_VABI _ZN3sce2np4NpIdC2Ev(NpIdObject* self) {
    _ZN3sce2np4NpIdC1Ev(self);
}

void APS5_VABI _ZN3sce2np4NpIdC1ERKS1_(NpIdObject* self, const NpIdObject* other) {
    self->vtable = &NpIdVtable[2];
    std::memcpy(&self->id, &other->id, sizeof(self->id));
}

void APS5_VABI _ZN3sce2np4NpIdC2ERKS1_(NpIdObject* self, const NpIdObject* other) {
    _ZN3sce2np4NpIdC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np4NpIdC1ERK7SceNpId(NpIdObject* self, const SceNpId* source) {
    self->vtable = &NpIdVtable[2];
    std::memcpy(&self->id, source, sizeof(self->id));
}

void APS5_VABI _ZN3sce2np4NpIdC2ERK7SceNpId(NpIdObject* self, const SceNpId* source) {
    _ZN3sce2np4NpIdC1ERK7SceNpId(self, source);
}

void APS5_VABI _ZN3sce2np4NpId5ClearEv(NpIdObject* self) {
    std::memset(&self->id, 0, sizeof(self->id));
}

void APS5_VABI _ZN3sce2np4UserC1Ev(UserObject* self) {
    self->vtable = &UserVtable[2];
    self->userId = USER_SERVICE_USER_ID_INVALID;
}

void APS5_VABI _ZN3sce2np4UserC2Ev(UserObject* self) {
    _ZN3sce2np4UserC1Ev(self);
}

void APS5_VABI _ZN3sce2np4UserC1Ei(UserObject* self, int userId) {
    self->vtable = &UserVtable[2];
    self->userId = userId;
}

void APS5_VABI _ZN3sce2np4UserC2Ei(UserObject* self, int userId) {
    _ZN3sce2np4UserC1Ei(self, userId);
}

void APS5_VABI _ZN3sce2np4UserC1ERKS1_(UserObject* self, const UserObject* other) {
    self->vtable = &UserVtable[2];
    self->userId = other->userId;
}

void APS5_VABI _ZN3sce2np4UserC2ERKS1_(UserObject* self, const UserObject* other) {
    _ZN3sce2np4UserC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np4User5ClearEv(UserObject* self) {
    self->userId = USER_SERVICE_USER_ID_INVALID;
}

bool APS5_VABI _ZNK3sce2np4User7IsEmptyEv(const UserObject* self) {
    return self->userId == USER_SERVICE_USER_ID_INVALID;
}

int APS5_VABI _ZNK3sce2np4User9GetUserIdEv(const UserObject* self) {
    return self->userId;
}

bool APS5_VABI _ZNK3sce2np4User10IsLoggedInEv(const UserObject* self) {
    if (self->userId == USER_SERVICE_USER_ID_INVALID) return false;
    std::uint64_t accountId = 0;
    return sceUserServiceGetNpAccountId(self->userId, &accountId) == 0;
}

bool APS5_VABI _ZNK3sce2np4User7IsGuestEv(const UserObject* self) {
    return sceUserServiceIsGuestUser(self->userId) != 0;
}

int APS5_VABI _ZNK3sce2np4User12GetAccountIdEPm(const UserObject* self, std::uint64_t* accountId) {
    if (accountId == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    *accountId = 0;
    const int result = sceUserServiceGetNpAccountId(self->userId, accountId);
    if (result < 0) return result;
    if (*accountId == 0) return NP_ERROR_ACCOUNT_ID_MISSING;
    return 0;
}

int APS5_VABI _ZNK3sce2np4User12HasAccountIdEPb(const UserObject* self, bool* hasAccountId) {
    if (hasAccountId == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    std::uint64_t accountId = 0;
    const int result = sceUserServiceGetNpAccountId(self->userId, &accountId);
    if (result < 0) return result;
    *hasAccountId = accountId != 0;
    return 0;
}

int APS5_VABI _ZNK3sce2np4User25GetAccountIdFromRegistoryEPm(const UserObject* self, std::uint64_t* accountId) {
    const int result = sceUserServiceGetNpAccountId(self->userId, accountId);
    return result < 0 ? result : 0;
}

int APS5_VABI _ZN3sce2np4User7GetUserEiPS1_(int userId, UserObject* out) {
    if (out == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    if (userId == USER_SERVICE_USER_ID_INVALID) return NP_ERROR_USER_INVALID;
    UserServiceRegisteredUserIdList users {};
    const int result = sceUserServiceGetRegisteredUserIdList(&users);
    if (result < 0) return result;
    if (std::find(std::begin(users.user_id), std::end(users.user_id), userId) == std::end(users.user_id)) {
        return NP_ERROR_USER_INVALID;
    }
    out->userId = userId;
    return 0;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_4UserES3_(const UserObject* lhs, const UserObject* rhs) {
    return lhs->userId == rhs->userId;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_4UserERKi(const UserObject* lhs, const int* rhs) {
    return lhs->userId == *rhs;
}

bool APS5_VABI _ZN3sce2npneERKNS0_4UserES3_(const UserObject* lhs, const UserObject* rhs) {
    return lhs->userId != rhs->userId;
}

bool APS5_VABI _ZN3sce2npneERKNS0_4UserERKi(const UserObject* lhs, const int* rhs) {
    return lhs->userId != *rhs;
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC1Ev(NpTitleTokenObject* self) {
    self->vtable = &NpTitleTokenVtable[2];
    std::memset(self->data, 0, sizeof(self->data));
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC2Ev(NpTitleTokenObject* self) {
    _ZN3sce2np12NpTitleTokenC1Ev(self);
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC1ERKS1_(NpTitleTokenObject* self, const NpTitleTokenObject* other) {
    self->vtable = &NpTitleTokenVtable[2];
    std::memcpy(self->data, other->data, sizeof(self->data));
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC2ERKS1_(NpTitleTokenObject* self, const NpTitleTokenObject* other) {
    _ZN3sce2np12NpTitleTokenC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np12NpTitleToken5ClearEv(NpTitleTokenObject* self) {
    std::memset(self->data, 0, sizeof(self->data));
}

const char* APS5_VABI _ZNK3sce2np12NpTitleToken6GetStrEv(const NpTitleTokenObject* self) {
    return self->data;
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC1Ev(NpAccessTokenObject* self) {
    self->vtable = &NpAccessTokenVtable[2];
    std::memset(self->data, 0, sizeof(self->data));
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC2Ev(NpAccessTokenObject* self) {
    _ZN3sce2np13NpAccessTokenC1Ev(self);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC1ERKS1_(NpAccessTokenObject* self, const NpAccessTokenObject* other) {
    self->vtable = &NpAccessTokenVtable[2];
    std::memcpy(self->data, other->data, sizeof(self->data));
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC2ERKS1_(NpAccessTokenObject* self, const NpAccessTokenObject* other) {
    _ZN3sce2np13NpAccessTokenC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken(NpAccessTokenObject* self, const SceNpAccessToken* source) {
    self->vtable = &NpAccessTokenVtable[2];
    std::memcpy(self->data, source->data, sizeof(self->data));
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC2ERK16SceNpAccessToken(NpAccessTokenObject* self, const SceNpAccessToken* source) {
    _ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken(self, source);
}

void APS5_VABI _ZN3sce2np13NpAccessToken5ClearEv(NpAccessTokenObject* self) {
    std::memset(self->data, 0, sizeof(self->data));
}

bool APS5_VABI _ZNK3sce2np13NpAccessToken7IsEmptyEv(const NpAccessTokenObject* self) {
    std::uint8_t empty[sizeof(self->data)] {};
    return std::memcmp(self->data, empty, sizeof(self->data)) == 0;
}

void* APS5_VABI _ZNK3sce2np8NpTicket13GetTicketDataEv(const NpTicketObject* self) {
    if (self->data == nullptr) throw std::runtime_error("NpTicket data is empty");
    return self->data;
}

std::size_t APS5_VABI _ZNK3sce2np8NpTicket13GetTicketSizeEv(const NpTicketObject* self) {
    return self->size;
}

}
