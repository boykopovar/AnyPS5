#pragma once

#include "SceTypes.hpp"
#include "prx/libSceNKWebKit/src/WtfStringAbi.hpp"
#include "prx/libSceNKWebKit/src/WtfVector.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace NKWebKit::Csp {

struct DirectiveList;
struct Source;

}  // namespace NKWebKit::Csp

namespace NKWebKit::WebCoreAbi {

struct ContentSecurityPolicyClient;

struct ContentSecurityPolicyClientVtable {
    void (APS5_VABI* willSendCSPViolationReport)(ContentSecurityPolicyClient* self, void* info);
    void (APS5_VABI* completeDestructor)(ContentSecurityPolicyClient* self);
    void (APS5_VABI* deletingDestructor)(ContentSecurityPolicyClient* self);
    void (APS5_VABI* addConsoleMessage)(ContentSecurityPolicyClient* self, std::uint8_t source, std::uint8_t level, const WtfAbi::String* message, std::uint64_t requestIdentifier);
    void (APS5_VABI* sendCSPViolationReport)(ContentSecurityPolicyClient* self, WtfAbi::URL* url, void* report);
    void (APS5_VABI* enqueueSecurityPolicyViolationEvent)(ContentSecurityPolicyClient* self, void* init);
};

struct ContentSecurityPolicyClient {
    const ContentSecurityPolicyClientVtable* vtable;
};

constexpr std::uint8_t kMessageSourceSecurity = 8;
constexpr std::uint8_t kMessageLevelError = 2;

struct LegacyContentSecurityPolicyHeader {
    WtfAbi::String text;
    bool enforce;
};

struct LegacyContentSecurityPolicyResponseHeaders {
    WtfAbi::Vector<LegacyContentSecurityPolicyHeader> headers;
    int httpStatusCode{0};
    bool emptyForMarkable{false};
};

struct LegacySecurityOriginData {
    WtfAbi::StringImpl* protocol;
    WtfAbi::StringImpl* host;
    bool hasPort;
    std::uint16_t port;
};

struct ContentSecurityPolicy {
    void* scriptExecutionContext;
    ContentSecurityPolicyClient* client;
    WtfAbi::URL protectedURL;
    Csp::Source* selfSource;
    WtfAbi::String selfSourceProtocol;
    WtfAbi::Vector<Csp::DirectiveList*> policies;
    WtfAbi::String lastPolicyEvalDisabledErrorMessage;
    WtfAbi::String lastPolicyWebAssemblyDisabledErrorMessage;
    WtfAbi::String referrer;
    int sandboxFlags;
    bool overrideInlineStyleAllowed;
    bool isReportingEnabled;
    bool upgradeInsecureRequests;
    bool hasAPIPolicy;
    int httpStatusCode;
    unsigned hashAlgorithmsForInlineScripts;
    unsigned hashAlgorithmsForInlineStylesheets;
    LegacySecurityOriginData* insecureNavigationRequestsToUpgrade;
    bool hasCachedResponseHeaders;
    alignas(8) unsigned char cachedResponseHeaders[sizeof(LegacyContentSecurityPolicyResponseHeaders)];
};

constexpr unsigned kURLValidBit = 1u << 0;
constexpr unsigned kURLPortLengthShift = 3;
constexpr unsigned kURLPortLengthMask = 7;

constexpr int kInsecureRequestTypeLoad = 0;
constexpr int kInsecureRequestTypeFormSubmission = 1;
constexpr int kInsecureRequestTypeNavigation = 2;

static_assert(sizeof(ContentSecurityPolicyClientVtable) == 48);
static_assert(offsetof(ContentSecurityPolicyClientVtable, willSendCSPViolationReport) == 0);
static_assert(offsetof(ContentSecurityPolicyClientVtable, completeDestructor) == 8);
static_assert(offsetof(ContentSecurityPolicyClientVtable, deletingDestructor) == 16);
static_assert(offsetof(ContentSecurityPolicyClientVtable, addConsoleMessage) == 24);
static_assert(offsetof(ContentSecurityPolicyClientVtable, sendCSPViolationReport) == 32);
static_assert(offsetof(ContentSecurityPolicyClientVtable, enqueueSecurityPolicyViolationEvent) == 40);
static_assert(sizeof(ContentSecurityPolicyClient) == 8);
static_assert(sizeof(LegacyContentSecurityPolicyHeader) == 16);
static_assert(offsetof(LegacyContentSecurityPolicyHeader, text) == 0);
static_assert(offsetof(LegacyContentSecurityPolicyHeader, enforce) == 8);
static_assert(sizeof(LegacyContentSecurityPolicyResponseHeaders) == 24);
static_assert(alignof(LegacyContentSecurityPolicyResponseHeaders) == 8);
static_assert(offsetof(LegacyContentSecurityPolicyResponseHeaders, headers) == 0);
static_assert(offsetof(LegacyContentSecurityPolicyResponseHeaders, httpStatusCode) == 16);
static_assert(offsetof(LegacyContentSecurityPolicyResponseHeaders, emptyForMarkable) == 20);
static_assert(sizeof(LegacySecurityOriginData) == 24);
static_assert(offsetof(LegacySecurityOriginData, protocol) == 0);
static_assert(offsetof(LegacySecurityOriginData, host) == 8);
static_assert(offsetof(LegacySecurityOriginData, hasPort) == 16);
static_assert(offsetof(LegacySecurityOriginData, port) == 18);
static_assert(sizeof(ContentSecurityPolicy) == 176);
static_assert(alignof(ContentSecurityPolicy) == 8);
static_assert(offsetof(ContentSecurityPolicy, scriptExecutionContext) == 0);
static_assert(offsetof(ContentSecurityPolicy, client) == 8);
static_assert(offsetof(ContentSecurityPolicy, protectedURL) == 16);
static_assert(offsetof(ContentSecurityPolicy, selfSource) == 56);
static_assert(offsetof(ContentSecurityPolicy, selfSourceProtocol) == 64);
static_assert(offsetof(ContentSecurityPolicy, policies) == 72);
static_assert(offsetof(ContentSecurityPolicy, lastPolicyEvalDisabledErrorMessage) == 88);
static_assert(offsetof(ContentSecurityPolicy, lastPolicyWebAssemblyDisabledErrorMessage) == 96);
static_assert(offsetof(ContentSecurityPolicy, referrer) == 104);
static_assert(offsetof(ContentSecurityPolicy, sandboxFlags) == 112);
static_assert(offsetof(ContentSecurityPolicy, overrideInlineStyleAllowed) == 116);
static_assert(offsetof(ContentSecurityPolicy, isReportingEnabled) == 117);
static_assert(offsetof(ContentSecurityPolicy, upgradeInsecureRequests) == 118);
static_assert(offsetof(ContentSecurityPolicy, hasAPIPolicy) == 119);
static_assert(offsetof(ContentSecurityPolicy, httpStatusCode) == 120);
static_assert(offsetof(ContentSecurityPolicy, hashAlgorithmsForInlineScripts) == 124);
static_assert(offsetof(ContentSecurityPolicy, hashAlgorithmsForInlineStylesheets) == 128);
static_assert(offsetof(ContentSecurityPolicy, insecureNavigationRequestsToUpgrade) == 136);
static_assert(offsetof(ContentSecurityPolicy, hasCachedResponseHeaders) == 144);
static_assert(offsetof(ContentSecurityPolicy, cachedResponseHeaders) == 152);

}  // namespace NKWebKit::WebCoreAbi
