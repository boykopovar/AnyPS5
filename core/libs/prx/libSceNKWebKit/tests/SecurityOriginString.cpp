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
    const void* urlCustomPort = WKStringCreateWithUTF8CString("https://example.org:8443/test");
    const void* originCustom = WKSecurityOriginCreateFromString(urlCustomPort);
    Require(originCustom != nullptr);
    Require(WKGetTypeID(originCustom) == WKSecurityOriginGetTypeID());
    const void* protoCustom = WKSecurityOriginCopyProtocol(originCustom);
    const void* hostCustom = WKSecurityOriginCopyHost(originCustom);
    const void* textCustom = WKSecurityOriginCopyToString(originCustom);
    Require(WKStringIsEqualToUTF8CString(protoCustom, "https"));
    Require(WKStringIsEqualToUTF8CString(hostCustom, "example.org"));
    Require(WKSecurityOriginGetPort(originCustom) == 8443);
    Require(WKStringIsEqualToUTF8CString(textCustom, "https://example.org:8443"));

    const void* urlHttpDefault = WKStringCreateWithUTF8CString("http://example.org:80/index.html");
    const void* originHttp = WKSecurityOriginCreateFromString(urlHttpDefault);
    Require(originHttp != nullptr);
    const void* protoHttp = WKSecurityOriginCopyProtocol(originHttp);
    const void* hostHttp = WKSecurityOriginCopyHost(originHttp);
    const void* textHttp = WKSecurityOriginCopyToString(originHttp);
    Require(WKStringIsEqualToUTF8CString(protoHttp, "http"));
    Require(WKStringIsEqualToUTF8CString(hostHttp, "example.org"));
    Require(WKSecurityOriginGetPort(originHttp) == 0);
    Require(WKStringIsEqualToUTF8CString(textHttp, "http://example.org"));

    const void* urlHttpsDefault = WKStringCreateWithUTF8CString("https://example.org:443/");
    const void* originHttps = WKSecurityOriginCreateFromString(urlHttpsDefault);
    Require(originHttps != nullptr);
    Require(WKSecurityOriginGetPort(originHttps) == 0);
    const void* textHttps = WKSecurityOriginCopyToString(originHttps);
    Require(WKStringIsEqualToUTF8CString(textHttps, "https://example.org"));

    const void* urlFtpDefault = WKStringCreateWithUTF8CString("ftp://example.org:21/files");
    const void* originFtp = WKSecurityOriginCreateFromString(urlFtpDefault);
    Require(originFtp != nullptr);
    Require(WKSecurityOriginGetPort(originFtp) == 0);
    const void* textFtp = WKSecurityOriginCopyToString(originFtp);
    Require(WKStringIsEqualToUTF8CString(textFtp, "ftp://example.org"));

    const void* urlUppercase = WKStringCreateWithUTF8CString("HTTP://EXAMPLE.COM:8080/FOO");
    const void* originUpper = WKSecurityOriginCreateFromString(urlUppercase);
    Require(originUpper != nullptr);
    const void* protoUpper = WKSecurityOriginCopyProtocol(originUpper);
    const void* hostUpper = WKSecurityOriginCopyHost(originUpper);
    Require(WKStringIsEqualToUTF8CString(protoUpper, "http"));
    Require(WKStringIsEqualToUTF8CString(hostUpper, "example.com"));
    Require(WKSecurityOriginGetPort(originUpper) == 8080);

    const void* urlFile = WKStringCreateWithUTF8CString("file:///path/to/data");
    const void* originFile = WKSecurityOriginCreateFromString(urlFile);
    Require(originFile != nullptr);
    const void* protoFile = WKSecurityOriginCopyProtocol(originFile);
    const void* hostFile = WKSecurityOriginCopyHost(originFile);
    const void* textFile = WKSecurityOriginCopyToString(originFile);
    Require(WKStringIsEqualToUTF8CString(protoFile, "file"));
    Require(WKStringIsEmpty(hostFile));
    Require(WKSecurityOriginGetPort(originFile) == 0);
    Require(WKStringIsEqualToUTF8CString(textFile, "file://"));

    const void* emptyStr = WKStringCreateWithUTF8CString("");
    const void* originEmpty = WKSecurityOriginCreateFromString(emptyStr);
    Require(originEmpty != nullptr);
    const void* protoEmpty = WKSecurityOriginCopyProtocol(originEmpty);
    const void* hostEmpty = WKSecurityOriginCopyHost(originEmpty);
    const void* textEmpty = WKSecurityOriginCopyToString(originEmpty);
    Require(WKStringIsEmpty(protoEmpty));
    Require(WKStringIsEmpty(hostEmpty));
    Require(WKSecurityOriginGetPort(originEmpty) == 0);
    Require(WKStringIsEqualToUTF8CString(textEmpty, "null"));

    const void* urlFileHost = WKStringCreateWithUTF8CString("file://server/share");
    const void* originFileHost = WKSecurityOriginCreateFromString(urlFileHost);
    Require(originFileHost != nullptr);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyProtocol(originFileHost), "file"));
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyHost(originFileHost), "server"));
    Require(WKSecurityOriginGetPort(originFileHost) == 0);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyToString(originFileHost), "file://"));

    const void* urlBlob = WKStringCreateWithUTF8CString("blob:https://example.org/0b1e");
    const void* originBlob = WKSecurityOriginCreateFromString(urlBlob);
    Require(originBlob != nullptr);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyProtocol(originBlob), "https"));
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyHost(originBlob), "example.org"));
    Require(WKSecurityOriginGetPort(originBlob) == 0);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyToString(originBlob), "https://example.org"));

    const void* urlWs = WKStringCreateWithUTF8CString("ws://example.org:80/");
    const void* originWs = WKSecurityOriginCreateFromString(urlWs);
    Require(originWs != nullptr);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyProtocol(originWs), "ws"));
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyHost(originWs), "example.org"));
    Require(WKSecurityOriginGetPort(originWs) == 0);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyToString(originWs), "ws://example.org"));

    const void* urlWss = WKStringCreateWithUTF8CString("wss://example.org:443/");
    const void* originWss = WKSecurityOriginCreateFromString(urlWss);
    Require(originWss != nullptr);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyProtocol(originWss), "wss"));
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyHost(originWss), "example.org"));
    Require(WKSecurityOriginGetPort(originWss) == 0);
    Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyToString(originWss), "wss://example.org"));

    for (const char* opaqueUrl : {"not a url", "data:text/plain,x", "about:blank", "custom://example.org/x", "http://[invalid"}) {
        const void* opaqueWkStr = WKStringCreateWithUTF8CString(opaqueUrl);
        const void* originOpaque = WKSecurityOriginCreateFromString(opaqueWkStr);
        Require(originOpaque != nullptr);
        Require(WKStringIsEmpty(WKSecurityOriginCopyProtocol(originOpaque)));
        Require(WKStringIsEmpty(WKSecurityOriginCopyHost(originOpaque)));
        Require(WKSecurityOriginGetPort(originOpaque) == 0);
        Require(WKStringIsEqualToUTF8CString(WKSecurityOriginCopyToString(originOpaque), "null"));
    }

    const void* nullWkString = NKWebKit::ToRef(NKWebKit::Make<NKWebKit::String>());
    const void* originNull = WKSecurityOriginCreateFromString(nullWkString);
    Require(originNull != nullptr);
    Require(WKStringIsEmpty(WKSecurityOriginCopyProtocol(originNull)));

    RequireThrows([] {
        WKSecurityOriginCreateFromString(nullptr);
    });

    RequireThrows([originCustom] {
        WKSecurityOriginCreateFromString(originCustom);
    });

    return 0;
}
