#include "prx/libSceJsc/include/ApiBridgeExports.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>

using namespace Jsc::WtfAbi;

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "Jsc text bridge check failed at line %d\n", line);
        std::exit(1);
    }
}

#define Require(value) Check((value), __LINE__)

unsigned urlCalls = 0;
unsigned indentCalls = 0;
unsigned numberCalls = 0;
URL* lastUrl = nullptr;
TextStream* lastStream = nullptr;
int lastIndent = 0;
double lastValue = 0;

void RecordUrl(URL* url) {
    ++urlCalls;
    lastUrl = url;
}

void RecordIndent(TextStream* stream, int indent) {
    ++indentCalls;
    lastStream = stream;
    lastIndent = indent;
}

void RecordNumber(TextStream* stream, double value) {
    ++numberCalls;
    lastStream = stream;
    lastValue = value;
}

template<typename Exception, typename Function>
bool Throws(Function function) {
    try {
        function();
    } catch (const Exception&) {
        return true;
    }
    return false;
}

bool SameUrl(const URL& url, const URL& reference) {
    return std::memcmp(&url, &reference, sizeof(URL)) == 0;
}

struct alignas(8) StreamStorage {
    unsigned char bytes[64];
};

TextStream* Handle(StreamStorage& storage) {
    return reinterpret_cast<TextStream*>(&storage);
}

bool SameStream(const StreamStorage& stream, const StreamStorage& reference) {
    return std::memcmp(&stream, &reference, sizeof(StreamStorage)) == 0;
}

struct Calls {
    unsigned url = urlCalls;
    unsigned indent = indentCalls;
    unsigned number = numberCalls;

    bool Unchanged() const {
        return url == urlCalls && indent == indentCalls && number == numberCalls;
    }
};

void RequireUnbridged() {
    URL url{};
    url.bits = URLValidBit;
    url.queryEnd = 3;
    const URL urlBefore = url;
    StreamStorage stream{};
    stream.bytes[44] = 4;
    const StreamStorage streamBefore = stream;
    const FormatNumberRespectingIntegers number{1.5};
    const Calls calls{};

    Require(Throws<std::runtime_error>([&] { _ZN3WTF3URL24removeFragmentIdentifierEv(&url); }));
    Require(Throws<std::runtime_error>([&] { _ZN3WTF11writeIndentERNS_10TextStreamEi(Handle(stream), 2); }));
    Require(Throws<std::runtime_error>([&] { _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(Handle(stream), &number); }));
    Require(SameUrl(url, urlBefore) && SameStream(stream, streamBefore));
    Require(calls.Unchanged());
}

void RequireNullArgumentsThrow() {
    StreamStorage stream{};
    const FormatNumberRespectingIntegers number{1.5};
    const Calls calls{};
    Require(Throws<std::invalid_argument>([] { _ZN3WTF3URL24removeFragmentIdentifierEv(nullptr); }));
    Require(Throws<std::invalid_argument>([] { _ZN3WTF11writeIndentERNS_10TextStreamEi(nullptr, 1); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(nullptr, &number); }));
    Require(Throws<std::invalid_argument>([&] { _ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(Handle(stream), nullptr); }));
    Require(calls.Unchanged());
}

void TestForwarding() {
    URL url{};
    StreamStorage firstStorage{};
    StreamStorage secondStorage{};
    TextStream* const first = Handle(firstStorage);
    TextStream* const second = Handle(secondStorage);

    _ZN3WTF3URL24removeFragmentIdentifierEv(&url);
    Require(urlCalls == 1 && lastUrl == &url && indentCalls == 0 && numberCalls == 0);

    _ZN3WTF11writeIndentERNS_10TextStreamEi(first, 7);
    Require(indentCalls == 1 && lastStream == first && lastIndent == 7);
    _ZN3WTF11writeIndentERNS_10TextStreamEi(second, -3);
    Require(indentCalls == 2 && lastStream == second && lastIndent == -3);
    Require(urlCalls == 1 && numberCalls == 0);

    const FormatNumberRespectingIntegers fraction{2.25};
    Require(_ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(first, &fraction) == first);
    Require(numberCalls == 1 && lastStream == first && lastValue == 2.25);
    const FormatNumberRespectingIntegers negativeZero{-0.0};
    Require(_ZN3WTF10TextStreamlsERKNS0_30FormatNumberRespectingIntegersE(second, &negativeZero) == second);
    Require(numberCalls == 2 && lastStream == second && lastValue == 0.0 && std::signbit(lastValue));
    Require(urlCalls == 1 && indentCalls == 2);
}

}

int main() {
    RequireUnbridged();
    RequireNullArgumentsThrow();

    const Jsc::ApiBridge partial{nullptr, nullptr, nullptr};
    RegisterApiBridge_nid_no_patch(&partial);
    RequireUnbridged();
    RequireNullArgumentsThrow();

    const Jsc::ApiBridge full{nullptr, nullptr, nullptr, &RecordUrl, &RecordIndent, &RecordNumber};
    RegisterApiBridge_nid_no_patch(&full);
    RequireNullArgumentsThrow();
    TestForwarding();

    RegisterApiBridge_nid_no_patch(nullptr);
    RequireUnbridged();
    return 0;
}
