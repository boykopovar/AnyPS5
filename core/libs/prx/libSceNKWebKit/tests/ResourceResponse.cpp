#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using NKWebKit::WebCoreAbi::ResourceResponseBase;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceResponseBase check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

static StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static void TestFields() {
    auto urlText = Make("http://a/");
    auto mime = Make("text/html");
    auto encoding = Make("utf-8");
    auto status = Make("OK");
    auto version = Make("HTTP/1.1");
    ResourceResponseBase response{};
    response.url.impl = &urlText;
    response.url.bits = 1 | (4 << 6);
    response.mimeType.impl = &mime;
    response.expectedContentLength = 1234567890123LL;
    response.textEncodingName.impl = &encoding;
    response.httpStatusText.impl = &status;
    response.httpVersion.impl = &version;
    response.httpHeaderFields.storage[3] = 9;

    Require(&_ZNK7WebCore20ResourceResponseBase3urlEv(&response) == &response.url);
    Require(&_ZNK7WebCore20ResourceResponseBase8mimeTypeEv(&response) == &response.mimeType);
    Require(_ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(&response) == 1234567890123LL);
    Require(&_ZNK7WebCore20ResourceResponseBase16textEncodingNameEv(&response) == &response.textEncodingName);
    Require(&_ZNK7WebCore20ResourceResponseBase14httpStatusTextEv(&response) == &response.httpStatusText);
    Require(&_ZNK7WebCore20ResourceResponseBase11httpVersionEv(&response) == &response.httpVersion);
    Require(&_ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(&response) == &response.httpHeaderFields);
    Require(_ZNK7WebCore20ResourceResponseBase16httpHeaderFieldsEv(&response).storage[3] == 9);
    Require(urlText.refCount == 2 && mime.refCount == 2);
    response.expectedContentLength = -1;
    Require(_ZNK7WebCore20ResourceResponseBase21expectedContentLengthEv(&response) == -1);
}

static void TestHTTPFamily() {
    auto http = Make("http://a/");
    auto https = Make("HTTPS://a/");
    auto file = Make("file:///a");
    auto ftp = Make("ftp://a/");
    ResourceResponseBase response{};
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &http;
    response.url.bits = 1 | (4 << 6);
    Require(_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &https;
    Require(_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &file;
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &ftp;
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
    response.url.impl = &http;
    response.url.bits = 0;
    Require(!_ZNK7WebCore20ResourceResponseBase14isInHTTPFamilyEv(&response));
}

static void TestNull() {
    bool thrown = false;
    try {
        _ZNK7WebCore20ResourceResponseBase3urlEv(nullptr);
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    Require(thrown);
}

int main() {
    TestFields();
    TestHTTPFamily();
    TestNull();
    return 0;
}
