#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

using namespace Jsc::WtfAbi;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF URL check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

StringImpl Make8(const char* text) {
    return {2, static_cast<unsigned>(std::strlen(text)), text, StringImplFlag8Bit};
}

StringImpl Make16(const std::u16string& text) {
    return {2, static_cast<unsigned>(text.size()), text.data(), 0};
}

URL MakeURL(const StringImpl* string, bool valid, unsigned schemeEnd, unsigned hostEnd, unsigned portLength, unsigned pathEnd, unsigned queryEnd) {
    URL url{};
    url.string = string;
    url.bits = (valid ? URLValidBit : 0) | (portLength << URLPortLengthShift) | (schemeEnd << URLSchemeEndShift);
    url.hostEnd = hostEnd;
    url.pathEnd = pathEnd;
    url.queryEnd = queryEnd;
    return url;
}

URL PortURL(const StringImpl* string) {
    const unsigned length = string->length;
    return MakeURL(string, true, 4, 6, length - 6, length, length);
}

bool Equals(StringView view, const char* text) {
    return view.is8Bit && view.length == std::strlen(text) && std::memcmp(view.characters, text, view.length) == 0;
}

bool IsPort(OptionalUInt16 port, std::uint16_t value) {
    return port.hasValue && port.value == value;
}

OptionalUInt16 DefaultPort(const char* scheme) {
    return _ZN3WTF22defaultPortForProtocolENS_10StringViewE({scheme, static_cast<unsigned>(std::strlen(scheme)), true});
}

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

