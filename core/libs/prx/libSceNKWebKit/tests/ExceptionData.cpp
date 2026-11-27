#include "prx/libc/include/General.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <source_location>
#include <stdexcept>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

namespace {

using NKWebKit::WebCoreAbi::ExceptionData;
using NKWebKit::WtfAbi::String;

using IsolatedCopy = ExceptionData* (APS5_VABI *)(void*, const ExceptionData*);

const auto isolatedCopy = std::bit_cast<IsolatedCopy>(&_ZNK7WebCore13ExceptionData12isolatedCopyEv);

void Require(bool condition, const char* message, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "ExceptionData line %u: %s\n", location.line(), message);
        std::abort();
    }
}

String Text(const char* text) {
    return _ZN3WTF10StringImpl6createEPKhj(reinterpret_cast<const unsigned char*>(text), std::strlen(text));
}

String Wide(const std::u16string& text) {
    return _ZN3WTF10StringImpl6createEPKDsj(text.data(), text.size());
}

std::u16string Units(const String& text) {
    std::u16string result;
    if (!text.impl) return result;
    for (unsigned i = 0; i < text.impl->length; ++i) {
        if (text.impl->hashAndFlags & 4) result.push_back(static_cast<const unsigned char*>(text.impl->data)[i]);
        else result.push_back(static_cast<const char16_t*>(text.impl->data)[i]);
    }
    return result;
}

void Check(std::uint32_t code, const String& message) {
    ExceptionData source { code, String(message) };
    const auto references = message.impl ? message.impl->refCount : 0;
    struct {
        std::array<unsigned char, 16> before;
        alignas(ExceptionData) std::array<unsigned char, sizeof(ExceptionData)> storage;
        std::array<unsigned char, 16> after;
    } guarded;
    guarded.before.fill(0xA5);
    guarded.after.fill(0x5A);
    guarded.storage.fill(0xCC);
    auto* copy = isolatedCopy(guarded.storage.data(), &source);
    Require(static_cast<void*>(copy) == guarded.storage.data(), "result is written through the hidden pointer");
    Require(std::all_of(guarded.before.begin(), guarded.before.end(), [](auto x) { return x == 0xA5; }), "leading canary");
    Require(std::all_of(guarded.after.begin(), guarded.after.end(), [](auto x) { return x == 0x5A; }), "trailing canary");
    Require(copy->code == code, "code is copied");
    Require(source.code == code && source.message.impl == message.impl, "source is left as it was");
    Require(!copy->message.impl == !message.impl, "null message stays null");
    Require(Units(copy->message) == Units(message), "message keeps its text");
    const bool hasText = message.impl && message.impl->length;
    if (hasText) {
        Require(message.impl->refCount == references, "source string is not retained");
        Require(copy->message.impl != message.impl, "message is a new string");
        Require(copy->message.impl->refCount == 2, "new string has one owner");
        Require((copy->message.impl->hashAndFlags & 4) == (message.impl->hashAndFlags & 4), "message keeps its width");
    }
    std::destroy_at(copy);
    Require(!hasText || message.impl->refCount == references, "destroying the copy leaves the source string");
}

struct GuardedPages {
    unsigned char* base = nullptr;
    std::size_t pageSize = 0;

    GuardedPages() {
#ifdef _WIN32
        SYSTEM_INFO info{};
        GetSystemInfo(&info);
        pageSize = info.dwPageSize;
        base = static_cast<unsigned char*>(VirtualAlloc(nullptr, pageSize * 3, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE));
        Require(base != nullptr, "VirtualAlloc");
        DWORD previous = 0;
        Require(VirtualProtect(base + pageSize * 2, pageSize, PAGE_NOACCESS, &previous) != 0, "VirtualProtect guard");
#else
        const long size = sysconf(_SC_PAGESIZE);
        Require(size > 0, "page size");
        pageSize = static_cast<std::size_t>(size);
        void* mapped = mmap(nullptr, pageSize * 3, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        Require(mapped != MAP_FAILED, "mmap");
        base = static_cast<unsigned char*>(mapped);
        Require(mprotect(base + pageSize * 2, pageSize, PROT_NONE) == 0, "mprotect guard");
#endif
    }

    ~GuardedPages() {
        if (!base) return;
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, pageSize * 3);
#endif
    }

