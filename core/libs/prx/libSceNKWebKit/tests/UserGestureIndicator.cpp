#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstddef>
#include <stdexcept>
#include <thread>
#include <vector>

using NKWebKit::WebCoreAbi::DefaultWeakPtrImpl;
using NKWebKit::WebCoreAbi::OptionalUUID;
using NKWebKit::WebCoreAbi::TokenCallableWrapper;
using NKWebKit::WebCoreAbi::TokenCallableWrapperVtable;
using NKWebKit::WebCoreAbi::TokenFunction;
using NKWebKit::WebCoreAbi::UUID;
using NKWebKit::WebCoreAbi::UserGestureTokenRef;
using NKWebKit::WebCoreAbi::WeakPtrImplWithEventTargetData;
using WebCore::DOMPasteAccessPolicy;
using WebCore::GestureScope;
using WebCore::IsPropagatedFromFetch;
using WebCore::UserGestureIndicator;
using WebCore::UserGestureToken;

using TokenConstructor = void(APS5_VABI*)(UserGestureIndicator*, const UserGestureTokenRef&, GestureScope, IsPropagatedFromFetch);

static TokenConstructor RefPtrConstructorC1 = _ZN7WebCore20UserGestureIndicatorC1EN3WTF6RefPtrINS_16UserGestureTokenENS1_13DumbPtrTraitsIS3_EEEENS3_12GestureScopeENS3_21IsPropagatedFromFetchE;
static TokenConstructor RefPtrConstructorC2 = _ZN7WebCore20UserGestureIndicatorC2EN3WTF6RefPtrINS_16UserGestureTokenENS1_13DumbPtrTraitsIS3_EEEENS3_12GestureScopeENS3_21IsPropagatedFromFetchE;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "UserGestureIndicator check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static UserGestureToken* MakeToken(unsigned refCount) {
    auto* token = static_cast<UserGestureToken*>(_ZN3WTF10fastMallocEm(sizeof(UserGestureToken)));
    std::memset(token, 0, sizeof(UserGestureToken));
    token->refCount = refCount;
    return token;
}

static WeakPtrImplWithEventTargetData* MakeEventTargetImpl(unsigned refCount) {
    auto* impl = static_cast<WeakPtrImplWithEventTargetData*>(_ZN3WTF16fastZeroedMallocEm(sizeof(WeakPtrImplWithEventTargetData)));
    impl->refCount = refCount;
    return impl;
}

static DefaultWeakPtrImpl* MakeFactoryImpl(unsigned refCount) {
    auto* impl = static_cast<DefaultWeakPtrImpl*>(_ZN3WTF16fastZeroedMallocEm(sizeof(DefaultWeakPtrImpl)));
    impl->refCount = refCount;
    return impl;
}

static void SetFactory(UserGestureToken* token, DefaultWeakPtrImpl* impl) {
    token->weakPtrFactoryData = static_cast<std::uint64_t>(reinterpret_cast<std::uintptr_t>(impl));
}

struct ObserverRecord {
    TokenCallableWrapper* wrapper;
    UserGestureToken* token;
    GestureScope scopeAtCall;
    DOMPasteAccessPolicy policyAtCall;
    unsigned firstImplRefCountAtCall;
};

static std::vector<ObserverRecord> s_observerCalls;
static std::vector<TokenCallableWrapper*> s_destroyedWrappers;

static void APS5_VABI ObserverCompleteDestructor(TokenCallableWrapper*) {
}

static void APS5_VABI ObserverDeletingDestructor(TokenCallableWrapper* self) {
    s_destroyedWrappers.push_back(self);
    _ZN3WTF8fastFreeEPv(self);
}

static void APS5_VABI ObserverCall(TokenCallableWrapper* self, UserGestureToken& token) {
    ObserverRecord record{self, &token, token.scope, token.domPasteAccessPolicy, 0};
    if (token.documentsImpactedByUserGesture.table) {
        auto* buckets = static_cast<WeakPtrImplWithEventTargetData**>(token.documentsImpactedByUserGesture.table);
        if (buckets[0]) record.firstImplRefCountAtCall = buckets[0]->refCount;
    }
    s_observerCalls.push_back(record);
}