void TestPortAndQuery() {
    const std::string text = "http://example.com:8080/path/a?x=1#frag";
    StringImpl string = Make8(text.c_str());
    const unsigned hostEnd = static_cast<unsigned>(text.find(':', 5));
    const unsigned pathEnd = static_cast<unsigned>(text.find('?'));
    const unsigned queryEnd = static_cast<unsigned>(text.find('#'));
    URL url = MakeURL(&string, true, 4, hostEnd, 5, pathEnd, queryEnd);
    Require(IsPort(_ZNK3WTF3URL4portEv(&url), 8080));
    Require(Equals(_ZNK3WTF3URL5queryEv(&url), "x=1"));
    Require(Equals(_ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv(&url), "http://example.com:8080/path/a"));
    url.bits &= ~(URLPortLengthMask << URLPortLengthShift);
    Require(!_ZNK3WTF3URL4portEv(&url).hasValue);
    url.queryEnd = pathEnd;
    StringView query = _ZNK3WTF3URL5queryEv(&url);
    Require(!query.characters && !query.length && query.is8Bit);
    url.queryEnd = pathEnd + 1;
    query = _ZNK3WTF3URL5queryEv(&url);
    Require(query.characters && !query.length);
    url.bits &= ~URLValidBit;
    Require(Equals(_ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv(&url), text.c_str()));
    URL nullURL = MakeURL(nullptr, false, 0, 0, 0, 0, 0);
    StringView whole = _ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv(&nullURL);
    Require(!whole.characters && !whole.length);
    nullURL.bits |= URLValidBit;
    whole = _ZNK3WTF3URL38stringWithoutQueryOrFragmentIdentifierEv(&nullURL);
    Require(whole.characters && !whole.length && whole.is8Bit);
    nullURL.bits |= 3u << URLPortLengthShift;
    Require(!_ZNK3WTF3URL4portEv(&nullURL).hasValue);
    Require(ThrowsInvalidArgument([] { _ZNK3WTF3URL4portEv(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZNK3WTF3URL5queryEv(nullptr); }));
}

void TestPortParsing() {
    const char* cases[] = {"http:x:65535", "http:x:65536", "http:x:+80", "http:x:-1", "http:x:8a", "http:x: 8\t", "http:x:", "http:x:00042"};
    StringImpl strings[8];
    URL urls[8];
    for (int index = 0; index < 8; ++index) {
        strings[index] = Make8(cases[index]);
        urls[index] = PortURL(&strings[index]);
    }
    Require(IsPort(_ZNK3WTF3URL4portEv(&urls[0]), 65535));
    Require(!_ZNK3WTF3URL4portEv(&urls[1]).hasValue);
    Require(IsPort(_ZNK3WTF3URL4portEv(&urls[2]), 80));
    Require(!_ZNK3WTF3URL4portEv(&urls[3]).hasValue);
    Require(!_ZNK3WTF3URL4portEv(&urls[4]).hasValue);
    Require(IsPort(_ZNK3WTF3URL4portEv(&urls[5]), 8));
    Require(!_ZNK3WTF3URL4portEv(&urls[6]).hasValue);
    Require(IsPort(_ZNK3WTF3URL4portEv(&urls[7]), 42));
    const std::u16string wide = u"http:x:\u30001\u3000";
    StringImpl wideString = Make16(wide);
    URL wideURL = PortURL(&wideString);
    Require(IsPort(_ZNK3WTF3URL4portEv(&wideURL), 1));
    const std::u16string latin = u"http:x:\u00A01";
    StringImpl latinString = Make16(latin);
    URL latinURL = PortURL(&latinString);
    Require(!_ZNK3WTF3URL4portEv(&latinURL).hasValue);
    const std::u16string fullWidth = u"http:x:\uFF11";
    StringImpl fullWidthString = Make16(fullWidth);
    URL fullWidthURL = PortURL(&fullWidthString);
    Require(!_ZNK3WTF3URL4portEv(&fullWidthURL).hasValue);
}

void TestProtocols() {
    StringImpl file = Make8("file:///tmp/a");
    StringImpl upperFile = Make8("FiLe:///tmp/a");
    StringImpl files = Make8("files:///tmp/a");
    StringImpl about = Make8("about:blank");
    StringImpl abou = Make8("abou:blank");
    URL fileURL = MakeURL(&file, true, 4, 7, 0, 13, 13);
    URL upperFileURL = MakeURL(&upperFile, true, 4, 7, 0, 13, 13);
    URL filesURL = MakeURL(&files, true, 5, 8, 0, 14, 14);
    URL aboutURL = MakeURL(&about, true, 5, 6, 0, 11, 11);
    URL abouURL = MakeURL(&abou, true, 4, 5, 0, 10, 10);
    Require(_ZNK3WTF3URL11isLocalFileEv(&fileURL));
    Require(_ZNK3WTF3URL11isLocalFileEv(&upperFileURL));
    Require(!_ZNK3WTF3URL11isLocalFileEv(&filesURL));
    Require(!_ZNK3WTF3URL11isLocalFileEv(&aboutURL));
    Require(_ZNK3WTF3URL15protocolIsAboutEv(&aboutURL));
    Require(!_ZNK3WTF3URL15protocolIsAboutEv(&abouURL));
    Require(!_ZNK3WTF3URL15protocolIsAboutEv(&fileURL));
    fileURL.bits &= ~URLValidBit;
    Require(!_ZNK3WTF3URL11isLocalFileEv(&fileURL));
    const std::u16string wide = u"file\u0100";
    StringImpl wideString = Make16(wide);
    URL wideURL = MakeURL(&wideString, true, 4, 4, 0, 5, 5);
    Require(_ZNK3WTF3URL11isLocalFileEv(&wideURL));
    URL shortURL = MakeURL(&abou, true, 20, 0, 0, 0, 0);
    Require(!_ZNK3WTF3URL15protocolIsAboutEv(&shortURL));
    Require(ThrowsInvalidArgument([] { _ZNK3WTF3URL11isLocalFileEv(nullptr); }));
}

void TestDefaultPorts() {
    Require(IsPort(DefaultPort("http"), 80));
    Require(IsPort(DefaultPort("https"), 443));
    Require(IsPort(DefaultPort("ws"), 80));
    Require(IsPort(DefaultPort("wss"), 443));
    Require(IsPort(DefaultPort("ftp"), 21));
    Require(!DefaultPort("HTTP").hasValue);
    Require(!DefaultPort("file").hasValue);
    Require(!DefaultPort("").hasValue);
    Require(!DefaultPort("wx").hasValue);
    Require(!DefaultPort("wsx").hasValue);
    Require(!DefaultPort("httpx").hasValue);
    Require(!DefaultPort("ftps").hasValue);
    Require(IsPort(DefaultPort("w"), 0));
    Require(IsPort(DefaultPort("webcal"), 0));
    const std::u16string wide = u"https";
    Require(IsPort(_ZN3WTF22defaultPortForProtocolENS_10StringViewE({wide.data(), 5, false}), 443));
    Require(!_ZN3WTF22defaultPortForProtocolENS_10StringViewE({nullptr, 0, true}).hasValue);
    Require(ThrowsInvalidArgument([] { _ZN3WTF22defaultPortForProtocolENS_10StringViewE({nullptr, 2, true}); }));
}

void TestTime() {
    const TimeWithDynamicClockType early{1.5, ClockType::Monotonic};
    const TimeWithDynamicClockType late{2.5, ClockType::Monotonic};
    const TimeWithDynamicClockType wall{2.5, ClockType::Wall};
    const TimeWithDynamicClockType notANumber{std::numeric_limits<double>::quiet_NaN(), ClockType::Monotonic};
    Require(_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&early, &late));
    Require(!_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&late, &early));
    Require(!_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&late, &late));
    Require(!_ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&early, &notANumber));
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF24TimeWithDynamicClockTypeltERKS0_(&early, &wall); }));
    const Seconds offset{0.25};
    const TimeWithDynamicClockType sum = _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(&offset, &wall);
    Require(sum.value == 2.75 && sum.type == ClockType::Wall);
    const TimeWithDynamicClockType approximate{-1.0, ClockType::Approximate};
    const TimeWithDynamicClockType moved = _ZNK3WTF7SecondsplERKNS_24TimeWithDynamicClockTypeE(&offset, &approximate);
    Require(moved.value == -0.75 && moved.type == ClockType::Approximate);
}

