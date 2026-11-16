#pragma once

#include "SceTypes.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace WebCore {

struct UserGestureToken;

}

namespace NKWebKit::WebCoreAbi {

struct TokenCallableWrapper;

struct TokenCallableWrapperVtable {
    void (APS5_VABI* completeDestructor)(TokenCallableWrapper* self);
    void (APS5_VABI* deletingDestructor)(TokenCallableWrapper* self);
    void (APS5_VABI* call)(TokenCallableWrapper* self, WebCore::UserGestureToken& token);
};

struct TokenCallableWrapper {
    const TokenCallableWrapperVtable* vtable;
};

struct TokenFunction {
    TokenCallableWrapper* callableWrapper;
};

struct DefaultWeakPtrImpl {
    unsigned refCount;
    void* ptr;
};

struct WeakPtrImplWithEventTargetData {
    unsigned refCount;
    void* ptr;
    std::uint8_t eventTargetData[24];
};

struct WeakHashSet {
    void* table;
    unsigned operationCountSinceLastCleanup;
    unsigned maxOperationCountWithoutCleanup;
};

struct UUID {
    unsigned __int128 value;
};

struct OptionalUUID {
    UUID value;
    bool hasValue;
};

struct UserGestureTokenRef {
    WebCore::UserGestureToken* ptr;
};

static_assert(sizeof(TokenCallableWrapperVtable) == 24);
static_assert(offsetof(TokenCallableWrapperVtable, completeDestructor) == 0);
static_assert(offsetof(TokenCallableWrapperVtable, deletingDestructor) == 8);
static_assert(offsetof(TokenCallableWrapperVtable, call) == 16);
static_assert(sizeof(TokenCallableWrapper) == 8);
static_assert(sizeof(TokenFunction) == 8);
static_assert(offsetof(TokenFunction, callableWrapper) == 0);
static_assert(sizeof(DefaultWeakPtrImpl) == 16);
static_assert(offsetof(DefaultWeakPtrImpl, refCount) == 0);
static_assert(offsetof(DefaultWeakPtrImpl, ptr) == 8);
static_assert(sizeof(WeakPtrImplWithEventTargetData) == 40);
static_assert(offsetof(WeakPtrImplWithEventTargetData, refCount) == 0);
static_assert(offsetof(WeakPtrImplWithEventTargetData, ptr) == 8);
static_assert(offsetof(WeakPtrImplWithEventTargetData, eventTargetData) == 16);
static_assert(sizeof(WeakHashSet) == 16);
static_assert(offsetof(WeakHashSet, table) == 0);
static_assert(offsetof(WeakHashSet, operationCountSinceLastCleanup) == 8);
static_assert(offsetof(WeakHashSet, maxOperationCountWithoutCleanup) == 12);
static_assert(sizeof(UUID) == 16);
static_assert(alignof(UUID) == 16);
static_assert(sizeof(OptionalUUID) == 32);
static_assert(alignof(OptionalUUID) == 16);
static_assert(offsetof(OptionalUUID, value) == 0);
static_assert(offsetof(OptionalUUID, hasValue) == 16);
static_assert(sizeof(UserGestureTokenRef) == 8);
static_assert(offsetof(UserGestureTokenRef, ptr) == 0);

}  // namespace NKWebKit::WebCoreAbi

namespace WebCore {

enum class ProcessingUserGestureState : int {
    ProcessingUserGesture,
    ProcessingPotentialUserGesture,
    NotProcessingUserGesture,
};

enum class UserGestureType : int {
    EscapeKey,
    ActivationTriggering,
    Other,
};

enum class GestureScope : int {
    All,
    MediaOnly,
};

enum class IsPropagatedFromFetch : bool {
    No,
    Yes,
};

enum class DOMPasteAccessPolicy : std::uint8_t {
    NotRequestedYet,
    Denied,
    Granted,
};

struct UserGestureToken {
    unsigned refCount;
    std::uint64_t weakPtrFactoryData;
    ProcessingUserGestureState state;
    NKWebKit::WebCoreAbi::TokenFunction* destructionObserversBuffer;
    unsigned destructionObserversCapacity;
    unsigned destructionObserversSize;
    UserGestureType gestureType;
    NKWebKit::WebCoreAbi::WeakHashSet documentsImpactedByUserGesture;
    DOMPasteAccessPolicy domPasteAccessPolicy;
    GestureScope scope;
    double startTime;
    IsPropagatedFromFetch isPropagatedFromFetch;
    NKWebKit::WebCoreAbi::OptionalUUID authorizationToken;
};

struct UserGestureIndicator {
    UserGestureToken* previousToken;
};

static_assert(sizeof(ProcessingUserGestureState) == 4);
static_assert(sizeof(UserGestureType) == 4);
static_assert(sizeof(GestureScope) == 4);
static_assert(sizeof(IsPropagatedFromFetch) == 1);
static_assert(sizeof(DOMPasteAccessPolicy) == 1);
static_assert(sizeof(UserGestureToken) == 128);
static_assert(alignof(UserGestureToken) == 16);
static_assert(offsetof(UserGestureToken, refCount) == 0);
static_assert(offsetof(UserGestureToken, weakPtrFactoryData) == 8);
static_assert(offsetof(UserGestureToken, state) == 16);
static_assert(offsetof(UserGestureToken, destructionObserversBuffer) == 24);
static_assert(offsetof(UserGestureToken, destructionObserversCapacity) == 32);
static_assert(offsetof(UserGestureToken, destructionObserversSize) == 36);
static_assert(offsetof(UserGestureToken, gestureType) == 40);
static_assert(offsetof(UserGestureToken, documentsImpactedByUserGesture) == 48);
static_assert(offsetof(UserGestureToken, domPasteAccessPolicy) == 64);
static_assert(offsetof(UserGestureToken, scope) == 68);
static_assert(offsetof(UserGestureToken, startTime) == 72);
static_assert(offsetof(UserGestureToken, isPropagatedFromFetch) == 80);
static_assert(offsetof(UserGestureToken, authorizationToken) == 96);
static_assert(sizeof(UserGestureIndicator) == 8);
static_assert(offsetof(UserGestureIndicator, previousToken) == 0);
static_assert(std::is_standard_layout_v<UserGestureToken>);

}  // namespace WebCore
