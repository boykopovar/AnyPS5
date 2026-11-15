#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

using NKWebKit::HttpAbi::HTTPHeaderMap;
using NKWebKit::HttpAbi::HTTPHeaderName;
using NKWebKit::WebCoreAbi::ResourceRequestBase;
using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "ResourceRequestBase header field check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template<class TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

static StringImpl Make(const char* text) {
    return StringImpl{2, static_cast<unsigned>(std::strlen(text)), text, 4};
}

static std::string Text(const String& value) {
    if (!value.impl) return "<null>";
    Require((value.impl->hashAndFlags & 4) != 0);
    return std::string(static_cast<const char*>(value.impl->data), value.impl->length);
}

static HTTPHeaderMap& Headers(ResourceRequestBase& request) {
    return *reinterpret_cast<HTTPHeaderMap*>(&request.httpHeaderFields);
}

static ResourceRequestBase MakeRequest() {
    ResourceRequestBase request = ResourceRequestBase();
    request.resourceRequestUpdated = true;
    request.platformRequestUpdated = true;
    return request;
}

static void Clear(ResourceRequestBase& request) {
    auto& map = Headers(request);
    while (map.common.size) _ZN7WebCore13HTTPHeaderMap6removeENS_14HTTPHeaderNameE(&map, map.common.buffer[0].key);
    while (map.uncommon.size) {
        String key(map.uncommon.buffer[0].key);
        _ZN7WebCore13HTTPHeaderMap6removeERKN3WTF6StringE(&map, key);
    }
    _ZN3WTF8fastFreeEPv(map.common.buffer);
    _ZN3WTF8fastFreeEPv(map.uncommon.buffer);
    map = HTTPHeaderMap{};
}

static void TestNamedSetAndGet() {
    auto request = MakeRequest();
    auto html = Make("text/html");
    auto json = Make("application/json");
    String htmlText(&html);
    String jsonText(&json);
    html.refCount += 2;
    json.refCount += 2;

    Require(!_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::ContentType));
    Require(_ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::ContentType).impl == nullptr);
    Require(_ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(&request).impl == nullptr);
    Require(request.platformRequestUpdated);

    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::ContentType, &htmlText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).common.size == 1 && Headers(request).common.capacity >= 6);
    Require(Headers(request).common.buffer[0].key == HTTPHeaderName::ContentType);
    Require(Headers(request).common.buffer[0].value.impl == &html);
    Require(html.refCount == 6);
    Require(_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::ContentType));
    Require(!_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Accept));
    {
        String value = _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::ContentType);
        Require(value.impl == &html && html.refCount == 8);
        String viaAlias = _ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(&request);
        Require(viaAlias.impl == &html && html.refCount == 10);
    }
    Require(html.refCount == 6);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase18setHTTPContentTypeERKN3WTF6StringE(&request, &jsonText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).common.size == 1);
    Require(Headers(request).common.buffer[0].value.impl == &json);
    Require(html.refCount == 4 && json.refCount == 6);

    String null;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::ContentType, &null);
    Require(Headers(request).common.size == 1 && Headers(request).common.buffer[0].value.impl == nullptr);
    Require(_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::ContentType));
    Require(json.refCount == 4);

    Clear(request);
}

static void TestAliases() {
    auto request = MakeRequest();
    auto referrer = Make("https://example.test/page");
    auto agent = Make("Agent/1.0");
    auto origin = Make("https://example.test");
    String referrerText(&referrer);
    String agentText(&agent);
    String originText(&origin);
    referrer.refCount += 2;
    agent.refCount += 2;
    origin.refCount += 2;

    _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(&request, &referrerText);
    Require(!request.platformRequestUpdated);
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase16setHTTPUserAgentERKN3WTF6StringE(&request, &agentText);
    Require(!request.platformRequestUpdated);
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Origin, &originText);

    Require(Headers(request).common.size == 3);
    Require(Headers(request).common.buffer[0].key == HTTPHeaderName::Referer);
    Require(Headers(request).common.buffer[1].key == HTTPHeaderName::UserAgent);
    Require(Headers(request).common.buffer[2].key == HTTPHeaderName::Origin);
    Require(_ZNK7WebCore19ResourceRequestBase12httpReferrerEv(&request).impl == &referrer);
    Require(_ZNK7WebCore19ResourceRequestBase13httpUserAgentEv(&request).impl == &agent);
    Require(_ZNK7WebCore19ResourceRequestBase10httpOriginEv(&request).impl == &origin);
    Require(_ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(&request).impl == nullptr);
    Require(_ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Referer).impl == &referrer);
    Require(referrer.refCount == 6 && agent.refCount == 6 && origin.refCount == 6);

    Clear(request);
    Require(referrer.refCount == 4 && agent.refCount == 4 && origin.refCount == 4);
}

static void TestAdd() {
    auto request = MakeRequest();
    auto first = Make("gzip");
    auto second = Make("br");
    String firstText(&first);
    String secondText(&second);
    first.refCount += 2;
    second.refCount += 2;

    _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::AcceptEncoding, &firstText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).common.size == 1 && Headers(request).common.buffer[0].value.impl == &first);
    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::AcceptEncoding, &secondText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).common.size == 1);
    Require(Text(_ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::AcceptEncoding)) == "gzip, br");
    Require(first.refCount == 4 && second.refCount == 4);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase30addHTTPHeaderFieldIfNotPresentENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::AcceptEncoding, &firstText);
    Require(request.platformRequestUpdated);
    Require(Headers(request).common.size == 1);
    Require(Text(_ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::AcceptEncoding)) == "gzip, br");

    _ZN7WebCore19ResourceRequestBase30addHTTPHeaderFieldIfNotPresentENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Accept, &secondText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).common.size == 2);
    Require(Headers(request).common.buffer[1].key == HTTPHeaderName::Accept);
    Require(Headers(request).common.buffer[1].value.impl == &second && second.refCount == 6);

    Clear(request);
    Require(first.refCount == 4 && second.refCount == 4);
}

