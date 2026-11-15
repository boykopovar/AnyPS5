#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <new>
#include <stdexcept>

using NKWebKit::WebCoreAbi::PasteboardPlainText;
using NKWebKit::WebCoreAbi::PasteboardURLData;
using NKWebKit::WebCoreAbi::PasteboardWriterData;
using NKWebKit::WtfAbi::String;

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "PasteboardWriterData check failed at line %d\n", line);
        std::abort();
    }
}
#define Require(value) Check((value), __LINE__)

template <typename Action>
static bool ThrowsInvalidArgument(Action action) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

static String Latin1(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), static_cast<unsigned>(std::strlen(text)));
}

struct Poisoned {
    alignas(PasteboardWriterData) unsigned char bytes[sizeof(PasteboardWriterData)];
    Poisoned() { std::memset(bytes, 0xAB, sizeof(bytes)); }
    PasteboardWriterData* get() { return reinterpret_cast<PasteboardWriterData*>(bytes); }
};

static void ConstructsEmpty() {
    for (auto construct : {&_ZN7WebCore20PasteboardWriterDataC1Ev, &_ZN7WebCore20PasteboardWriterDataC2Ev}) {
        Poisoned storage;
        construct(storage.get());
        Require(storage.bytes[0x10] == 0 && storage.bytes[0x48] == 0 && storage.bytes[0x51] == 0);
        Require(_ZNK7WebCore20PasteboardWriterData7isEmptyEv(storage.get()));
        _ZN7WebCore20PasteboardWriterDataD2Ev(storage.get());
    }
}

static void ReportsEachMember() {
    Poisoned storage;
    auto* data = storage.get();
    _ZN7WebCore20PasteboardWriterDataC1Ev(data);
    data->webContent.engaged = true;
    Require(!_ZNK7WebCore20PasteboardWriterData7isEmptyEv(data));
    _ZN7WebCore20PasteboardWriterDataD1Ev(data);
    _ZN7WebCore20PasteboardWriterDataC1Ev(data);
    auto text = Latin1("copied text");
    new (data->plainText.storage) PasteboardPlainText{true, text};
    data->plainText.engaged = true;
    Require(!_ZNK7WebCore20PasteboardWriterData7isEmptyEv(data) && text.impl->refCount == 4);
    _ZN7WebCore20PasteboardWriterDataD1Ev(data);
    Require(text.impl->refCount == 2);
}

static void DestroysEngagedURLData() {
    Poisoned storage;
    auto* data = storage.get();
    _ZN7WebCore20PasteboardWriterDataC2Ev(data);
    auto address = Latin1("https://example.com/");
    auto title = Latin1("Example");
    auto* url = new (data->url.storage) PasteboardURLData{};
    url->url.impl = address.impl;
    address.impl->refCount += 2;
    url->title = title;
    data->url.engaged = true;
    Require(!_ZNK7WebCore20PasteboardWriterData7isEmptyEv(data));
    Require(address.impl->refCount == 4 && title.impl->refCount == 4);
    _ZN7WebCore20PasteboardWriterDataD2Ev(data);
    Require(address.impl->refCount == 2 && title.impl->refCount == 2);
}

static void RefusesNull() {
    Require(ThrowsInvalidArgument([] { _ZN7WebCore20PasteboardWriterDataC1Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZN7WebCore20PasteboardWriterDataD2Ev(nullptr); }));
    Require(ThrowsInvalidArgument([] { _ZNK7WebCore20PasteboardWriterData7isEmptyEv(nullptr); }));
}

int main() {
    ConstructsEmpty();
    ReportsEachMember();
    DestroysEngagedURLData();
    RefusesNull();
    std::puts("PasteboardWriterData passed");
    return 0;
}
