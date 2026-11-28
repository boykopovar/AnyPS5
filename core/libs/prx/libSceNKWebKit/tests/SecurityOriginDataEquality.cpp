#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>
#include <utility>

static void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WebCore::SecurityOriginData equality check failed at line %d\n", line);
        std::abort();
    }
}

#define Require(value) Check((value), __LINE__)

using NKWebKit::WebCoreAbi::OpaqueOriginIdentifier;
using NKWebKit::WebCoreAbi::OptionalPort;
using NKWebKit::WebCoreAbi::SecurityOriginData;
using NKWebKit::WebCoreAbi::SecurityOriginTuple;
using NKWebKit::WtfAbi::String;

static String Text(const char* text) {
    return _ZN3WTF6String8fromUTF8EPKh(reinterpret_cast<const unsigned char*>(text));
}

struct Origin {
    SecurityOriginData data;

    Origin(const char* protocol, const char* host, OptionalPort port) {
        new (&data.storage.tuple) SecurityOriginTuple{protocol ? Text(protocol) : String(), host ? Text(host) : String(), port};
        data.index = NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex;
    }

    Origin(std::uint64_t object, std::uint64_t process) {
        data.storage.opaque = OpaqueOriginIdentifier{object, process};
        data.index = NKWebKit::WebCoreAbi::kSecurityOriginOpaqueIndex;
    }

    explicit Origin(std::uint32_t index) {
        data.index = index;
    }

    ~Origin() {
        if (data.index == NKWebKit::WebCoreAbi::kSecurityOriginTupleIndex) data.storage.tuple.~SecurityOriginTuple();
    }
};

static bool Equal(const Origin& a, const Origin& b) {
    const bool result = _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&a.data, &b.data);
    Require(result == _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&b.data, &a.data));
    return result;
}

template<typename TFunction>
static bool Throws(TFunction function) {
    try {
        function();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

int main() {
    const OptionalPort none{0, false};
    const OptionalPort port443{443, true};
    const OptionalPort port8080{8080, true};

    Origin https("https", "example.com", none);
    Require(Equal(https, https));
    Require(Equal(https, Origin("https", "example.com", none)));
    Require(Equal(https, Origin("https", "example.com", OptionalPort{77, false})));
    Require(!Equal(https, Origin("http", "example.com", none)));
    Require(!Equal(https, Origin("https", "example.org", none)));
    Require(!Equal(https, Origin("https", "Example.com", none)));
    Require(!Equal(https, Origin("https", "example.com", port443)));
    Require(Equal(Origin("https", "example.com", port8080), Origin("https", "example.com", port8080)));
    Require(!Equal(Origin("https", "example.com", port8080), Origin("https", "example.com", port443)));
    Require(Equal(Origin(nullptr, nullptr, none), Origin(nullptr, nullptr, none)));
    Require(!Equal(Origin(nullptr, nullptr, none), Origin("", "", none)));

    Origin opaque(5, 9);
    Require(Equal(opaque, opaque));
    Require(Equal(opaque, Origin(5, 9)));
    Require(!Equal(opaque, Origin(5, 10)));
    Require(!Equal(opaque, Origin(6, 9)));
    Require(!Equal(opaque, Origin(nullptr, nullptr, none)));
    Require(!Equal(Origin(0, 0), Origin(nullptr, nullptr, none)));

    const std::uint32_t valueless = std::numeric_limits<std::uint32_t>::max();
    Require(Equal(Origin(valueless), Origin(valueless)));
    Require(!Equal(Origin(valueless), opaque));
    Require(!Equal(Origin(valueless), https));

    Origin invalid(2u);
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&invalid.data, &https.data); }));
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, &invalid.data); }));
    Require(_ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&invalid.data, &invalid.data));
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(nullptr, &https.data); }));
    Require(Throws([&] { _ZN7WebCoreeqERKNS_18SecurityOriginDataES2_(&https.data, nullptr); }));

    std::puts("libSceNKWebKit WebCore::SecurityOriginData equality passed");
    return 0;
}