static const TokenCallableWrapperVtable s_observerVtable = {
    &ObserverCompleteDestructor,
    &ObserverDeletingDestructor,
    &ObserverCall,
};

static void AddObservers(UserGestureToken* token, unsigned count) {
    auto* buffer = static_cast<TokenFunction*>(_ZN3WTF10fastMallocEm(count * sizeof(TokenFunction)));
    for (unsigned i = 0; i < count; ++i) {
        auto* wrapper = static_cast<TokenCallableWrapper*>(_ZN3WTF10fastMallocEm(sizeof(TokenCallableWrapper)));
        wrapper->vtable = &s_observerVtable;
        buffer[i].callableWrapper = wrapper;
    }
    token->destructionObserversBuffer = buffer;
    token->destructionObserversCapacity = count;
    token->destructionObserversSize = count;
}

static constexpr unsigned kBucketCount = 4;

static void SetWeakSet(UserGestureToken* token, WeakPtrImplWithEventTargetData* first, WeakPtrImplWithEventTargetData* last) {
    auto* raw = _ZN3WTF16fastZeroedMallocEm(16 + kBucketCount * sizeof(void*));
    auto* buckets = reinterpret_cast<void**>(static_cast<unsigned char*>(raw) + 16);
    reinterpret_cast<unsigned*>(buckets)[-1] = kBucketCount;
    buckets[0] = first;
    buckets[1] = nullptr;
    buckets[2] = reinterpret_cast<void*>(static_cast<std::uintptr_t>(-1));
    buckets[3] = last;
    token->documentsImpactedByUserGesture.table = buckets;
}

static void CallConstructor(TokenConstructor constructor, UserGestureIndicator* indicator, UserGestureToken* token, GestureScope scope, IsPropagatedFromFetch isPropagatedFromFetch) {
    if (token) ++token->refCount;
    UserGestureTokenRef ref{token};
    constructor(indicator, ref, scope, isPropagatedFromFetch);
    if (token) --token->refCount;
}

static UserGestureIndicator* MakeIndicatorStorage() {
    auto* storage = static_cast<UserGestureIndicator*>(_ZN3WTF10fastMallocEm(sizeof(UserGestureIndicator)));
    std::memset(storage, 0xAB, sizeof(UserGestureIndicator));
    return storage;
}

static void FreeIndicatorStorage(UserGestureIndicator* indicator) {
    _ZN3WTF8fastFreeEPv(indicator);
}

static bool CurrentTokenHolds(UserGestureToken* token) {
    UserGestureIndicator* probe = MakeIndicatorStorage();
    CallConstructor(RefPtrConstructorC2, probe, nullptr, GestureScope::All, IsPropagatedFromFetch::No);
    bool holds = probe->previousToken == token;
    _ZN7WebCore20UserGestureIndicatorD2Ev(probe);
    FreeIndicatorStorage(probe);
    return holds;
}

