#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Configuration.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>

static void Check(bool condition, int line) {
    if (!condition) {
        std::fprintf(stderr, "check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(condition) Check((condition), __LINE__)

template<typename Fn>
static void RequireThrows(Fn&& fn) {
    bool threw = false;
    try {
        fn();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

int main() {
    const void* httpProto = WKStringCreateWithUTF8CString("http");
    const void* httpsProto = WKStringCreateWithUTF8CString("https");
    const void* fileProto = WKStringCreateWithUTF8CString("file");
    const void* upperFileProto = WKStringCreateWithUTF8CString("FILE");
    const void* emptyHost = WKStringCreateWithUTF8CString("");
    const void* exampleHost = WKStringCreateWithUTF8CString("example.com");
    const void* encodedHost = WKStringCreateWithUTF8CString("a:b/c");

    const void* originHttp = WKSecurityOriginCreate(httpProto, exampleHost, 80);
    Require(originHttp != nullptr);
    const void* idHttp = WKSecurityOriginCopyDatabaseIdentifier(originHttp);
    Require(idHttp != nullptr);
    Require(WKStringIsEqualToUTF8CString(idHttp, "http_example.com_80"));

    const void* recreatedHttp = WKSecurityOriginCreateFromDatabaseIdentifier(idHttp);
    Require(recreatedHttp != nullptr);
    const void* recProtoHttp = WKSecurityOriginCopyProtocol(recreatedHttp);
    const void* recHostHttp = WKSecurityOriginCopyHost(recreatedHttp);
    Require(WKStringIsEqualToUTF8CString(recProtoHttp, "http"));
    Require(WKStringIsEqualToUTF8CString(recHostHttp, "example.com"));
    Require(WKSecurityOriginGetPort(recreatedHttp) == 80);

    const void* originHttps = WKSecurityOriginCreate(httpsProto, exampleHost, 443);
    Require(originHttps != nullptr);
    const void* idHttps = WKSecurityOriginCopyDatabaseIdentifier(originHttps);
    Require(idHttps != nullptr);
    Require(WKStringIsEqualToUTF8CString(idHttps, "https_example.com_443"));

    const void* recreatedHttps = WKSecurityOriginCreateFromDatabaseIdentifier(idHttps);
    Require(recreatedHttps != nullptr);
    const void* recProtoHttps = WKSecurityOriginCopyProtocol(recreatedHttps);
    const void* recHostHttps = WKSecurityOriginCopyHost(recreatedHttps);
    Require(WKStringIsEqualToUTF8CString(recProtoHttps, "https"));
    Require(WKStringIsEqualToUTF8CString(recHostHttps, "example.com"));
    Require(WKSecurityOriginGetPort(recreatedHttps) == 443);

    const void* originFile = WKSecurityOriginCreate(fileProto, emptyHost, 0);
    Require(originFile != nullptr);
    const void* idFile = WKSecurityOriginCopyDatabaseIdentifier(originFile);
    Require(idFile != nullptr);
    Require(WKStringIsEqualToUTF8CString(idFile, "file__0"));

    const void* recreatedFile = WKSecurityOriginCreateFromDatabaseIdentifier(idFile);
    Require(recreatedFile != nullptr);
    const void* recProtoFile = WKSecurityOriginCopyProtocol(recreatedFile);
    const void* recHostFile = WKSecurityOriginCopyHost(recreatedFile);
    Require(WKStringIsEqualToUTF8CString(recProtoFile, "file"));
    Require(WKStringIsEmpty(recHostFile));
    Require(WKSecurityOriginGetPort(recreatedFile) == 0);

    const void* originUpperFile = WKSecurityOriginCreate(upperFileProto, emptyHost, 0);
    Require(originUpperFile != nullptr);
    const void* idUpperFile = WKSecurityOriginCopyDatabaseIdentifier(originUpperFile);
    Require(idUpperFile != nullptr);
    Require(WKStringIsEqualToUTF8CString(idUpperFile, "file__0"));

    const void* originFileWithHost = WKSecurityOriginCreate(fileProto, exampleHost, 80);
    Require(originFileWithHost != nullptr);
    const void* idFileWithHost = WKSecurityOriginCopyDatabaseIdentifier(originFileWithHost);
    Require(idFileWithHost != nullptr);
    Require(WKStringIsEqualToUTF8CString(idFileWithHost, "file__0"));

    const void* originEncoded = WKSecurityOriginCreate(httpProto, encodedHost, 0);
    Require(originEncoded != nullptr);
    const void* idEncoded = WKSecurityOriginCopyDatabaseIdentifier(originEncoded);
    Require(idEncoded != nullptr);
    Require(WKStringIsEqualToUTF8CString(idEncoded, "http_a%3Ab%2Fc_0"));

    const void* recreatedEncoded = WKSecurityOriginCreateFromDatabaseIdentifier(idEncoded);
    Require(recreatedEncoded != nullptr);
    const void* recProtoEncoded = WKSecurityOriginCopyProtocol(recreatedEncoded);
    const void* recHostEncoded = WKSecurityOriginCopyHost(recreatedEncoded);
    Require(WKStringIsEqualToUTF8CString(recProtoEncoded, "http"));
    Require(WKStringIsEqualToUTF8CString(recHostEncoded, "a%3Ab%2Fc"));
    Require(WKSecurityOriginGetPort(recreatedEncoded) == 0);

    const void* emptyPortId = WKStringCreateWithUTF8CString("https_example.com_");
    const void* emptyPortOrigin = WKSecurityOriginCreateFromDatabaseIdentifier(emptyPortId);
    Require(emptyPortOrigin != nullptr);
    Require(WKSecurityOriginGetPort(emptyPortOrigin) == 0);

    const void* zeroPortId = WKStringCreateWithUTF8CString("http_example.com_0");
    const void* zeroPortOrigin = WKSecurityOriginCreateFromDatabaseIdentifier(zeroPortId);
    Require(zeroPortOrigin != nullptr);
    const void* zeroPortStr = WKSecurityOriginCopyToString(zeroPortOrigin);
    Require(WKStringIsEqualToUTF8CString(zeroPortStr, "http://example.com"));

    for (const char* portCase : {"http_example.com_ 80", "http_example.com_\t80", "http_example.com_+80", "http_example.com_80abc", "http_example.com_\f80"}) {
        const void* caseStr = WKStringCreateWithUTF8CString(portCase);
        const void* caseOrigin = WKSecurityOriginCreateFromDatabaseIdentifier(caseStr);
        Require(caseOrigin != nullptr);
        Require(WKSecurityOriginGetPort(caseOrigin) == 80);
    }

    const char* malformedCases[] = {
        "",
        "https",
        "https_example.com",
        "https_example.com_abc",
        "https_example.com_65536",
        "https_example.com_9999999999999999999999999999999"
    };
    for (const char* malformed : malformedCases) {
        const void* malformedStr = WKStringCreateWithUTF8CString(malformed);
        Require(WKSecurityOriginCreateFromDatabaseIdentifier(malformedStr) == nullptr);
    }

    const void* nullWkString = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    Require(WKSecurityOriginCreateFromDatabaseIdentifier(nullWkString) == nullptr);

    RequireThrows([] {
        WKSecurityOriginCopyDatabaseIdentifier(nullptr);
    });

    RequireThrows([] {
        WKSecurityOriginCreateFromDatabaseIdentifier(nullptr);
    });

    const void* dummyString = WKStringCreateWithUTF8CString("dummy");
    RequireThrows([dummyString] {
        WKSecurityOriginCopyDatabaseIdentifier(dummyString);
    });

    RequireThrows([originHttp] {
        WKSecurityOriginCreateFromDatabaseIdentifier(originHttp);
    });

    return 0;
}
