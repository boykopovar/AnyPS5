#include "prx/libSceNpManager/include/Api.hpp"
#include "prx/libSceUserService/UserService.hpp"
#include "prx/libc/include/general/ExportMacros.hpp"
#include "prx/libc/include/General.hpp"
#include <algorithm>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <stdexcept>
#include <string>

namespace {

constexpr int NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);
constexpr int NP_ERROR_SIGNED_OUT = static_cast<int>(0x80550006);
constexpr int NP_ERROR_USER_NOT_FOUND = static_cast<int>(0x80550007);
constexpr int NP_ERROR_ONLINE_ID_TOO_LONG = static_cast<int>(0x80559E11);
constexpr int NP_ERROR_USER_INVALID = static_cast<int>(0x80559E2A);
constexpr int NP_ERROR_ACCOUNT_ID_MISSING = static_cast<int>(0x80559E2B);
constexpr std::size_t NP_ONLINE_ID_SIZE = 16;

template <typename T>
bool ReadGuest(const T* source, T& destination) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&destination, source, sizeof(destination));
    errno = savedErrno;
    return copied;
}

template <typename T>
bool ProbeGuestWritable(T* destination) {
    T value{};
    const int savedErrno = errno;
    const bool ok = GuestCopyIn_nid_no_patch(&value, destination, sizeof(value)) &&
        GuestCopyOut_nid_no_patch(destination, &value, sizeof(value));
    errno = savedErrno;
    return ok;
}

bool SameOnlineId(const char* lhs, const char* rhs) {
    return std::strncmp(lhs, rhs, NP_ONLINE_ID_SIZE) == 0;
}

template <typename T>
T LoadObject(const T* source, const char* function) {
    T value{};
    if (!ReadGuest(source, value)) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
    return value;
}

void StoreBytes(void* destination, const void* source, std::size_t bytes, const char* function) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    if (!copied) NotImplementedAt_nid_no_patch(function, __FILE__, __LINE__);
}