static void TestStringNames() {
    auto request = MakeRequest();
    auto knownName = Make("content-TYPE");
    auto customName = Make("X-Custom");
    auto customNameOtherCase = Make("x-custom");
    auto one = Make("one");
    auto two = Make("two");
    String known(&knownName);
    String custom(&customName);
    String customOtherCase(&customNameOtherCase);
    String oneText(&one);
    String twoText(&two);
    for (auto* impl : {&knownName, &customName, &customNameOtherCase, &one, &two}) impl->refCount += 2;

    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &known, &oneText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).common.size == 1 && Headers(request).uncommon.size == 0);
    Require(_ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(&request).impl == &one);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &custom, &oneText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).uncommon.size == 1);
    Require(Headers(request).uncommon.buffer[0].key.impl == &customName);
    Require(Headers(request).uncommon.buffer[0].value.impl == &one);

    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &customOtherCase, &twoText);
    Require(Headers(request).uncommon.size == 1);
    Require(Headers(request).uncommon.buffer[0].key.impl == &customName);
    Require(Headers(request).uncommon.buffer[0].value.impl == &two);

    request.platformRequestUpdated = true;
    _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldERKN3WTF6StringES4_(&request, &custom, &oneText);
    Require(!request.platformRequestUpdated);
    Require(Headers(request).uncommon.size == 1);
    Require(Text(Headers(request).uncommon.buffer[0].value) == "two, one");

    _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldERKN3WTF6StringES4_(&request, &known, &twoText);
    Require(Headers(request).common.size == 1);
    Require(Text(_ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(&request)) == "one, two");

    Clear(request);
    Require(knownName.refCount == 4 && customName.refCount == 4 && customNameOtherCase.refCount == 4);
    Require(one.refCount == 4 && two.refCount == 4);
}

static void TestLongReferrer() {
    auto request = MakeRequest();
    std::vector<char> characters(4097, 'a');
    StringImpl limit{4, 4096, characters.data(), 4};
    StringImpl tooLong{4, 4097, characters.data(), 4};
    String limitText(&limit);
    String tooLongText(&tooLong);

    _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(&request, &limitText);
    Require(!request.platformRequestUpdated);
    Require(_ZNK7WebCore19ResourceRequestBase12httpReferrerEv(&request).impl == &limit);

    request.platformRequestUpdated = true;
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(&request, &tooLongText); }));
    Require(request.platformRequestUpdated);
    Require(_ZNK7WebCore19ResourceRequestBase12httpReferrerEv(&request).impl == &limit);
    Require(tooLong.refCount == 4);

    String null;
    _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(&request, &null);
    Require(_ZNK7WebCore19ResourceRequestBase12httpReferrerEv(&request).impl == nullptr);
    Require(_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Referer));
    Require(limit.refCount == 4);
    Clear(request);
}

static void TestRefusals() {
    auto request = MakeRequest();
    auto valueImpl = Make("v");
    auto nameImpl = Make("X-Name");
    String value(&valueImpl);
    String name(&nameImpl);
    valueImpl.refCount += 2;
    nameImpl.refCount += 2;
    _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Accept, &value);
    request.platformRequestUpdated = true;
    request.resourceRequestUpdated = false;

    Require(_ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Accept));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(&request, HTTPHeaderName::Accept); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase12httpReferrerEv(&request); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase15httpContentTypeEv(&request); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase13httpUserAgentEv(&request); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase10httpOriginEv(&request); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Accept, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &name, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Accept, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldERKN3WTF6StringES4_(&request, &name, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase30addHTTPHeaderFieldIfNotPresentENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Age, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(&request, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase16setHTTPUserAgentERKN3WTF6StringE(&request, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPContentTypeERKN3WTF6StringE(&request, &value); }));
    Require(request.platformRequestUpdated);
    Require(Headers(request).common.size == 1 && Headers(request).uncommon.size == 0);
    Require(valueImpl.refCount == 6);

    request.resourceRequestUpdated = true;
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase15httpHeaderFieldENS_14HTTPHeaderNameE(nullptr, HTTPHeaderName::Accept); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase18hasHTTPHeaderFieldENS_14HTTPHeaderNameE(nullptr, HTTPHeaderName::Accept); }));
    Require(Throws([&] { _ZNK7WebCore19ResourceRequestBase12httpReferrerEv(nullptr); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(nullptr, HTTPHeaderName::Accept, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Accept, nullptr); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, nullptr, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldERKN3WTF6StringES4_(&request, &name, nullptr); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18addHTTPHeaderFieldERKN3WTF6StringES4_(&request, nullptr, &value); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase30addHTTPHeaderFieldIfNotPresentENS_14HTTPHeaderNameERKN3WTF6StringE(&request, HTTPHeaderName::Age, nullptr); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase15setHTTPReferrerERKN3WTF6StringE(&request, nullptr); }));
    Require(Throws([&] { _ZN7WebCore19ResourceRequestBase18setHTTPHeaderFieldENS_14HTTPHeaderNameERKN3WTF6StringE(&request, static_cast<HTTPHeaderName>(1000), &value); }));
    Require(request.platformRequestUpdated);
    Require(Headers(request).common.size == 1 && Headers(request).uncommon.size == 0);

    Clear(request);
    Require(valueImpl.refCount == 4 && nameImpl.refCount == 4);
}

int main() {
    TestNamedSetAndGet();
    TestAliases();
    TestAdd();
    TestStringNames();
    TestLongReferrer();
    TestRefusals();
    return 0;
}
