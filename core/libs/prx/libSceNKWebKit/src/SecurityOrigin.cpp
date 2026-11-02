#include "prx/libSceNKWebKit/src/Types.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include <string>

extern "C" {

const void* APS5_VABI WKSecurityOriginCreate(const void* protocolRef, const void* hostRef, int port) {
    const auto* proto = NKWebKit::Cast<NKWebKit::String>(protocolRef, __func__);
    const auto* host = NKWebKit::Cast<NKWebKit::String>(hostRef, __func__);
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SecurityOrigin>(proto->Utf8(), host->Utf8(), port));
}

const void* APS5_VABI WKSecurityOriginCreateFromString(const void* stringRef) {
    const auto* str = NKWebKit::Cast<NKWebKit::String>(stringRef, __func__);
    std::string text = str->Utf8();
    std::string proto;
    std::string host;
    int port = 0;
    auto schemeEnd = text.find("://");
    if (schemeEnd != std::string::npos) {
        proto = text.substr(0, schemeEnd);
        std::string rest = text.substr(schemeEnd + 3);
        auto pathStart = rest.find('/');
        std::string hostAndPort = (pathStart != std::string::npos) ? rest.substr(0, pathStart) : rest;
        auto portColon = hostAndPort.find(':');
        if (portColon != std::string::npos) {
            host = hostAndPort.substr(0, portColon);
            try {
                port = std::stoi(hostAndPort.substr(portColon + 1));
            } catch (...) {
                port = 0;
            }
        } else {
            host = hostAndPort;
        }
    } else {
        proto = text;
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SecurityOrigin>(std::move(proto), std::move(host), port));
}

const void* APS5_VABI WKSecurityOriginCreateFromDatabaseIdentifier(const void* databaseIdentifierRef) {
    const auto* dbId = NKWebKit::Cast<NKWebKit::String>(databaseIdentifierRef, __func__);
    std::string text = dbId->Utf8();
    std::string proto;
    std::string host;
    int port = 0;
    auto firstUnderscore = text.find('_');
    if (firstUnderscore != std::string::npos) {
        proto = text.substr(0, firstUnderscore);
        auto lastUnderscore = text.rfind('_');
        if (lastUnderscore != std::string::npos && lastUnderscore > firstUnderscore) {
            host = text.substr(firstUnderscore + 1, lastUnderscore - firstUnderscore - 1);
            try {
                port = std::stoi(text.substr(lastUnderscore + 1));
            } catch (...) {
                port = 0;
            }
        } else {
            host = text.substr(firstUnderscore + 1);
        }
    } else {
        proto = text;
    }
    return NKWebKit::ToRef(NKWebKit::Make<NKWebKit::SecurityOrigin>(std::move(proto), std::move(host), port));
}

const void* APS5_VABI WKSecurityOriginCopyProtocol(const void* securityOriginRef) {
    const auto* origin = NKWebKit::Cast<NKWebKit::SecurityOrigin>(securityOriginRef, __func__);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(origin->protocol.c_str()));
}

const void* APS5_VABI WKSecurityOriginCopyHost(const void* securityOriginRef) {
    const auto* origin = NKWebKit::Cast<NKWebKit::SecurityOrigin>(securityOriginRef, __func__);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(origin->host.c_str()));
}

int APS5_VABI WKSecurityOriginGetPort(const void* securityOriginRef) {
    const auto* origin = NKWebKit::Cast<NKWebKit::SecurityOrigin>(securityOriginRef, __func__);
    return origin->port;
}

const void* APS5_VABI WKSecurityOriginCopyToString(const void* securityOriginRef) {
    const auto* origin = NKWebKit::Cast<NKWebKit::SecurityOrigin>(securityOriginRef, __func__);
    std::string result = origin->protocol + "://" + origin->host;
    if (origin->port != 0) {
        result += ":" + std::to_string(origin->port);
    }
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(result.c_str()));
}

const void* APS5_VABI WKSecurityOriginCopyDatabaseIdentifier(const void* securityOriginRef) {
    const auto* origin = NKWebKit::Cast<NKWebKit::SecurityOrigin>(securityOriginRef, __func__);
    std::string result = origin->protocol + "_" + origin->host + "_" + std::to_string(origin->port);
    return NKWebKit::ToRef(NKWebKit::String::FromUtf8(result.c_str()));
}

}
