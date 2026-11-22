#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

namespace NKWebKit::Csp {

enum class HeaderType : bool { Report, Enforce };

constexpr unsigned kSha256 = 1 << 0;
constexpr unsigned kSha384 = 1 << 1;
constexpr unsigned kSha512 = 1 << 2;

constexpr int kSandboxNone = 0;
constexpr int kSandboxNavigation = 1;
constexpr int kSandboxPlugins = 1 << 1;
constexpr int kSandboxOrigin = 1 << 2;
constexpr int kSandboxForms = 1 << 3;
constexpr int kSandboxScripts = 1 << 4;
constexpr int kSandboxTopNavigation = 1 << 5;
constexpr int kSandboxPopups = 1 << 6;
constexpr int kSandboxAutomaticFeatures = 1 << 7;
constexpr int kSandboxPointerLock = 1 << 8;
constexpr int kSandboxPropagatesToAuxiliaryBrowsingContexts = 1 << 9;
constexpr int kSandboxTopNavigationByUserActivation = 1 << 10;
constexpr int kSandboxDocumentDomain = 1 << 11;
constexpr int kSandboxModals = 1 << 12;
constexpr int kSandboxStorageAccessByUserActivation = 1 << 13;
constexpr int kSandboxAll = -1;

struct PolicyHost {
    virtual void LogToConsole(const std::u16string& message) = 0;
    virtual void EnforceSandboxFlags(int flags) = 0;
    virtual void AddHashAlgorithmsForInlineScripts(unsigned algorithms) = 0;
    virtual void AddHashAlgorithmsForInlineStylesheets(unsigned algorithms) = 0;
    virtual void SetUpgradeInsecureRequests() = 0;

protected:
    ~PolicyHost() = default;
};

struct Source {
    std::u16string scheme;
    std::u16string host;
    std::u16string path;
    std::optional<std::uint16_t> port;
    bool hostHasWildcard = false;
    bool portHasWildcard = false;
};

struct Digest {
    unsigned algorithm = 0;
    std::vector<std::uint8_t> value;

    bool operator<(const Digest& other) const {
        return algorithm != other.algorithm ? algorithm < other.algorithm : value < other.value;
    }
};

struct SourceList {
    std::vector<Source> list;
    std::set<std::u16string> nonces;
    std::set<Digest> hashes;
    unsigned hashAlgorithmsUsed = 0;
    std::u16string directiveName;
    bool allowSelf = false;
    bool allowStar = false;
    bool allowInline = false;
    bool allowEval = false;
    bool isNone = false;
};

struct SourceListDirective {
    std::u16string name;
    std::u16string text;
    SourceList sourceList;
};

struct MediaListDirective {
    std::u16string name;
    std::u16string text;
    std::set<std::u16string> pluginTypes;
};

struct DirectiveList {
    std::u16string header;
    HeaderType headerType = HeaderType::Enforce;
    bool reportOnly = false;
    bool haveSandboxPolicy = false;
    bool upgradeInsecureRequests = false;
    bool hasBlockAllMixedContentDirective = false;
    std::unique_ptr<MediaListDirective> pluginTypes;
    std::unique_ptr<SourceListDirective> baseURI;
    std::unique_ptr<SourceListDirective> connectSrc;
    std::unique_ptr<SourceListDirective> childSrc;
    std::unique_ptr<SourceListDirective> defaultSrc;
    std::unique_ptr<SourceListDirective> fontSrc;
    std::unique_ptr<SourceListDirective> formAction;
    std::unique_ptr<SourceListDirective> frameAncestors;
    std::unique_ptr<SourceListDirective> frameSrc;
    std::unique_ptr<SourceListDirective> imgSrc;
    std::unique_ptr<SourceListDirective> mediaSrc;
    std::unique_ptr<SourceListDirective> objectSrc;
    std::unique_ptr<SourceListDirective> scriptSrc;
    std::unique_ptr<SourceListDirective> styleSrc;
    std::vector<std::u16string> reportURIs;
};

std::unique_ptr<DirectiveList> ParseDirectiveList(PolicyHost& host, const std::u16string& header, HeaderType type);

}  // namespace NKWebKit::Csp
