#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "%s\n", what);
    std::abort();
}

static int deleterCalls = 0;

static void APS5_VABI CountDelete(void*) {
    ++deleterCalls;
}

static std::int32_t ReadCount(const void* object, const std::size_t offset) {
    std::int32_t value = 0;
    std::memcpy(&value, static_cast<const std::uint8_t*>(object) + offset, sizeof(value));
    return value;
}

static void WriteCount(void* object, const std::size_t offset, const std::int32_t value) {
    std::memcpy(static_cast<std::uint8_t*>(object) + offset, &value, sizeof(value));
}

static void TestParameterLifecycle() {
    alignas(16) unsigned char parameter[0x58];
    std::memset(parameter, 0x5a, sizeof(parameter));
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessionsC1Ev(parameter) == 0,
            "the parameter constructor failed");
    for (std::size_t index = 0; index < 0x10; ++index)
        Require(parameter[index] == 0, "the parameter constructor wrote the wrong bytes");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions10initializeEPNS1_6Common10LibContextE(parameter, nullptr) == 0x80553503,
            "initialize accepted a null context");
    std::uint64_t context = 0;
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions10initializeEPNS1_6Common10LibContextE(parameter, &context) == 0x80553502,
            "initialize accepted an uninitialized context");
    context = 1;
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions10initializeEPNS1_6Common10LibContextE(parameter, &context) == 0,
            "initialize failed");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions10initializeEPNS1_6Common10LibContextE(parameter, &context) == 0x8055350f,
            "initialize ran twice");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions9terminateEv(parameter) == 0,
            "terminate failed");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessions9terminateEv(parameter) == 0x80553510,
            "terminate accepted an uninitialized parameter");
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi35ParameterToGetFriendsPlayerSessionsD1Ev(parameter) == 0,
            "the parameter destructor failed");
}

static void TestOfflineCalls() {
    Require(_ZN3sce2Np9CppWebApi14SessionManager2V117PlayerSessionsApi24getFriendsPlayerSessionsEiRKNS4_35ParameterToGetFriendsPlayerSessionsERNS1_6Common11TransactionINS8_12IntrusivePtrINS3_36GetFriendsPlayerSessionsResponseBodyEEENSA_INS8_18ResponseHeaderBaseEEEEE() == 0x80552907,
            "getFriendsPlayerSessions did not report a signed-out console");
    alignas(8) unsigned char transaction[0x40] = {};
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEEC1Ev(transaction) == 0,
            "the transaction constructor failed");
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE5startEPNS2_10LibContextE() == 0x80552907,
            "start did not report a signed-out console");
    Require(_ZN3sce2Np9CppWebApi6Common15TransactionBaseINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE6finishEv() == 0x80553508,
            "finish did not report an unstarted transaction");
    Require(_ZNK3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEE11getResponseERS8_() == 0x80552907,
            "getResponse did not report a signed-out console");
    Require(_ZN3sce2Np9CppWebApi6Common11TransactionINS2_12IntrusivePtrINS1_14SessionManager2V136GetFriendsPlayerSessionsResponseBodyEEENS4_INS2_18ResponseHeaderBaseEEEED1Ev(transaction) == 0,
            "the transaction destructor failed");
}

static void TestFriendModel() {
    alignas(8) unsigned char object[0x60] = {};
    const char* accountId = "friend-account";
    std::memcpy(object + 0x10, &accountId, sizeof(accountId));
    object[0x18] = 1;
    Require(std::strcmp(_ZNK3sce2Np9CppWebApi14SessionManager2V16Friend12getAccountIdEv(object), accountId) == 0,
            "getAccountId returned the wrong value");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V16Friend14accountIdIsSetEv(object), "accountIdIsSet was false");
    Require(!_ZNK3sce2Np9CppWebApi14SessionManager2V16Friend19playerSessionsIsSetEv(object), "playerSessionsIsSet was true");
    object[0x48] = 1;
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V16Friend19playerSessionsIsSetEv(object), "playerSessionsIsSet was false");

    alignas(8) unsigned char joined[0x98] = {};
    joined[0x30] = 1;
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V125FriendJoinedPlayerSession12getSessionIdEv(joined) ==
            reinterpret_cast<const CommonString*>(joined + 0x10), "getSessionId returned the wrong address");
    Require(_ZNK3sce2Np9CppWebApi14SessionManager2V125FriendJoinedPlayerSession14sessionIdIsSetEv(joined),
            "sessionIdIsSet was false");
}

static void TestIntrusivePtr() {
    alignas(8) unsigned char object[0x40] = {};
    WriteCount(object, 0, 1);
    IntrusivePtr source{object, CountDelete, nullptr};
    IntrusivePtr copy;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEEC1ERKS7_(&copy, &source);
    Require(ReadCount(object, 0) == 2, "the copy constructor did not add a reference");
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEE3getEv(&copy) == object,
            "get returned the wrong object");
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEEptEv(&copy) == object,
            "operator-> returned the wrong object");
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEEcvbEv(&copy),
            "operator bool was false");
    deleterCalls = 0;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEED1Ev(&copy);
    Require(ReadCount(object, 0) == 1 && deleterCalls == 0, "destroying a shared pointer released the object");
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS1_14SessionManager2V16FriendEED1Ev(&source);
    Require(ReadCount(object, 0) == 0 && deleterCalls == 1, "the last reference did not run the deleter");
}