constexpr std::size_t OnlineIdFieldBytes = offsetof(NpOnlineIdObject, term) + sizeof(std::uint32_t);
constexpr std::size_t NpIdFieldBytes = offsetof(NpIdObject, id) + sizeof(SceNpId);
constexpr std::size_t UserFieldBytes = offsetof(UserObject, userId) + sizeof(std::int32_t);

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
    NpOnlineIdObject object{};
    object.vtable = &NpOnlineIdVtable[2];
    StoreBytes(self, &object, OnlineIdFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC2Ev(NpOnlineIdObject* self) {
    _ZN3sce2np10NpOnlineIdC1Ev(self);
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC1ERKS1_(NpOnlineIdObject* self, const NpOnlineIdObject* other) {
    auto object = LoadObject(other, __func__);
    object.vtable = &NpOnlineIdVtable[2];
    StoreBytes(self, &object, OnlineIdFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC2ERKS1_(NpOnlineIdObject* self, const NpOnlineIdObject* other) {
    _ZN3sce2np10NpOnlineIdC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC1ERK13SceNpOnlineId(NpOnlineIdObject* self, const SceNpOnlineId* source) {
    const auto id = LoadObject(source, __func__);
    NpOnlineIdObject object{};
    object.vtable = &NpOnlineIdVtable[2];
    std::memcpy(object.data, id.data, sizeof(object.data));
    object.term = id.term;
    StoreBytes(self, &object, OnlineIdFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np10NpOnlineIdC2ERK13SceNpOnlineId(NpOnlineIdObject* self, const SceNpOnlineId* source) {
    _ZN3sce2np10NpOnlineIdC1ERK13SceNpOnlineId(self, source);
}

void APS5_VABI _ZN3sce2np10NpOnlineId5ClearEv(NpOnlineIdObject* self) {
    const NpOnlineIdObject object{};
    StoreBytes(self->data, object.data, OnlineIdFieldBytes - offsetof(NpOnlineIdObject, data), __func__);
}

int APS5_VABI _ZN3sce2np10NpOnlineId16SetNpOnlineIdStrEPKc(NpOnlineIdObject* self, const char* text) {
    if (text == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    std::string copy;
    const int error = CopyGuestString_nid_no_patch(text, NP_ONLINE_ID_SIZE + 1, copy);
    if (error == 63) return NP_ERROR_ONLINE_ID_TOO_LONG;
    if (error != 0) return NP_ERROR_INVALID_ARGUMENT;
    std::memset(self->data, 0, sizeof(self->data));
    self->term = 0;
    std::memcpy(self->data, copy.data(), copy.size());
    return 0;
}

bool APS5_VABI _ZNK3sce2np10NpOnlineId7IsEmptyEv(const NpOnlineIdObject* self) {
    return self->data[0] == '\0';
}

int APS5_VABI _ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEP13SceNpOnlineId(const UserObject* user, SceNpOnlineId* out) {
    if (user == nullptr || out == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    UserObject localUser{};
    if (!ReadGuest(user, localUser)) return NP_ERROR_INVALID_ARGUMENT;
    if (!ProbeGuestWritable(out)) return NP_ERROR_INVALID_ARGUMENT;
    if (localUser.userId == USER_SERVICE_USER_ID_INVALID) return NP_ERROR_USER_NOT_FOUND;
    return NP_ERROR_SIGNED_OUT;
}

int APS5_VABI _ZN3sce2np10NpOnlineId13GetNpOnlineIdERKNS0_4UserEPS1_(const UserObject* user, NpOnlineIdObject* out) {
    if (user == nullptr || out == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    UserObject localUser{};
    if (!ReadGuest(user, localUser)) return NP_ERROR_INVALID_ARGUMENT;
    if (!ProbeGuestWritable(out)) return NP_ERROR_INVALID_ARGUMENT;
    if (localUser.userId == USER_SERVICE_USER_ID_INVALID) return NP_ERROR_USER_NOT_FOUND;
    return NP_ERROR_SIGNED_OUT;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_10NpOnlineIdES3_(const NpOnlineIdObject* lhs, const NpOnlineIdObject* rhs) {
    const auto left = LoadObject(lhs, __func__);
    const auto right = LoadObject(rhs, __func__);
    return SameOnlineId(left.data, right.data);
}

bool APS5_VABI _ZN3sce2npeqERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(const SceNpOnlineId* lhs, const NpOnlineIdObject* rhs) {
    const auto left = LoadObject(lhs, __func__);
    const auto right = LoadObject(rhs, __func__);
    return SameOnlineId(left.data, right.data);
}

bool APS5_VABI _ZN3sce2npeqERKNS0_10NpOnlineIdERK13SceNpOnlineId(const NpOnlineIdObject* lhs, const SceNpOnlineId* rhs) {
    const auto left = LoadObject(lhs, __func__);
    const auto right = LoadObject(rhs, __func__);
    return SameOnlineId(left.data, right.data);
}

bool APS5_VABI _ZN3sce2npneERKNS0_10NpOnlineIdES3_(const NpOnlineIdObject* lhs, const NpOnlineIdObject* rhs) {
    const auto left = LoadObject(lhs, __func__);
    const auto right = LoadObject(rhs, __func__);
    return !SameOnlineId(left.data, right.data);
}

bool APS5_VABI _ZN3sce2npneERK13SceNpOnlineIdRKNS0_10NpOnlineIdE(const SceNpOnlineId* lhs, const NpOnlineIdObject* rhs) {
    const auto left = LoadObject(lhs, __func__);
    const auto right = LoadObject(rhs, __func__);
    return !SameOnlineId(left.data, right.data);
}

bool APS5_VABI _ZN3sce2npneERKNS0_10NpOnlineIdERK13SceNpOnlineId(const NpOnlineIdObject* lhs, const SceNpOnlineId* rhs) {
    const auto left = LoadObject(lhs, __func__);
    const auto right = LoadObject(rhs, __func__);
    return !SameOnlineId(left.data, right.data);
}

void APS5_VABI _ZN3sce2np4NpIdC1Ev(NpIdObject* self) {
    NpIdObject object{};
    object.vtable = &NpIdVtable[2];
    StoreBytes(self, &object, NpIdFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np4NpIdC2Ev(NpIdObject* self) {
    _ZN3sce2np4NpIdC1Ev(self);
}

void APS5_VABI _ZN3sce2np4NpIdC1ERKS1_(NpIdObject* self, const NpIdObject* other) {
    auto object = LoadObject(other, __func__);
    object.vtable = &NpIdVtable[2];
    StoreBytes(self, &object, NpIdFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np4NpIdC2ERKS1_(NpIdObject* self, const NpIdObject* other) {
    _ZN3sce2np4NpIdC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np4NpIdC1ERK7SceNpId(NpIdObject* self, const SceNpId* source) {
    NpIdObject object{};
    object.vtable = &NpIdVtable[2];
    object.id = LoadObject(source, __func__);
    StoreBytes(self, &object, NpIdFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np4NpIdC2ERK7SceNpId(NpIdObject* self, const SceNpId* source) {
    _ZN3sce2np4NpIdC1ERK7SceNpId(self, source);
}

void APS5_VABI _ZN3sce2np4NpId5ClearEv(NpIdObject* self) {
    const SceNpId id{};
    StoreBytes(&self->id, &id, sizeof(id), __func__);
}

void APS5_VABI _ZN3sce2np4UserC1Ev(UserObject* self) {
    UserObject object{};
    object.vtable = &UserVtable[2];
    object.userId = USER_SERVICE_USER_ID_INVALID;
    StoreBytes(self, &object, UserFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np4UserC2Ev(UserObject* self) {
    _ZN3sce2np4UserC1Ev(self);
}

void APS5_VABI _ZN3sce2np4UserC1Ei(UserObject* self, int userId) {
    UserObject object{};
    object.vtable = &UserVtable[2];
    object.userId = userId;
    StoreBytes(self, &object, UserFieldBytes, __func__);
}

void APS5_VABI _ZN3sce2np4UserC2Ei(UserObject* self, int userId) {
    _ZN3sce2np4UserC1Ei(self, userId);
}

void APS5_VABI _ZN3sce2np4UserC1ERKS1_(UserObject* self, const UserObject* other) {
    UserObject object{};
    object.vtable = &UserVtable[2];
    object.userId = LoadObject(other, __func__).userId;
    StoreBytes(self, &object, UserFieldBytes, __func__);
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
    std::uint64_t value = 0;
    if (!GuestCopyOut_nid_no_patch(accountId, &value, sizeof(value))) return NP_ERROR_INVALID_ARGUMENT;
    const int result = sceUserServiceGetNpAccountId(self->userId, &value);
    if (result < 0) return result;
    if (!GuestCopyOut_nid_no_patch(accountId, &value, sizeof(value))) return NP_ERROR_INVALID_ARGUMENT;
    if (value == 0) return NP_ERROR_ACCOUNT_ID_MISSING;
    return 0;
}

int APS5_VABI _ZNK3sce2np4User12HasAccountIdEPb(const UserObject* self, bool* hasAccountId) {
    if (hasAccountId == nullptr) return NP_ERROR_INVALID_ARGUMENT;
    std::uint64_t accountId = 0;
    const int result = sceUserServiceGetNpAccountId(self->userId, &accountId);
    if (result < 0) return result;
    const bool has = accountId != 0;
    if (!GuestCopyOut_nid_no_patch(hasAccountId, &has, sizeof(has))) return NP_ERROR_INVALID_ARGUMENT;
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
    if (!GuestCopyOut_nid_no_patch(&out->userId, &userId, sizeof(userId))) return NP_ERROR_INVALID_ARGUMENT;
    return 0;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_4UserES3_(const UserObject* lhs, const UserObject* rhs) {
    return LoadObject(lhs, __func__).userId == LoadObject(rhs, __func__).userId;
}

bool APS5_VABI _ZN3sce2npeqERKNS0_4UserERKi(const UserObject* lhs, const int* rhs) {
    return LoadObject(lhs, __func__).userId == LoadObject(rhs, __func__);
}

bool APS5_VABI _ZN3sce2npneERKNS0_4UserES3_(const UserObject* lhs, const UserObject* rhs) {
    return LoadObject(lhs, __func__).userId != LoadObject(rhs, __func__).userId;
}

bool APS5_VABI _ZN3sce2npneERKNS0_4UserERKi(const UserObject* lhs, const int* rhs) {
    return LoadObject(lhs, __func__).userId != LoadObject(rhs, __func__);
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC1Ev(NpTitleTokenObject* self) {
    NpTitleTokenObject object{};
    object.vtable = &NpTitleTokenVtable[2];
    StoreBytes(self, &object, sizeof(object), __func__);
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC2Ev(NpTitleTokenObject* self) {
    _ZN3sce2np12NpTitleTokenC1Ev(self);
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC1ERKS1_(NpTitleTokenObject* self, const NpTitleTokenObject* other) {
    auto object = LoadObject(other, __func__);
    object.vtable = &NpTitleTokenVtable[2];
    StoreBytes(self, &object, sizeof(object), __func__);
}

void APS5_VABI _ZN3sce2np12NpTitleTokenC2ERKS1_(NpTitleTokenObject* self, const NpTitleTokenObject* other) {
    _ZN3sce2np12NpTitleTokenC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np12NpTitleToken5ClearEv(NpTitleTokenObject* self) {
    const NpTitleTokenObject object{};
    StoreBytes(self->data, object.data, sizeof(object.data), __func__);
}

const char* APS5_VABI _ZNK3sce2np12NpTitleToken6GetStrEv(const NpTitleTokenObject* self) {
    return self->data;
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC1Ev(NpAccessTokenObject* self) {
    NpAccessTokenObject object{};
    object.vtable = &NpAccessTokenVtable[2];
    StoreBytes(self, &object, sizeof(object), __func__);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC2Ev(NpAccessTokenObject* self) {
    _ZN3sce2np13NpAccessTokenC1Ev(self);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC1ERKS1_(NpAccessTokenObject* self, const NpAccessTokenObject* other) {
    auto object = LoadObject(other, __func__);
    object.vtable = &NpAccessTokenVtable[2];
    StoreBytes(self, &object, sizeof(object), __func__);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC2ERKS1_(NpAccessTokenObject* self, const NpAccessTokenObject* other) {
    _ZN3sce2np13NpAccessTokenC1ERKS1_(self, other);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken(NpAccessTokenObject* self, const SceNpAccessToken* source) {
    const auto token = LoadObject(source, __func__);
    NpAccessTokenObject object{};
    object.vtable = &NpAccessTokenVtable[2];
    std::memcpy(object.data, token.data, sizeof(object.data));
    StoreBytes(self, &object, sizeof(object), __func__);
}

void APS5_VABI _ZN3sce2np13NpAccessTokenC2ERK16SceNpAccessToken(NpAccessTokenObject* self, const SceNpAccessToken* source) {
    _ZN3sce2np13NpAccessTokenC1ERK16SceNpAccessToken(self, source);
}

void APS5_VABI _ZN3sce2np13NpAccessToken5ClearEv(NpAccessTokenObject* self) {
    const NpAccessTokenObject object{};
    StoreBytes(self->data, object.data, sizeof(object.data), __func__);
}

bool APS5_VABI _ZNK3sce2np13NpAccessToken7IsEmptyEv(const NpAccessTokenObject* self) {
    const auto token = LoadObject(self, __func__);
    std::uint8_t empty[sizeof(token.data)] {};
    return std::memcmp(token.data, empty, sizeof(token.data)) == 0;
}

void* APS5_VABI _ZNK3sce2np8NpTicket13GetTicketDataEv(const NpTicketObject* self) {
    const auto ticket = LoadObject(self, __func__);
    if (ticket.data == nullptr) throw std::runtime_error("NpTicket data is empty");
    return ticket.data;
}

std::size_t APS5_VABI _ZNK3sce2np8NpTicket13GetTicketSizeEv(const NpTicketObject* self) {
    return LoadObject(self, __func__).size;
}

}
