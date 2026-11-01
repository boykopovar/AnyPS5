#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <utility>

namespace {

void Require(bool condition) {
    if (!condition) {
        std::abort();
    }
}

template <typename F>
void RequireThrows(F&& f) {
    bool caught = false;
    try {
        f();
    } catch (const std::exception&) {
        caught = true;
    }
    Require(caught);
}

using NKWebKit::WtfAbi::String;
using NKWebKit::WtfAbi::StringImpl;
using NKWebKit::WtfAbi::StringView;
using NKWebKit::WtfAbi::CString;
using NKWebKit::WtfAbi::URL;
using NKWebKit::WtfAbi::SplitResult;
using NKWebKit::WtfAbi::SplitResultIterator;
using NKWebKit::WtfAbi::Vector;
using NKWebKit::WtfAbi::Encoder;
using NKWebKit::WtfAbi::Decoder;
using NKWebKit::WtfAbi::AtomString;

std::string ToString(const String& s) {
    if (!s.impl) {
        return "";
    }
    std::string result;
    result.reserve(s.impl->length);
    bool is8 = (s.impl->hashAndFlags & 4) != 0;
    for (unsigned i = 0; i < s.impl->length; ++i) {
        char c = is8 ? static_cast<char>(static_cast<const unsigned char*>(s.impl->data)[i])
                     : static_cast<char>(static_cast<const char16_t*>(s.impl->data)[i]);
        result.push_back(c);
    }
    return result;
}

void TestURL() {
    auto uStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("https://alice:secret@api.example.com:8443/v1/data?key=val#section"), 65);
    URL url{uStr.impl, 1U | 2U | (5U << 3) | (5U << 6), 8, 13, 20, 36, 45, 49, 57};

    Require(_ZNK3WTF3URL10protocolIsEPKc(&url, "https"));
    Require(!_ZNK3WTF3URL10protocolIsEPKc(&url, "http"));
    Require(!_ZNK3WTF3URL10protocolIsEPKc(&url, "ftp"));
    RequireThrows([&] { _ZNK3WTF3URL10protocolIsEPKc(&url, nullptr); });
    Require(!_ZNK3WTF3URL10protocolIsEPKc(&url, ""));
    RequireThrows([] { _ZNK3WTF3URL10protocolIsEPKc(nullptr, "https"); });

    URL invalidUrl{uStr.impl, 0U, 8, 13, 20, 36, 45, 49, 57};
    Require(!_ZNK3WTF3URL10protocolIsEPKc(&invalidUrl, "https"));