static void TestVectorLayout() {
    alignas(8) unsigned char elements[2 * 0x18] = {};
    alignas(8) unsigned char vector[0x40] = {};
    void* begin = elements;
    void* capacity = elements + sizeof(elements);
    std::memcpy(vector + 0x18, &begin, sizeof(begin));
    std::memcpy(vector + 0x20, &capacity, sizeof(capacity));
    std::memcpy(vector + 0x28, &capacity, sizeof(capacity));
    WriteCount(vector, 0x30, 1);

    Require(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE4sizeEv(vector) == 2,
            "size returned the wrong value");
    Require(!_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE5emptyEv(vector),
            "empty returned true for a non-empty vector");
    Require(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEixEm(vector, 0) == elements,
            "operator[] returned the wrong first element");
    Require(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEixEm(vector, 1) == elements + 0x18,
            "operator[] returned the wrong second element");

    CommonConstIterator iterator = _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE5beginEv(vector);
    CommonConstIterator last = _ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE3endEv(vector);
    Require(iterator.element == elements && last.element == elements + sizeof(elements),
            "begin/end returned the wrong elements");
    Require(_ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEdeEv(&iterator) == elements,
            "iterator dereference returned the wrong element");
    Require(_ZNK3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEneERKS9_(&iterator, &last),
            "iterator comparison returned false");
    _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEEppEv(&iterator);
    Require(iterator.element == elements + 0x18, "iterator increment moved by the wrong size");
    _ZN3sce2Np9CppWebApi6Common13ConstIteratorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEED1Ev(&iterator);

    alignas(8) unsigned char emptyVector[0x40] = {};
    std::memcpy(emptyVector + 0x18, &emptyVector, sizeof(void*));
    std::memcpy(emptyVector + 0x28, &emptyVector, sizeof(void*));
    Require(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE4sizeEv(emptyVector) == 0,
            "an empty vector reported elements");
    Require(_ZNK3sce2Np9CppWebApi6Common6VectorINS2_12IntrusivePtrINS1_14SessionManager2V16FriendEEEE5emptyEv(emptyVector),
            "empty returned false for an empty vector");
}

static void TestResponseAndFriendVectors() {
    alignas(8) unsigned char elements[0x18] = {};
    alignas(8) unsigned char vector[0x40] = {};
    void* begin = elements;
    void* capacity = elements + sizeof(elements);
    std::memcpy(vector + 0x18, &begin, sizeof(begin));
    std::memcpy(vector + 0x20, &capacity, sizeof(capacity));
    std::memcpy(vector + 0x28, &capacity, sizeof(capacity));
    WriteCount(vector, 0x30, 1);

    const IntrusivePtr source{vector, CountDelete, nullptr};
    alignas(8) unsigned char response[0x50] = {};
    std::memcpy(response + 0x30, &source, sizeof(source));

    IntrusivePtr friends = _ZN3sce2Np9CppWebApi14SessionManager2V136GetFriendsPlayerSessionsResponseBody10getFriendsEv(response);
    Require(friends.object == vector && ReadCount(vector, 0x30) == 2,
            "getFriends did not copy and retain the vector");
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V16FriendEEEEEEcvbEv(&friends),
            "the vector pointer was empty");
    Require(_ZNK3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V16FriendEEEEEEdeEv(&friends) == vector,
            "the vector pointer returned the wrong object");

    alignas(8) unsigned char friendObject[0x60] = {};
    std::memcpy(friendObject + 0x30, &source, sizeof(source));
    IntrusivePtr playerSessions = _ZN3sce2Np9CppWebApi14SessionManager2V16Friend17getPlayerSessionsEv(friendObject);
    Require(playerSessions.object == vector && ReadCount(vector, 0x30) == 3,
            "getPlayerSessions did not copy and retain the vector");

    deleterCalls = 0;
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V16FriendEEEEEED1Ev(&friends);
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V125FriendJoinedPlayerSessionEEEEEED1Ev(&playerSessions);
    Require(ReadCount(vector, 0x30) == 1 && deleterCalls == 0,
            "destroying shared vector pointers released the vector");
    IntrusivePtr last{vector, CountDelete, nullptr};
    _ZN3sce2Np9CppWebApi6Common12IntrusivePtrINS2_6VectorINS3_INS1_14SessionManager2V16FriendEEEEEED1Ev(&last);
    Require(ReadCount(vector, 0x30) == 0 && deleterCalls == 1,
            "the last vector reference did not run the deleter");
}

int main() {
    TestParameterLifecycle();
    TestOfflineCalls();
    TestFriendModel();
    TestIntrusivePtr();
    TestVectorLayout();
    TestResponseAndFriendVectors();
    return 0;
}