    void ProtectReadOnly() {
#ifdef _WIN32
        DWORD previous = 0;
        Require(VirtualProtect(base, pageSize, PAGE_READONLY, &previous) != 0, "VirtualProtect ro");
#else
        Require(mprotect(base, pageSize, PROT_READ) == 0, "mprotect ro");
#endif
    }

    unsigned char* Readable() const { return base; }
    unsigned char* Guard() const { return base + pageSize * 2; }
};

template<typename TFunction>
void RequireNotImplemented(TFunction&& function, const char* label) {
    const auto before = NotImplementedCalls_nid_no_patch();
    bool threw = false;
    try {
        function();
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Require(threw, label);
    Require(NotImplementedCalls_nid_no_patch() == before + 1, label);
}

void TestGuestPointers() {
    GuardedPages pages;
    const String message = Text("readonly error");
    ExceptionData source{42, String(message)};
    std::memcpy(pages.Readable(), &source, sizeof(source));
    pages.ProtectReadOnly();

    alignas(ExceptionData) std::array<unsigned char, sizeof(ExceptionData)> storage{};
    auto* roCopy = isolatedCopy(storage.data(), reinterpret_cast<const ExceptionData*>(pages.Readable()));
    Require(roCopy->code == 42 && Units(roCopy->message) == u"readonly error", "read-only guest ExceptionData succeeds");
    std::destroy_at(roCopy);

    RequireNotImplemented([&] {
        isolatedCopy(storage.data(), reinterpret_cast<const ExceptionData*>(pages.Guard()));
    }, "guard ExceptionData throws");

    RequireNotImplemented([&] {
        isolatedCopy(storage.data(), reinterpret_cast<const ExceptionData*>(pages.Guard() - (sizeof(ExceptionData) - 4)));
    }, "split ExceptionData throws");

    using NKWebKit::WtfAbi::StringImpl;
    ExceptionData badImpl{7, String()};
    badImpl.message.impl = reinterpret_cast<StringImpl*>(pages.Guard());
    RequireNotImplemented([&] {
        isolatedCopy(storage.data(), &badImpl);
    }, "guard StringImpl throws");
    badImpl.message.impl = nullptr;

    StringImpl badChars{2, 8, pages.Guard(), 4};
    ExceptionData badCharsData{7, String()};
    badCharsData.message.impl = &badChars;
    RequireNotImplemented([&] {
        isolatedCopy(storage.data(), &badCharsData);
    }, "guard characters throw");
    badCharsData.message.impl = nullptr;

    StringImpl splitChars{2, 8, pages.Guard() - 2, 0};
    ExceptionData splitCharsData{7, String()};
    splitCharsData.message.impl = &splitChars;
    RequireNotImplemented([&] {
        isolatedCopy(storage.data(), &splitCharsData);
    }, "split characters throw");
    splitCharsData.message.impl = nullptr;
}

}

int main() {
    try {
        Check(0, String());
        Check(8, Text("The object can not be found here."));
        Check(18, Wide(u"s\u00E9curit\u00E9 \u20AC \U0001F600"));
        Check(24, Text(""));
        Check(31, Text(std::string(5000, 'x').c_str()));
        Check(0xFFFFFFFFu, Text("out of range code"));
        bool threw = false;
        try {
            std::array<unsigned char, sizeof(ExceptionData)> storage {};
            isolatedCopy(storage.data(), nullptr);
        } catch (const std::invalid_argument&) {
            threw = true;
        }
        Require(threw, "null object throws");
        TestGuestPointers();
        std::puts("ExceptionData: isolatedCopy code, message, ownership and hidden-return checks passed");
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ExceptionData unexpected exception: %s\n", error.what());
        return 1;
    }
}