void TestStringBuilderASCII() {
    StringBuilder empty{};
    Require(_ZNK3WTF13StringBuilder10isAllASCIIEv(&empty));
    StringImpl ascii = Make8("abc");
    StringImpl latin = Make8("ab\x80");
    StringBuilder fromString{&ascii, &latin, nullptr, false, false, 3, true};
    Require(_ZNK3WTF13StringBuilder10isAllASCIIEv(&fromString));
    StringBuilder fromBuffer{nullptr, &latin, latin.data, false, false, 3, true};
    Require(!_ZNK3WTF13StringBuilder10isAllASCIIEv(&fromBuffer));
    fromBuffer.length = 2;
    Require(_ZNK3WTF13StringBuilder10isAllASCIIEv(&fromBuffer));
    const std::u16string wideText = u"a\u0100";
    StringImpl wide = Make16(wideText);
    StringBuilder wideBuilder{nullptr, &wide, wide.data, false, false, 2, false};
    Require(!_ZNK3WTF13StringBuilder10isAllASCIIEv(&wideBuilder));
    const std::u16string narrowText = u"az";
    StringImpl narrow = Make16(narrowText);
    wideBuilder.buffer = &narrow;
    Require(_ZNK3WTF13StringBuilder10isAllASCIIEv(&wideBuilder));
    StringBuilder overflowed{&ascii, nullptr, nullptr, true, false, 3, true};
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF13StringBuilder10isAllASCIIEv(&overflowed); }));
    StringBuilder missing{nullptr, nullptr, nullptr, false, false, 1, true};
    Require(ThrowsInvalidArgument([&] { _ZNK3WTF13StringBuilder10isAllASCIIEv(&missing); }));
    Require(ThrowsInvalidArgument([] { _ZNK3WTF13StringBuilder10isAllASCIIEv(nullptr); }));
}

}

int main() {
    TestPortAndQuery();
    TestPortParsing();
    TestProtocols();
    TestDefaultPorts();
    TestTime();
    TestStringBuilderASCII();
    return 0;
}