    Require(_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"192.168.0.1", 11, true}));
    Require(_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"127.0.0.1", 9, true}));
    Require(_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"0.0.0.0", 7, true}));
    Require(_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"255.255.255.255", 15, true}));
    Require(_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"::1", 3, true}));
    Require(_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"::ffff:192.168.0.1", 18, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"[::1]", 5, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"256.0.0.1", 9, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"192.168.1", 9, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"1.2.3.4.5", 9, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"12345::1", 8, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"example.com", 11, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({"", 0, true}));
    Require(!_ZN3WTF3URL15hostIsIPAddressENS_10StringViewE({nullptr, 0, true}));

    auto hp = _ZNK3WTF3URL11hostAndPortEv(&url);
    Require(ToString(hp) == "api.example.com:8443");
    RequireThrows([] { _ZNK3WTF3URL11hostAndPortEv(nullptr); });

    auto php = _ZNK3WTF3URL19protocolHostAndPortEv(&url);
    Require(ToString(php) == "https://api.example.com:8443");
    RequireThrows([] { _ZNK3WTF3URL19protocolHostAndPortEv(nullptr); });

    auto u = _ZNK3WTF3URL4userEv(&url);
    Require(ToString(u) == "alice");
    RequireThrows([] { _ZNK3WTF3URL4userEv(nullptr); });

    auto p = _ZNK3WTF3URL8passwordEv(&url);
    Require(ToString(p) == "secret");
    RequireThrows([] { _ZNK3WTF3URL8passwordEv(nullptr); });

    Require(_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(&url, {"api.example.com", 15, true}));
    Require(_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(&url, {"example.com", 11, true}));
    Require(_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(&url, {"", 0, true}));
    Require(!_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(&url, {"google.com", 10, true}));
    Require(!_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(&url, {"ple.com", 7, true}));
    Require(!_ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(&url, {"sub.api.example.com", 19, true}));
    RequireThrows([] { _ZNK3WTF3URL16isMatchingDomainENS_10StringViewE(nullptr, {"example.com", 11, true}); });

    auto fStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("file:///etc/hosts"), 17);
    URL fileUrl{fStr.impl, 1U | (4U << 6), 7, 7, 7, 7, 8, 17, 17};
    auto fsPath = _ZNK3WTF3URL14fileSystemPathEv(&fileUrl);
    Require(ToString(fsPath) == "/etc/hosts");

    auto nonFilePath = _ZNK3WTF3URL14fileSystemPathEv(&url);
    Require(ToString(nonFilePath).empty());
    RequireThrows([] { _ZNK3WTF3URL14fileSystemPathEv(nullptr); });

    URL noHostUrl{fStr.impl, 1U | (4U << 6), 7, 7, 7, 7, 7, 17, 17};
    auto noHp = _ZNK3WTF3URL11hostAndPortEv(&noHostUrl);
    Require(ToString(noHp).empty());
    auto noPhp = _ZNK3WTF3URL19protocolHostAndPortEv(&noHostUrl);
    Require(ToString(noPhp) == "file://");
    URL invUrl{nullptr, 0, 0, 0, 0, 0, 0, 0, 0};
    auto invPhp = _ZNK3WTF3URL19protocolHostAndPortEv(&invUrl);
    Require(ToString(invPhp).empty());

    URL noCredUrl{uStr.impl, 1U | 2U | (5U << 3) | (5U << 6), 8, 8, 8, 24, 33, 37, 45};
    auto noUser = _ZNK3WTF3URL4userEv(&noCredUrl);
    Require(ToString(noUser).empty());
    auto noPass = _ZNK3WTF3URL8passwordEv(&noCredUrl);
    Require(ToString(noPass).empty());

    String owned = uStr;
    URL ownedUrl{owned.impl, 1U | 2U | (5U << 3) | (5U << 6), 8, 13, 20, 36, 45, 49, 57};
    owned.impl = nullptr;
    _ZN3WTF3URL32removeQueryAndFragmentIdentifierEv(&ownedUrl);
    Require(ownedUrl.queryEnd == 49);
    String stripped(ownedUrl.impl);
    ownedUrl.impl = nullptr;
    Require(ToString(stripped) == "https://alice:secret@api.example.com:8443/v1/data");
    RequireThrows([] { _ZN3WTF3URL32removeQueryAndFragmentIdentifierEv(nullptr); });
}

void TestStringImplReplace() {
    auto str8 = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("banana"), 6);
    auto rep1 = _ZN3WTF10StringImpl7replaceEDsDs(str8.impl, 'a', 'o');
    Require(ToString(rep1) == "bonono");
    auto noRep1 = _ZN3WTF10StringImpl7replaceEDsDs(str8.impl, 'z', 'x');
    Require(noRep1.impl == str8.impl);
    RequireThrows([] { _ZN3WTF10StringImpl7replaceEDsDs(nullptr, 'a', 'b'); });

    auto rep2 = _ZN3WTF10StringImpl7replaceEDsPKhj(str8.impl, 'a', reinterpret_cast<const unsigned char*>("12"), 2);
    Require(ToString(rep2) == "b12n12n12");
    auto noRep2 = _ZN3WTF10StringImpl7replaceEDsPKhj(str8.impl, 'z', reinterpret_cast<const unsigned char*>("12"), 2);
    Require(noRep2.impl == str8.impl);
    const unsigned char emptyRep[] = "";
    auto repEmpty = _ZN3WTF10StringImpl7replaceEDsPKhj(str8.impl, 'a', emptyRep, 0);
    Require(ToString(repEmpty) == "bnn");
    RequireThrows([] { _ZN3WTF10StringImpl7replaceEDsPKhj(nullptr, 'a', reinterpret_cast<const unsigned char*>("b"), 1); });
    RequireThrows([&] { _ZN3WTF10StringImpl7replaceEDsPKhj(str8.impl, 'a', nullptr, 0); });

    const char16_t wideB[] = u"banana";
    auto str16 = _ZN3WTF10StringImpl6createEPKDsj(wideB, 6);
    auto rep3 = _ZN3WTF10StringImpl7replaceEDsDs(str16.impl, 'n', 'm');
    Require(ToString(rep3) == "bamama");
    auto noRep3 = _ZN3WTF10StringImpl7replaceEDsDs(str16.impl, 'z', 'x');
    Require(noRep3.impl == str16.impl);

    auto rep4 = _ZN3WTF10StringImpl7replaceEDsPKhj(str16.impl, 'b', reinterpret_cast<const unsigned char*>("xy"), 2);
    Require(ToString(rep4) == "xyanana");
}

