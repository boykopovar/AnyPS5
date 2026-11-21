#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>

namespace {

using NKWebKit::WebCoreAbi::OptionalIdentifier;
using NKWebKit::WebCoreAbi::WidgetRefPtr;
using NKWebKit::WtfAbi::String;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "empty loader legacy callback check failed at line %u\n", location.line());
        std::abort();
    }
}

void TestMimeType() {
    int client = 0;
    String scheme(NKWebKit::WtfAbi::EmptyStringImpl());
    for (int i = 0; i < 3; ++i) {
        String result = _ZNK7WebCore22EmptyFrameLoaderClient29generatedMIMETypeForURLSchemeERKN3WTF6StringE(&client, &scheme);
        Require(result.impl != nullptr);
        Require(result.impl->length == 0);
        Require((result.impl->hashAndFlags & 4) != 0);
        String empty(NKWebKit::WtfAbi::EmptyStringImpl());
        Require(result.impl == empty.impl);
    }
    String withNull = _ZNK7WebCore22EmptyFrameLoaderClient29generatedMIMETypeForURLSchemeERKN3WTF6StringE(nullptr, nullptr);
    Require(withNull.impl && withNull.impl->length == 0);
}

void TestNoOps() {
    int client = 0;
    const char data[] = "abc";
    _ZN7WebCore22EmptyFrameLoaderClient13committedLoadEPNS_14DocumentLoaderEPKci(&client, &client, data, 3);
    _ZN7WebCore22EmptyFrameLoaderClient13committedLoadEPNS_14DocumentLoaderEPKci(nullptr, nullptr, nullptr, 0);
    _ZN7WebCore22EmptyFrameLoaderClient21dispatchDidCommitLoadEN3WTF8OptionalINS_18HasInsecureContentEEENS2_INS_13UsedLegacyTLSEEE(&client);
    Require(client == 0);
    Require(std::strcmp(data, "abc") == 0);
}

void TestNullResults() {
    int client = 0;
    int action = 0;
    Require(_ZN7WebCore22EmptyFrameLoaderClient18dispatchCreatePageERKNS_16NavigationActionE(&client, &action) == nullptr);
    Require(_ZN7WebCore22EmptyFrameLoaderClient18dispatchCreatePageERKNS_16NavigationActionE(nullptr, nullptr) == nullptr);
    WidgetRefPtr widget = _ZN7WebCore22EmptyFrameLoaderClient22createJavaAppletWidgetERKNS_7IntSizeERNS_17HTMLAppletElementERKN3WTF3URLERKNS6_6VectorINS6_6StringELm0ENS6_15CrashOnOverflowELm16ENS6_10FastMallocEEESG_(&client, &action, &action, &action, &action, &action);
    Require(widget.ptr == nullptr);
}

void TestIdentifiers() {
    int client = 0;
    OptionalIdentifier page;
    std::memset(&page, 0xa5, sizeof(page));
    page = _ZNK7WebCore22EmptyFrameLoaderClient6pageIDEv(&client);
    Require(!page.engaged);
    OptionalIdentifier frame;
    std::memset(&frame, 0xa5, sizeof(frame));
    frame = _ZNK7WebCore22EmptyFrameLoaderClient7frameIDEv(&client);
    Require(!frame.engaged);
    Require(!_ZNK7WebCore22EmptyFrameLoaderClient6pageIDEv(nullptr).engaged);
    Require(!_ZNK7WebCore22EmptyFrameLoaderClient7frameIDEv(nullptr).engaged);
}

}

int main() {
    TestMimeType();
    TestNoOps();
    TestNullResults();
    TestIdentifiers();
    return 0;
}