int main() {
    _ZN3WTF20initializeMainThreadEv();

    {
        Require(sizeof(UUID) == 16);
        Require(alignof(UUID) == 16);
        Require(sizeof(OptionalUUID) == 32);
        Require(alignof(OptionalUUID) == 16);
        Require(offsetof(OptionalUUID, value) == 0);
        Require(offsetof(OptionalUUID, hasValue) == 16);
    }

    {
        UserGestureToken* token = MakeToken(0);
        token->scope = GestureScope::MediaOnly;
        token->isPropagatedFromFetch = IsPropagatedFromFetch::Yes;
        token->domPasteAccessPolicy = DOMPasteAccessPolicy::Granted;
        AddObservers(token, 1);
        UserGestureIndicator* indicator = MakeIndicatorStorage();
        CallConstructor(RefPtrConstructorC2, indicator, token, GestureScope::MediaOnly, IsPropagatedFromFetch::Yes);
        Require(token->refCount == 1);
        Require(token->scope == GestureScope::MediaOnly);
        Require(token->isPropagatedFromFetch == IsPropagatedFromFetch::Yes);
        Require(indicator->previousToken == nullptr);
        std::size_t observedBefore = s_observerCalls.size();
        _ZN7WebCore20UserGestureIndicatorD2Ev(indicator);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(s_observerCalls[observedBefore].token == token);
        Require(s_observerCalls[observedBefore].scopeAtCall == GestureScope::All);
        Require(s_observerCalls[observedBefore].policyAtCall == DOMPasteAccessPolicy::NotRequestedYet);
        Require(CurrentTokenHolds(nullptr));
        FreeIndicatorStorage(indicator);
    }

    {
        UserGestureToken* token = MakeToken(0);
        UserGestureIndicator* indicator = MakeIndicatorStorage();
        CallConstructor(RefPtrConstructorC1, indicator, token, GestureScope::All, IsPropagatedFromFetch::No);
        Require(token->refCount == 1);
        Require(indicator->previousToken == nullptr);
        _ZN7WebCore20UserGestureIndicatorD1Ev(indicator);
        Require(CurrentTokenHolds(nullptr));
        FreeIndicatorStorage(indicator);
    }

    {
        UserGestureToken* first = MakeToken(0);
        UserGestureToken* second = MakeToken(0);
        AddObservers(first, 1);
        AddObservers(second, 1);
        UserGestureIndicator* firstIndicator = MakeIndicatorStorage();
        UserGestureIndicator* secondIndicator = MakeIndicatorStorage();
        CallConstructor(RefPtrConstructorC2, firstIndicator, first, GestureScope::All, IsPropagatedFromFetch::No);
        Require(first->refCount == 1);
        Require(firstIndicator->previousToken == nullptr);
        CallConstructor(RefPtrConstructorC2, secondIndicator, second, GestureScope::All, IsPropagatedFromFetch::No);
        Require(first->refCount == 1);
        Require(second->refCount == 1);
        Require(secondIndicator->previousToken == first);
        std::size_t observedBefore = s_observerCalls.size();
        _ZN7WebCore20UserGestureIndicatorD2Ev(secondIndicator);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(s_observerCalls[observedBefore].token == second);
        Require(first->refCount == 1);
        Require(CurrentTokenHolds(first));
        _ZN7WebCore20UserGestureIndicatorD2Ev(firstIndicator);
        Require(s_observerCalls.size() == observedBefore + 2);
        Require(s_observerCalls[observedBefore + 1].token == first);
        Require(CurrentTokenHolds(nullptr));
        FreeIndicatorStorage(firstIndicator);
        FreeIndicatorStorage(secondIndicator);
    }

    {
        UserGestureToken* token = MakeToken(0);
        token->scope = GestureScope::MediaOnly;
        AddObservers(token, 1);
        UserGestureIndicator* offThreadIndicator = MakeIndicatorStorage();
        std::thread constructorWorker([&] {
            CallConstructor(RefPtrConstructorC2, offThreadIndicator, token, GestureScope::MediaOnly, IsPropagatedFromFetch::Yes);
        });
        constructorWorker.join();
        Require(token->refCount == 0);
        Require(offThreadIndicator->previousToken == nullptr);
        Require(token->scope == GestureScope::MediaOnly);
        Require(!CurrentTokenHolds(token));
        UserGestureIndicator* mainIndicator = MakeIndicatorStorage();
        CallConstructor(RefPtrConstructorC2, mainIndicator, token, GestureScope::MediaOnly, IsPropagatedFromFetch::Yes);
        Require(token->refCount == 1);
        Require(token->scope == GestureScope::MediaOnly);
        std::thread destructorWorker([&] {
            _ZN7WebCore20UserGestureIndicatorD2Ev(mainIndicator);
        });
        destructorWorker.join();
        Require(token->refCount == 1);
        Require(token->scope == GestureScope::MediaOnly);
        Require(CurrentTokenHolds(token));
        std::size_t observedBefore = s_observerCalls.size();
        _ZN7WebCore20UserGestureIndicatorD2Ev(offThreadIndicator);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(s_observerCalls[observedBefore].token == token);
        Require(s_observerCalls[observedBefore].scopeAtCall == GestureScope::All);
        Require(CurrentTokenHolds(nullptr));
        FreeIndicatorStorage(offThreadIndicator);
        FreeIndicatorStorage(mainIndicator);
    }

    {
        UserGestureToken* token = MakeToken(0);
        token->scope = GestureScope::MediaOnly;
        AddObservers(token, 2);
        WeakPtrImplWithEventTargetData* firstImpl = MakeEventTargetImpl(2);
        WeakPtrImplWithEventTargetData* lastImpl = MakeEventTargetImpl(2);
        SetWeakSet(token, firstImpl, lastImpl);
        DefaultWeakPtrImpl* factoryImpl = MakeFactoryImpl(2);
        SetFactory(token, factoryImpl);
        UserGestureIndicator* indicator = MakeIndicatorStorage();
        CallConstructor(RefPtrConstructorC2, indicator, token, GestureScope::MediaOnly, IsPropagatedFromFetch::Yes);
        Require(token->refCount == 1);
        std::size_t observedBefore = s_observerCalls.size();
        std::size_t wrappersBefore = s_destroyedWrappers.size();
        _ZN7WebCore20UserGestureIndicatorD2Ev(indicator);
        Require(s_observerCalls.size() == observedBefore + 2);
        Require(s_observerCalls[observedBefore].token == token);
        Require(s_observerCalls[observedBefore + 1].token == token);
        Require(s_observerCalls[observedBefore].scopeAtCall == GestureScope::All);
        Require(s_observerCalls[observedBefore].firstImplRefCountAtCall == 2);
        Require(s_observerCalls[observedBefore + 1].firstImplRefCountAtCall == 2);
        Require(firstImpl->refCount == 1);
        Require(lastImpl->refCount == 1);
        Require(factoryImpl->refCount == 1);
        Require(factoryImpl->ptr == nullptr);
        Require(s_destroyedWrappers.size() == wrappersBefore + 2);
        Require(CurrentTokenHolds(nullptr));
        FreeIndicatorStorage(indicator);
        _ZN3WTF8fastFreeEPv(firstImpl);
        _ZN3WTF8fastFreeEPv(lastImpl);
        _ZN3WTF8fastFreeEPv(factoryImpl);
    }

    {
        UserGestureToken* token = MakeToken(1);
        AddObservers(token, 1);
        WeakPtrImplWithEventTargetData* impl = MakeEventTargetImpl(3);
        SetWeakSet(token, impl, nullptr);
        std::size_t observedBefore = s_observerCalls.size();
        _ZN7WebCore16UserGestureTokenD2Ev(token);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(impl->refCount == 2);
        _ZN3WTF8fastFreeEPv(token);
        _ZN3WTF8fastFreeEPv(impl);
    }

    {
        UserGestureToken* token = MakeToken(1);
        AddObservers(token, 1);
        WeakPtrImplWithEventTargetData* impl = MakeEventTargetImpl(3);
        SetWeakSet(token, impl, nullptr);
        std::size_t observedBefore = s_observerCalls.size();
        _ZN7WebCore16UserGestureTokenD1Ev(token);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(s_observerCalls[observedBefore].token == token);
        Require(impl->refCount == 2);
        _ZN3WTF8fastFreeEPv(token);
        _ZN3WTF8fastFreeEPv(impl);
    }

    {
        UserGestureToken* token = MakeToken(1);
        AddObservers(token, 1);
        WeakPtrImplWithEventTargetData* impl = MakeEventTargetImpl(1);
        SetWeakSet(token, impl, nullptr);
        std::size_t observedBefore = s_observerCalls.size();
        bool threw = false;
        try {
            _ZN7WebCore16UserGestureTokenD2Ev(token);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(impl->refCount == 1);
        _ZN3WTF8fastFreeEPv(token);
        _ZN3WTF8fastFreeEPv(impl);
    }

    {
        UserGestureToken* token = MakeToken(1);
        AddObservers(token, 1);
        WeakPtrImplWithEventTargetData* impl = MakeEventTargetImpl(1);
        SetWeakSet(token, impl, nullptr);
        std::size_t observedBefore = s_observerCalls.size();
        bool threw = false;
        try {
            _ZN7WebCore16UserGestureTokenD1Ev(token);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(impl->refCount == 1);
        _ZN3WTF8fastFreeEPv(token);
        _ZN3WTF8fastFreeEPv(impl);
    }

    {
        UserGestureToken* token = MakeToken(0);
        AddObservers(token, 1);
        WeakPtrImplWithEventTargetData* garbage = MakeEventTargetImpl(1);
        SetWeakSet(token, garbage, nullptr);
        UserGestureIndicator* indicator = MakeIndicatorStorage();
        CallConstructor(RefPtrConstructorC2, indicator, token, GestureScope::All, IsPropagatedFromFetch::No);
        Require(token->refCount == 1);
        std::size_t observedBefore = s_observerCalls.size();
        bool threw = false;
        try {
            _ZN7WebCore20UserGestureIndicatorD2Ev(indicator);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(garbage->refCount == 1);
        Require(CurrentTokenHolds(token));
        FreeIndicatorStorage(indicator);
    }

    {
        UserGestureToken* token = MakeToken(1);
        AddObservers(token, 1);
        WeakPtrImplWithEventTargetData* firstImpl = MakeEventTargetImpl(3);
        WeakPtrImplWithEventTargetData* lastImpl = MakeEventTargetImpl(1);
        SetWeakSet(token, firstImpl, lastImpl);
        void* table = token->documentsImpactedByUserGesture.table;
        std::size_t observedBefore = s_observerCalls.size();
        bool threw = false;
        try {
            _ZN7WebCore16UserGestureTokenD2Ev(token);
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(firstImpl->refCount == 3);
        Require(lastImpl->refCount == 1);
        Require(token->documentsImpactedByUserGesture.table == table);
        _ZN3WTF8fastFreeEPv(token);
        _ZN3WTF8fastFreeEPv(firstImpl);
        _ZN3WTF8fastFreeEPv(lastImpl);
        _ZN3WTF8fastFreeEPv(reinterpret_cast<unsigned char*>(table) - 16);
    }

    {
        UserGestureToken* token = MakeToken(1);
        unsigned count = 2;
        auto* buffer = static_cast<TokenFunction*>(_ZN3WTF10fastMallocEm(count * sizeof(TokenFunction)));
        auto* wrapper = static_cast<TokenCallableWrapper*>(_ZN3WTF10fastMallocEm(sizeof(TokenCallableWrapper)));
        wrapper->vtable = &s_observerVtable;
        buffer[0].callableWrapper = wrapper;
        buffer[1].callableWrapper = nullptr;
        token->destructionObserversBuffer = buffer;
        token->destructionObserversCapacity = count;
        token->destructionObserversSize = count;
        std::size_t observedBefore = s_observerCalls.size();
        bool threw = false;
        try {
            _ZN7WebCore16UserGestureTokenD2Ev(token);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw);
        Require(s_observerCalls.size() == observedBefore + 1);
        Require(s_observerCalls[observedBefore].token == token);
        wrapper->vtable->deletingDestructor(wrapper);
        _ZN3WTF8fastFreeEPv(buffer);
        _ZN3WTF8fastFreeEPv(token);
    }

    {
        bool threw = false;
        try {
            _ZN7WebCore20UserGestureIndicatorC1EN3WTF8OptionalINS_26ProcessingUserGestureStateEEEPNS_8DocumentENS_15UserGestureTypeENS0_23ProcessInteractionStyleE();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            _ZN7WebCore20UserGestureIndicatorC2EN3WTF8OptionalINS_26ProcessingUserGestureStateEEEPNS_8DocumentENS_15UserGestureTypeENS0_23ProcessInteractionStyleE();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            _ZN7WebCore26UserTypingGestureIndicatorC1ERNS_5FrameE();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            _ZN7WebCore26UserTypingGestureIndicatorC2ERNS_5FrameE();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            _ZN7WebCore26UserTypingGestureIndicatorD1Ev();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
        threw = false;
        try {
            _ZN7WebCore26UserTypingGestureIndicatorD2Ev();
        } catch (const std::runtime_error&) {
            threw = true;
        }
        Require(threw);
    }

    std::printf("UserGestureIndicator tests passed\n");
    return 0;
}
