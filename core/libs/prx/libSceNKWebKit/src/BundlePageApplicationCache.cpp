#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cctype>
#include <set>
#include <stdexcept>
#include <string>

namespace {

std::string NormalizeKey(const std::string& input) {
    auto pos = input.find("://");
    if (pos != std::string::npos) {
        std::string protocol = input.substr(0, pos);
        for (char& c : protocol) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (protocol == "file") {
            return "file__0";
        }
        auto rest = input.substr(pos + 3);
        auto slashPos = rest.find_first_of("/?#");
        if (slashPos != std::string::npos) {
            rest = rest.substr(0, slashPos);
        }
        auto colonPos = rest.rfind(':');
        std::uint16_t port = 0;
        std::string host = rest;
        if (colonPos != std::string::npos) {
            host = rest.substr(0, colonPos);
            port = static_cast<std::uint16_t>(std::stoul(rest.substr(colonPos + 1)));
        } else {
            if (protocol == "https") {
                port = 443;
            } else if (protocol == "http") {
                port = 80;
            } else {
                port = 0;
            }
        }
        for (char& c : host) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        return protocol + "_" + host + "_" + std::to_string(port);
    }

    auto firstUnderscore = input.find('_');
    auto lastUnderscore = input.rfind('_');
    if (firstUnderscore != std::string::npos && lastUnderscore != std::string::npos && firstUnderscore != lastUnderscore) {
        auto portStr = input.substr(lastUnderscore + 1);
        bool allDigits = !portStr.empty();
        for (char c : portStr) {
            if (!std::isdigit(static_cast<unsigned char>(c))) {
                allDigits = false;
                break;
            }
        }
        if (allDigits) {
            std::string protocol = input.substr(0, firstUnderscore);
            for (char& c : protocol) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            std::string host = input.substr(firstUnderscore + 1, lastUnderscore - firstUnderscore - 1);
            for (char& c : host) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            return protocol + "_" + host + "_" + portStr;
        }
    }

    return input;
}

std::string OriginKey(const void* originRef, const char* functionName) {
    const auto* object = NKWebKit::ToObject(originRef, functionName);
    if (object->Type() == NKWebKit::ObjectType::SecurityOrigin) {
        const auto* origin = static_cast<const NKWebKit::SecurityOrigin*>(object);
        std::string protocol = origin->protocolNull ? "" : NKWebKit::EncodeUtf8(origin->protocol);
        for (char& c : protocol) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (protocol == "file") {
            return "file__0";
        }
        std::string host = origin->hostNull ? "" : NKWebKit::EncodeUtf8(origin->host);
        for (char& c : host) {
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
        if (protocol.empty() && host.empty()) {
            return "__0";
        }
        std::uint16_t port = 0;
        if (origin->port) {
            port = *origin->port;
        } else if (protocol == "https") {
            port = 443;
        } else if (protocol == "http") {
            port = 80;
        }
        return protocol + "_" + host + "_" + std::to_string(port);
    }
    if (object->Type() == NKWebKit::ObjectType::String) {
        const auto* str = static_cast<const NKWebKit::String*>(object);
        return NormalizeKey(str->Utf8());
    }
    throw std::invalid_argument(std::string(functionName) + ": invalid origin reference");
}

}

extern "C" {

void APS5_VABI WKBundlePageClearApplicationCache(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    page->applicationCacheClearCount++;
    page->appCacheOriginQuotas.clear();
    page->appCacheOriginUsage.clear();
}

void APS5_VABI WKBundlePageClearApplicationCacheForOrigin(const void* pageRef, const void* originRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto key = OriginKey(originRef, __func__);
    page->appCacheOriginUsage.erase(key);
}

std::uint64_t APS5_VABI WKBundlePageGetAppCacheUsageForOrigin(const void* pageRef, const void* originRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto key = OriginKey(originRef, __func__);
    auto it = page->appCacheOriginUsage.find(key);
    return it != page->appCacheOriginUsage.end() ? it->second : 0;
}

void APS5_VABI WKBundlePageSetApplicationCacheOriginQuota(const void* pageRef, const void* originRef, std::uint64_t bytes) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto key = OriginKey(originRef, __func__);
    page->appCacheOriginQuotas[key] = bytes;
}

void APS5_VABI WKBundlePageResetApplicationCacheOriginQuota(const void* pageRef, const void* originRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    auto key = OriginKey(originRef, __func__);
    page->appCacheOriginQuotas.erase(key);
}

const void* APS5_VABI WKBundlePageCopyOriginsWithApplicationCache(const void* pageRef) {
    auto page = NKWebKit::Cast<NKWebKit::BundlePage>(pageRef, __func__);
    std::set<std::string> origins;
    for (const auto& [k, usage] : page->appCacheOriginUsage) {
        if (usage > 0) {
            origins.insert(k);
        }
    }
    auto array = NKWebKit::Make<NKWebKit::Array>();
    for (const auto& origin : origins) {
        array->elements.push_back(NKWebKit::String::FromUtf8(origin.c_str()));
    }
    return NKWebKit::ToRef(array);
}

}
