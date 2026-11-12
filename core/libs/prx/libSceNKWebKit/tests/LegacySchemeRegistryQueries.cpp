#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "LegacySchemeRegistry query check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

struct Scheme {
    StringImpl impl;
    String string;
    explicit Scheme(const char* text)
        : impl{4, static_cast<unsigned>(std::strlen(text)), text, 4}, string(&impl) {}
    ~Scheme() { Require(impl.refCount == 4); }
};

static bool IsLocal(const char* text) {
    Scheme scheme(text);
    return _ZN7WebCore20LegacySchemeRegistry27shouldTreatURLSchemeAsLocalERKN3WTF6StringE(scheme.string);
}

static bool IsEmptyDocument(const char* text) {
    Scheme scheme(text);
    return _ZN7WebCore20LegacySchemeRegistry34shouldLoadURLSchemeAsEmptyDocumentERKN3WTF6StringE(scheme.string);
}

static void TestBuiltins() {
    Require(IsLocal("file"));
    Require(IsLocal("FILE"));
    Require(IsLocal("FiLe"));
    Require(!IsLocal("files"));
    Require(!IsLocal("fil"));
    Require(!IsLocal("http"));
    Require(!IsLocal("about"));
    Require(!IsLocal(""));

    Require(IsEmptyDocument("about"));
    Require(IsEmptyDocument("ABOUT"));
    Require(!IsEmptyDocument("about:"));
    Require(!IsEmptyDocument("abou"));
    Require(!IsEmptyDocument("file"));
    Require(!IsEmptyDocument("https"));
    Require(!IsEmptyDocument(""));

    String null;
    Require(!_ZN7WebCore20LegacySchemeRegistry27shouldTreatURLSchemeAsLocalERKN3WTF6StringE(null));
    Require(!_ZN7WebCore20LegacySchemeRegistry34shouldLoadURLSchemeAsEmptyDocumentERKN3WTF6StringE(null));
}

static void TestRegistered() {
    Require(!IsLocal("x-local-test"));
    Require(!IsEmptyDocument("x-local-test"));
    Require(!IsLocal("x-empty-test"));
    Require(!IsEmptyDocument("x-empty-test"));

    {
        Scheme local("X-Local-Test");
        _ZN7WebCore20LegacySchemeRegistry24registerURLSchemeAsLocalERKN3WTF6StringE(local.string);
    }
    Require(IsLocal("x-local-test"));
    Require(IsLocal("X-LOCAL-TEST"));
    Require(!IsEmptyDocument("x-local-test"));
    Require(!IsLocal("x-empty-test"));

    {
        Scheme empty("x-empty-test");
        _ZN7WebCore20LegacySchemeRegistry32registerURLSchemeAsEmptyDocumentERKN3WTF6StringE(empty.string);
    }
    Require(IsEmptyDocument("x-empty-test"));
    Require(IsEmptyDocument("X-Empty-Test"));
    Require(!IsLocal("x-empty-test"));
    Require(IsLocal("x-local-test"));

    {
        Scheme secure("x-secure-test");
        _ZN7WebCore20LegacySchemeRegistry25registerURLSchemeAsSecureERKN3WTF6StringE(secure.string);
        Scheme cors("x-cors-test");
        _ZN7WebCore20LegacySchemeRegistry30registerURLSchemeAsCORSEnabledERKN3WTF6StringE(cors.string);
    }
    Require(!IsLocal("x-secure-test") && !IsEmptyDocument("x-secure-test"));
    Require(!IsLocal("x-cors-test") && !IsEmptyDocument("x-cors-test"));
    Require(!IsLocal("https") && !IsEmptyDocument("https"));
    Require(IsLocal("file") && IsEmptyDocument("about"));
}

int main() {
    TestBuiltins();
    TestRegistered();
    return 0;
}