void TestStringViewSplitResultIterator() {
    const char str[] = "one,two,,four";
    StringView sv{str, 13, true};
    SplitResult res{sv, ',', true};
    SplitResultIterator it{&res, 0, 0, false};
    _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(&it);
    Require(!it.isDone);
    Require(it.position == 0 && it.length == 3);
    _ZN3WTF10StringView11SplitResult8IteratorppEv(&it);
    Require(!it.isDone);
    Require(it.position == 4 && it.length == 3);
    _ZN3WTF10StringView11SplitResult8IteratorppEv(&it);
    Require(!it.isDone);
    Require(it.position == 8 && it.length == 0);
    _ZN3WTF10StringView11SplitResult8IteratorppEv(&it);
    Require(!it.isDone);
    Require(it.position == 9 && it.length == 4);
    _ZN3WTF10StringView11SplitResult8IteratorppEv(&it);
    Require(it.isDone);

    const char single[] = "apple";
    StringView singleView{single, 5, true};
    SplitResult singleRes{singleView, ',', false};
    SplitResultIterator singleIt{&singleRes, 0, 0, false};
    _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(&singleIt);
    Require(!singleIt.isDone);
    Require(singleIt.position == 0 && singleIt.length == 5);
    _ZN3WTF10StringView11SplitResult8IteratorppEv(&singleIt);
    Require(singleIt.isDone);

    RequireThrows([] { _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(nullptr); });
    RequireThrows([] { _ZN3WTF10StringView11SplitResult8IteratorppEv(nullptr); });
    SplitResultIterator brokenIt{nullptr, 0, 0, false};
    RequireThrows([&] { _ZN3WTF10StringView11SplitResult8Iterator17findNextSubstringEv(&brokenIt); });
    RequireThrows([&] { _ZN3WTF10StringView11SplitResult8IteratorppEv(&brokenIt); });
}

