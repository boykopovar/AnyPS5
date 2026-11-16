#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using NKWebKit::WebCoreAbi::CredentialBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "CredentialBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static StringImpl Make(const char* text) {
    return StringImpl{4, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static void TestDefault() {
    alignas(CredentialBase) unsigned char storage[sizeof(CredentialBase)];
    std::memset(storage, 0xAB, sizeof(storage));
    auto* credential = reinterpret_cast<CredentialBase*>(storage);
    _ZN7WebCore14CredentialBaseC1Ev(credential);
    Require(credential->user.impl && credential->user.impl->length == 0);
    Require(credential->password.impl && credential->password.impl->length == 0);
    Require(_ZNK7WebCore14CredentialBase7isEmptyEv(credential));
    Require(!_ZNK7WebCore14CredentialBase11hasPasswordEv(credential));
    Require(_ZNK7WebCore14CredentialBase11persistenceEv(credential) == 0);
    Require(&_ZNK7WebCore14CredentialBase4userEv(credential) == &credential->user);
    Require(&_ZNK7WebCore14CredentialBase8passwordEv(credential) == &credential->password);

    std::memset(storage, 0xCD, sizeof(storage));
    _ZN7WebCore14CredentialBaseC2Ev(credential);
    Require(credential->user.impl && credential->user.impl->length == 0);
    Require(_ZNK7WebCore14CredentialBase11persistenceEv(credential) == 0);
}

static void TestValues() {
    auto user = Make("alice");
    auto password = Make("secret");
    String userString(&user);
    String passwordString(&password);
    user.refCount = password.refCount = 4;

    alignas(CredentialBase) unsigned char storage[sizeof(CredentialBase)];
    auto* credential = reinterpret_cast<CredentialBase*>(storage);
    _ZN7WebCore14CredentialBaseC1ERKN3WTF6StringES4_NS_21CredentialPersistenceE(credential, &userString, &passwordString, 2);
    Require(credential->user.impl == &user && credential->password.impl == &password);
    Require(user.refCount == 6 && password.refCount == 6);
    Require(!_ZNK7WebCore14CredentialBase7isEmptyEv(credential));
    Require(_ZNK7WebCore14CredentialBase11hasPasswordEv(credential));
    Require(_ZNK7WebCore14CredentialBase11persistenceEv(credential) == 2);

    alignas(CredentialBase) unsigned char otherStorage[sizeof(CredentialBase)];
    auto* other = reinterpret_cast<CredentialBase*>(otherStorage);
    _ZN7WebCore14CredentialBaseC2ERKN3WTF6StringES4_NS_21CredentialPersistenceE(other, &userString, &passwordString, 2);
    Require(_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, other));
    Require(_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, credential));

    other->persistence = NKWebKit::WebCoreAbi::CredentialPersistenceForSession;
    Require(!_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, other));
    other->persistence = NKWebKit::WebCoreAbi::CredentialPersistencePermanent;
    Require(_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, other));

    auto differentText = Make("secreT");
    differentText.refCount = 4;
    other->password.impl = &differentText;
    Require(!_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, other));
    other->password.impl = &password;
    auto differentUser = Make("bob");
    differentUser.refCount = 4;
    other->user.impl = &differentUser;
    Require(!_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, other));
    other->user.impl = &user;
    Require(_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, other));
}

static void TestEmptyNormalization() {
    String nullString;
    auto onlyUser = Make("carol");
    String userString(&onlyUser);
    onlyUser.refCount = 4;

    alignas(CredentialBase) unsigned char storage[sizeof(CredentialBase)];
    auto* credential = reinterpret_cast<CredentialBase*>(storage);
    _ZN7WebCore14CredentialBaseC1ERKN3WTF6StringES4_NS_21CredentialPersistenceE(credential, &userString, &nullString, 1);
    Require(credential->password.impl && credential->password.impl->length == 0);
    Require(!_ZNK7WebCore14CredentialBase7isEmptyEv(credential));
    Require(!_ZNK7WebCore14CredentialBase11hasPasswordEv(credential));

    alignas(CredentialBase) unsigned char emptyStorage[sizeof(CredentialBase)];
    auto* empty = reinterpret_cast<CredentialBase*>(emptyStorage);
    _ZN7WebCore14CredentialBaseC1ERKN3WTF6StringES4_NS_21CredentialPersistenceE(empty, &nullString, &nullString, 1);
    Require(empty->user.impl && empty->user.impl->length == 0);
    Require(_ZNK7WebCore14CredentialBase7isEmptyEv(empty));
    Require(!_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(credential, empty));

    alignas(CredentialBase) unsigned char defaultStorage[sizeof(CredentialBase)];
    auto* fresh = reinterpret_cast<CredentialBase*>(defaultStorage);
    _ZN7WebCore14CredentialBaseC1Ev(fresh);
    Require(!_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(fresh, empty));
    empty->persistence = NKWebKit::WebCoreAbi::CredentialPersistenceNone;
    Require(_ZN7WebCore14CredentialBase7compareERKNS_10CredentialES3_(fresh, empty));
}

static void TestNull() {
    bool threw = false;
    try {
        _ZNK7WebCore14CredentialBase4userEv(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    alignas(CredentialBase) unsigned char storage[sizeof(CredentialBase)];
    auto* credential = reinterpret_cast<CredentialBase*>(storage);
    threw = false;
    try {
        _ZN7WebCore14CredentialBaseC1ERKN3WTF6StringES4_NS_21CredentialPersistenceE(credential, nullptr, nullptr, 0);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);

    threw = false;
    try {
        _ZN7WebCore14CredentialBaseC1Ev(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    TestDefault();
    TestValues();
    TestEmptyNormalization();
    TestNull();
    return 0;
}
