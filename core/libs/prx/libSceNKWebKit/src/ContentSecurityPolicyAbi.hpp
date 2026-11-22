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

namespace NKWebKit {

bool SchemeShouldBypassContentSecurityPolicy(const WtfAbi::StringImpl* scheme);

}  // namespace NKWebKit

namespace NKWebKit::WebCoreAbi {

struct ContentSecurityPolicyClient;
struct LegacyCSPInfo;
struct LegacyFormData;
struct LegacySecurityPolicyViolationEventInit;

struct ContentSecurityPolicyClientVtable {
    void (APS5_VABI* willSendCSPViolationReport)(ContentSecurityPolicyClient* self, LegacyCSPInfo* info);
    void (APS5_VABI* completeDestructor)(ContentSecurityPolicyClient* self);
    void (APS5_VABI* deletingDestructor)(ContentSecurityPolicyClient* self);
    void (APS5_VABI* addConsoleMessage)(ContentSecurityPolicyClient* self, std::uint8_t source, std::uint8_t level, const WtfAbi::String* message, std::uint64_t requestIdentifier);
    void (APS5_VABI* sendCSPViolationReport)(ContentSecurityPolicyClient* self, WtfAbi::URL* url, LegacyFormData** report);
    void (APS5_VABI* enqueueSecurityPolicyViolationEvent)(ContentSecurityPolicyClient* self, LegacySecurityPolicyViolationEventInit* init);
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

struct LegacyCSPInfo {
    WtfAbi::String documentURI;
    WtfAbi::String sourceFile;
    int lineNumber{0};
    int columnNumber{0};
};

struct LegacySecurityPolicyViolationEventInit {
    bool bubbles{false};
    bool cancelable{false};
    bool composed{false};
    WtfAbi::String documentURI;
    WtfAbi::String referrer;
    WtfAbi::String blockedURI;
    WtfAbi::String violatedDirective;
    WtfAbi::String effectiveDirective;
    WtfAbi::String originalPolicy;
    WtfAbi::String sourceFile;
    unsigned short statusCode{0};
    int lineNumber{0};
    int columnNumber{0};
};

struct LegacyFormDataElement {
    char* buffer;
    unsigned capacity;
    unsigned size;
    unsigned char otherAlternatives[24];
    signed char index;
};

struct LegacyFormData {
    unsigned refCount;
    LegacyFormDataElement* elements;
    unsigned elementsCapacity;
    unsigned elementsSize;
    std::int64_t identifier;
    bool alwaysStream;
    char* boundary;
    unsigned boundaryCapacity;
    unsigned boundarySize;
    bool containsPasswordData;
    alignas(8) bool hasLengthInBytes;
    std::uint64_t lengthInBytes;
};

struct LegacySecurityOrigin {
    unsigned refCount;
    WtfAbi::StringImpl* protocol;
    WtfAbi::StringImpl* host;
    bool hasPort;
    std::uint16_t port;
    WtfAbi::StringImpl* domain;
    WtfAbi::StringImpl* filePath;
    bool isUnique;
    bool universalAccess;
    bool domainWasSetInDOM;
    bool canLoadLocalResources;
    int storageBlockingPolicy;
    bool enforcesFilePathSeparation;
    bool needsStorageAccessFromFileURLsQuirk;
    bool isPotentiallyTrustworthy;
    bool isLocal;
};

struct LegacySecurityOriginVector {
    LegacySecurityOrigin** buffer;
    unsigned capacity;
    unsigned size;
};

static_assert(sizeof(LegacyCSPInfo) == 24);
static_assert(offsetof(LegacyCSPInfo, documentURI) == 0);
static_assert(offsetof(LegacyCSPInfo, sourceFile) == 8);
static_assert(offsetof(LegacyCSPInfo, lineNumber) == 16);
static_assert(offsetof(LegacyCSPInfo, columnNumber) == 20);
static_assert(sizeof(LegacySecurityPolicyViolationEventInit) == 80);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, bubbles) == 0);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, cancelable) == 1);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, composed) == 2);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, documentURI) == 8);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, referrer) == 16);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, blockedURI) == 24);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, violatedDirective) == 32);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, effectiveDirective) == 40);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, originalPolicy) == 48);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, sourceFile) == 56);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, statusCode) == 64);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, lineNumber) == 68);
static_assert(offsetof(LegacySecurityPolicyViolationEventInit, columnNumber) == 72);
static_assert(sizeof(LegacyFormDataElement) == 48);
static_assert(offsetof(LegacyFormDataElement, buffer) == 0);
static_assert(offsetof(LegacyFormDataElement, capacity) == 8);
static_assert(offsetof(LegacyFormDataElement, size) == 12);
static_assert(offsetof(LegacyFormDataElement, index) == 40);
static_assert(sizeof(LegacyFormData) == 80);
static_assert(offsetof(LegacyFormData, refCount) == 0);
static_assert(offsetof(LegacyFormData, elements) == 8);
static_assert(offsetof(LegacyFormData, elementsCapacity) == 16);
static_assert(offsetof(LegacyFormData, elementsSize) == 20);
static_assert(offsetof(LegacyFormData, identifier) == 24);
static_assert(offsetof(LegacyFormData, alwaysStream) == 32);
static_assert(offsetof(LegacyFormData, boundary) == 40);
static_assert(offsetof(LegacyFormData, boundaryCapacity) == 48);
static_assert(offsetof(LegacyFormData, boundarySize) == 52);
static_assert(offsetof(LegacyFormData, containsPasswordData) == 56);
static_assert(offsetof(LegacyFormData, hasLengthInBytes) == 64);
static_assert(offsetof(LegacyFormData, lengthInBytes) == 72);
static_assert(sizeof(LegacySecurityOrigin) == 64);
static_assert(offsetof(LegacySecurityOrigin, refCount) == 0);
static_assert(offsetof(LegacySecurityOrigin, protocol) == 8);
static_assert(offsetof(LegacySecurityOrigin, host) == 16);
static_assert(offsetof(LegacySecurityOrigin, hasPort) == 24);
static_assert(offsetof(LegacySecurityOrigin, port) == 26);
static_assert(offsetof(LegacySecurityOrigin, domain) == 32);
static_assert(offsetof(LegacySecurityOrigin, filePath) == 40);
static_assert(offsetof(LegacySecurityOrigin, isUnique) == 48);
static_assert(offsetof(LegacySecurityOrigin, universalAccess) == 49);
static_assert(offsetof(LegacySecurityOrigin, domainWasSetInDOM) == 50);
static_assert(offsetof(LegacySecurityOrigin, canLoadLocalResources) == 51);
static_assert(offsetof(LegacySecurityOrigin, storageBlockingPolicy) == 52);
static_assert(offsetof(LegacySecurityOrigin, enforcesFilePathSeparation) == 56);
static_assert(offsetof(LegacySecurityOrigin, isLocal) == 59);
static_assert(sizeof(LegacySecurityOriginVector) == 16);

constexpr unsigned kURLValidBit = 1u << 0;
constexpr unsigned kURLProtocolIsInHTTPFamilyBit = 1u << 1;
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