void TestPersistence() {
    Encoder enc;
    _ZN3WTF11Persistence7EncoderC1Ev(&enc);
    _ZN3WTF11Persistence7EncoderlsEb(&enc, true);
    _ZN3WTF11Persistence7EncoderlsEb(&enc, false);
    _ZN3WTF11Persistence7EncoderlsEh(&enc, 0x42);
    _ZN3WTF11Persistence7EncoderlsEs(&enc, -1234);
    _ZN3WTF11Persistence7EncoderlsEt(&enc, 5678);
    _ZN3WTF11Persistence7EncoderlsEi(&enc, -100000);
    _ZN3WTF11Persistence7EncoderlsEj(&enc, 200000);
    _ZN3WTF11Persistence7EncoderlsEl(&enc, -9999999999LL);
    _ZN3WTF11Persistence7EncoderlsEm(&enc, 8888888888ULL);
    _ZN3WTF11Persistence7EncoderlsEf(&enc, 1.25f);
    _ZN3WTF11Persistence7EncoderlsEd(&enc, 2.5);

    auto strVal = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("test"), 4);
    _ZN3WTF11Persistence5CoderINS_6StringEE6encodeERNS0_7EncoderERKS2_(enc, strVal);

    char* cstrData = nullptr;
    auto cstrVal = _ZN3WTF7CString16newUninitializedEmRPc(4, cstrData);
    std::memcpy(cstrData, "cstr", 4);
    _ZN3WTF11Persistence5CoderINS_7CStringEE6encodeERNS0_7EncoderERKS2_(enc, cstrVal);

    CString nullCstr{nullptr};
    _ZN3WTF11Persistence5CoderINS_7CStringEE6encodeERNS0_7EncoderERKS2_(enc, nullCstr);

    auto atomInner = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("atom"), 4);
    AtomString atomVal{std::move(atomInner)};
    _ZN3WTF11Persistence5CoderINS_10AtomStringEE6encodeERNS0_7EncoderERKS2_(enc, atomVal);

    NKWebKit::WtfAbi::SHA1 payloadHash = enc.sha1;
    unsigned payloadSize = enc.size;
    _ZN3WTF11Persistence7Encoder14encodeChecksumEv(&enc);
    Require(enc.size == payloadSize + 20);

    Decoder dec{enc.buffer, enc.size, enc.buffer + payloadSize, payloadHash};
    Require(_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(&dec, 20));
    Require(!_ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(&dec, 21));
    Require(_ZN3WTF11Persistence7Decoder14verifyChecksumEv(&dec));
    Require(dec.bufferPosition == enc.buffer + enc.size);

    enc.buffer[enc.size - 1] ^= 0xFF;
    Decoder badDec{enc.buffer, enc.size, enc.buffer + payloadSize, payloadHash};
    Require(!_ZN3WTF11Persistence7Decoder14verifyChecksumEv(&badDec));
    _ZN3WTF11Persistence7EncoderD1Ev(&enc);

    Encoder enc2;
    _ZN3WTF11Persistence7EncoderC2Ev(&enc2);
    _ZN3WTF11Persistence7EncoderD2Ev(&enc2);

    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEb(nullptr, true); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEh(nullptr, 0); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEs(nullptr, 0); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEt(nullptr, 0); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEi(nullptr, 0); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEj(nullptr, 0); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEl(nullptr, 0); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEm(nullptr, 0); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEf(nullptr, 0.0f); });
    RequireThrows([] { _ZN3WTF11Persistence7EncoderlsEd(nullptr, 0.0); });
    RequireThrows([] { _ZN3WTF11Persistence7Encoder14encodeChecksumEv(nullptr); });
    RequireThrows([] { _ZN3WTF11Persistence7Decoder14verifyChecksumEv(nullptr); });
    RequireThrows([] { _ZNK3WTF11Persistence7Decoder28bufferIsLargeEnoughToContainEm(nullptr, 10); });
}

