#pragma once

#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/src/InspectorFrontendRouterAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfJsonTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::InspectorAbi {

struct SupplementalBackendDispatcher;

enum class CommonErrorCode : int {
    ParseError = 0,
    InvalidRequest,
    MethodNotFound,
    InvalidParams,
    InternalError,
    ServerError,
};

struct DispatcherMapEntry {
    WtfAbi::String key;
    SupplementalBackendDispatcher* value;
};

struct ProtocolError {
    CommonErrorCode code;
    std::uint8_t padding[4];
    WtfAbi::String message;
};

struct BackendDispatcher {
    unsigned refCount;
    std::uint8_t padding0[4];
    FrontendRouter* frontendRouter;
    DispatcherMapEntry* dispatchers;
    ProtocolError* protocolErrors;
    unsigned protocolErrorCapacity;
    unsigned protocolErrorCount;
    bool hasCurrentRequestId;
    std::uint8_t padding1[7];
    std::int64_t currentRequestId;
};

struct SupplementalBackendDispatcherVtable {
    void (APS5_VABI* completeDestructor)(SupplementalBackendDispatcher* self);
    void (APS5_VABI* deletingDestructor)(SupplementalBackendDispatcher* self);
    void (APS5_VABI* dispatch)(SupplementalBackendDispatcher* self, std::int64_t requestId, const WtfAbi::String* method, JsonAbi::Object** message);
};

struct SupplementalBackendDispatcher {
    const SupplementalBackendDispatcherVtable* vptr;
    unsigned refCount;
    std::uint8_t padding[4];
    BackendDispatcher* backendDispatcher;
};

struct CallbackBase {
    unsigned refCount;
    std::uint8_t padding0[4];
    BackendDispatcher* backendDispatcher;
    std::int64_t requestId;
    bool alreadySent;
    std::uint8_t padding1[7];
};

static_assert(sizeof(DispatcherMapEntry) == 16);
static_assert(std::is_standard_layout_v<ProtocolError>);
static_assert(sizeof(ProtocolError) == 16);
static_assert(offsetof(ProtocolError, message) == 8);
static_assert(std::is_standard_layout_v<BackendDispatcher>);
static_assert(sizeof(BackendDispatcher) == 56);
static_assert(offsetof(BackendDispatcher, frontendRouter) == 8);
static_assert(offsetof(BackendDispatcher, dispatchers) == 16);
static_assert(offsetof(BackendDispatcher, protocolErrors) == 24);
static_assert(offsetof(BackendDispatcher, protocolErrorCount) == 36);
static_assert(offsetof(BackendDispatcher, hasCurrentRequestId) == 40);
static_assert(offsetof(BackendDispatcher, currentRequestId) == 48);
static_assert(std::is_standard_layout_v<SupplementalBackendDispatcher>);
static_assert(sizeof(SupplementalBackendDispatcher) == 24);
static_assert(offsetof(SupplementalBackendDispatcher, refCount) == 8);
static_assert(offsetof(SupplementalBackendDispatcher, backendDispatcher) == 16);
static_assert(std::is_standard_layout_v<CallbackBase>);
static_assert(sizeof(CallbackBase) == 32);
static_assert(offsetof(CallbackBase, backendDispatcher) == 8);
static_assert(offsetof(CallbackBase, requestId) == 16);
static_assert(offsetof(CallbackBase, alreadySent) == 24);

BackendDispatcher* CreateBackendDispatcher(FrontendRouter& router);
void RefBackendDispatcher(BackendDispatcher& dispatcher);
void DerefBackendDispatcher(BackendDispatcher* dispatcher);

}  // namespace NKWebKit::InspectorAbi

extern "C" {

void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherC2ERNS_17BackendDispatcherE(NKWebKit::InspectorAbi::SupplementalBackendDispatcher* self, NKWebKit::InspectorAbi::BackendDispatcher* backendDispatcher);
void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherD0Ev(NKWebKit::InspectorAbi::SupplementalBackendDispatcher* self);
void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherD1Ev(NKWebKit::InspectorAbi::SupplementalBackendDispatcher* self);
void APS5_VABI _ZN9Inspector29SupplementalBackendDispatcherD2Ev(NKWebKit::InspectorAbi::SupplementalBackendDispatcher* self);
void APS5_VABI _ZN9Inspector17BackendDispatcherD2Ev(NKWebKit::InspectorAbi::BackendDispatcher* self);
bool APS5_VABI _ZNK9Inspector17BackendDispatcher8isActiveEv(const NKWebKit::InspectorAbi::BackendDispatcher* self);
bool APS5_VABI _ZNK9Inspector17BackendDispatcher17hasProtocolErrorsEv(const NKWebKit::InspectorAbi::BackendDispatcher* self);
void APS5_VABI _ZN9Inspector17BackendDispatcher27registerDispatcherForDomainERKN3WTF6StringEPNS_29SupplementalBackendDispatcherE(NKWebKit::InspectorAbi::BackendDispatcher* self, const NKWebKit::WtfAbi::String* domain, NKWebKit::InspectorAbi::SupplementalBackendDispatcher* dispatcher);
void APS5_VABI _ZN9Inspector17BackendDispatcher8dispatchERKN3WTF6StringE(NKWebKit::InspectorAbi::BackendDispatcher* self, const NKWebKit::WtfAbi::String* message);
void APS5_VABI _ZN9Inspector17BackendDispatcher17sendPendingErrorsEv(NKWebKit::InspectorAbi::BackendDispatcher* self);
void APS5_VABI _ZN9Inspector17BackendDispatcher19reportProtocolErrorENS0_15CommonErrorCodeERKN3WTF6StringE(NKWebKit::InspectorAbi::BackendDispatcher* self, NKWebKit::InspectorAbi::CommonErrorCode code, const NKWebKit::WtfAbi::String* message);
bool APS5_VABI _ZNK9Inspector17BackendDispatcher12CallbackBase8isActiveEv(const NKWebKit::InspectorAbi::CallbackBase* self);
void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBase7disableEv(NKWebKit::InspectorAbi::CallbackBase* self);
void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBase11sendFailureERKN3WTF6StringE(NKWebKit::InspectorAbi::CallbackBase* self, const NKWebKit::WtfAbi::String* error);
void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBaseD1Ev(NKWebKit::InspectorAbi::CallbackBase* self);
void APS5_VABI _ZN9Inspector17BackendDispatcher12CallbackBaseD2Ev(NKWebKit::InspectorAbi::CallbackBase* self);

}