void TestString() {
    auto spStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("foo,bar,,baz"), 12);
    auto vec1 = _ZNK3WTF6String5splitEDs(&spStr, ',');
    Require(vec1.size == 3);
    Require(ToString(vec1.buffer[0]) == "foo");
    Require(ToString(vec1.buffer[1]) == "bar");
    Require(ToString(vec1.buffer[2]) == "baz");

    auto emptySpStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(",,,,"), 4);
    auto vecEmpty = _ZNK3WTF6String5splitEDs(&emptySpStr, ',');
    Require(vecEmpty.size == 0);
    RequireThrows([] { _ZNK3WTF6String5splitEDs(nullptr, ','); });

    auto vec2 = _ZNK3WTF6String25splitAllowingEmptyEntriesEDs(&spStr, ',');
    Require(vec2.size == 4);
    Require(ToString(vec2.buffer[0]) == "foo");
    Require(ToString(vec2.buffer[1]) == "bar");
    Require(ToString(vec2.buffer[2]).empty());
    Require(ToString(vec2.buffer[3]) == "baz");

    auto emptyAllow = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(""), 0);
    auto vecAllowEmpty = _ZNK3WTF6String25splitAllowingEmptyEntriesEDs(&emptyAllow, ',');
    Require(vecAllowEmpty.size == 1);
    Require(ToString(vecAllowEmpty.buffer[0]).empty());
    RequireThrows([] { _ZNK3WTF6String25splitAllowingEmptyEntriesEDs(nullptr, ','); });

    auto ntStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("test"), 4);
    auto ntVec = _ZNK3WTF6String29charactersWithNullTerminationEv(&ntStr);
    Require(ntVec.size == 5);
    Require(ntVec.buffer[0] == 't' && ntVec.buffer[1] == 'e' && ntVec.buffer[2] == 's' && ntVec.buffer[3] == 't' && ntVec.buffer[4] == 0);

    auto ntEmpty = _ZNK3WTF6String29charactersWithNullTerminationEv(&emptyAllow);
    Require(ntEmpty.size == 1 && ntEmpty.buffer[0] == 0);
    RequireThrows([] { _ZNK3WTF6String29charactersWithNullTerminationEv(nullptr); });

    auto numStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("123.5"), 5);
    bool okF = false;
    bool okD = false;
    float fVal = _ZNK3WTF6String7toFloatEPb(&numStr, &okF);
    Require(okF && std::abs(fVal - 123.5f) < 0.001f);
    float fValNoOk = _ZNK3WTF6String7toFloatEPb(&numStr, nullptr);
    Require(std::abs(fValNoOk - 123.5f) < 0.001f);

    double dVal = _ZNK3WTF6String8toDoubleEPb(&numStr, &okD);
    Require(okD && std::abs(dVal - 123.5) < 0.0001);
    double dValNoOk = _ZNK3WTF6String8toDoubleEPb(&numStr, nullptr);
    Require(std::abs(dValNoOk - 123.5) < 0.0001);

    auto numNegStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("-0.25"), 5);
    bool okNegF = false;
    float fNeg = _ZNK3WTF6String7toFloatEPb(&numNegStr, &okNegF);
    Require(okNegF && std::abs(fNeg - (-0.25f)) < 0.001f);

    auto numPrecStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("1.23456789"), 10);
    bool okPrecD = false;
    double dPrec = _ZNK3WTF6String8toDoubleEPb(&numPrecStr, &okPrecD);
    Require(okPrecD && std::abs(dPrec - 1.23456789) < 0.0000001);

    auto badStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("not_a_number"), 12);
    bool badOkF = true;
    bool badOkD = true;
    _ZNK3WTF6String7toFloatEPb(&badStr, &badOkF);
    Require(!badOkF);
    _ZNK3WTF6String8toDoubleEPb(&badStr, &badOkD);
    Require(!badOkD);

    auto badTrailingStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("123xyz"), 6);
    bool badTrailOkD = true;
    _ZNK3WTF6String8toDoubleEPb(&badTrailingStr, &badTrailOkD);
    Require(!badTrailOkD);

    bool emptyOkF = true;
    bool emptyOkD = true;
    _ZNK3WTF6String7toFloatEPb(&emptyAllow, &emptyOkF);
    Require(!emptyOkF);
    _ZNK3WTF6String8toDoubleEPb(&emptyAllow, &emptyOkD);
    Require(!emptyOkD);

    RequireThrows([] { _ZNK3WTF6String7toFloatEPb(nullptr, nullptr); });
    RequireThrows([] { _ZNK3WTF6String8toDoubleEPb(nullptr, nullptr); });

    auto origStr = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("isolated"), 8);
    auto copy1 = _ZNKR3WTF6String12isolatedCopyEv(&origStr);
    Require(copy1.impl != origStr.impl);
    Require(ToString(copy1) == "isolated");

    String nullStr(nullptr);
    auto nullCopy1 = _ZNKR3WTF6String12isolatedCopyEv(&nullStr);
    Require(nullCopy1.impl == nullptr);

    auto emptyCopy1 = _ZNKR3WTF6String12isolatedCopyEv(&emptyAllow);
    Require(ToString(emptyCopy1).empty());
    RequireThrows([] { _ZNKR3WTF6String12isolatedCopyEv(nullptr); });

    auto origStr2 = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>("isolated"), 8);
    auto copy2 = _ZNO3WTF6String12isolatedCopyEv(&origStr2);
    Require(ToString(copy2) == "isolated");

    String nullRval(nullptr);
    auto nullCopy2 = _ZNO3WTF6String12isolatedCopyEv(&nullRval);
    Require(nullCopy2.impl == nullptr);

    String emptyRval = _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(""), 0);
    auto emptyCopy2 = _ZNO3WTF6String12isolatedCopyEv(&emptyRval);
    Require(ToString(emptyCopy2).empty());
    RequireThrows([] { _ZNO3WTF6String12isolatedCopyEv(nullptr); });
}

}  // namespace

int main() {
    TestURL();
    TestStringImplReplace();
    TestStringViewSplitResultIterator();
    TestPersistence();
    TestString();
    return 0;
}
