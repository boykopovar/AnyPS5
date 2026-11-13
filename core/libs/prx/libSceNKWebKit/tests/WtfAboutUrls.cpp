#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <array>
#include <barrier>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <source_location>
#include <string_view>
#include <thread>

namespace {

using namespace NKWebKit::WtfAbi;

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "About URL check failed at line %u\n", location.line());
        std::abort();
    }
}

void Text(StringView view, std::string_view expected) {
    Require(view.is8Bit);
    Require(view.length == expected.size());
    Require(view.data);
    Require(std::string_view(static_cast<const char*>(view.data), view.length) == expected);
}

void Check(const URL& url, std::string_view literal, bool blank) {
    Require(url.bits == 0x145);
    Require((url.bits & 1) == 1);
    Require((url.bits & 2) == 0);
    Require((url.bits & 4) == 4);
    Require(((url.bits >> 3) & 7) == 0);
    Require((url.bits >> 6) == 5);
    Require(url.userStart == 6);
    Require(url.userEnd == 6);
    Require(url.passwordEnd == 6);
    Require(url.hostEnd == 6);
    Require(url.pathAfterLastSlash == 6);
    Require(url.pathEnd == literal.size());
    Require(url.queryEnd == literal.size());
    Require(url.impl);
    Require(url.impl->refCount == 1);
    Require((url.impl->hashAndFlags & 255) == 12);
    Text({url.impl->data, url.impl->length, !!(url.impl->hashAndFlags & 4)}, literal);
    unsigned byteLength = 0;
    unsigned utf16Length = 0;
    unsigned hash = _ZN3WTF7Unicode51calculateStringHashAndLengthFromUTF8MaskingTop8BitsEPKcS2_RjS3_(
        literal.data(), literal.data() + literal.size(), byteLength, utf16Length);
    Require(byteLength == literal.size() && utf16Length == literal.size());
    Require((url.impl->hashAndFlags >> 8) == hash);
    {
        String borrowed(url.impl);
        borrowed.impl->refCount += 2;
        Require(url.impl->refCount == 3);
        {
            auto retained = borrowed;
            Require(url.impl->refCount == 5);
        }
        Require(url.impl->refCount == 3);
    }
    Require(url.impl->refCount == 1);
    Text({url.impl->data, url.impl->length, true}, literal);
    Text(_ZNK3WTF3URL8protocolEv(&url), "about");
    Text(_ZNK3WTF3URL4pathEv(&url), blank ? "blank" : "srcdoc");
    Require(_ZNK3WTF3URL12isAboutBlankEv(&url) == blank);
    Require(_ZNK3WTF3URL13isAboutSrcDocEv(&url) == !blank);
    Require(!_ZNK3WTF3URL14isHierarchicalEv(&url));
    Text(_ZNK3WTF3URL4hostEv(&url), "");
}

void Run() {
    std::array<const URL*, 2> blanks{};
    std::array<const URL*, 2> srcdocs{};
    std::array<std::exception_ptr, 4> errors{};
    std::barrier start(2);
    auto call = [&](unsigned index) {
        start.arrive_and_wait();
        try {
            blanks[index] = &_ZN3WTF13aboutBlankURLEv();
        } catch (...) {
            errors[index] = std::current_exception();
        }
        start.arrive_and_wait();
        try {
            srcdocs[index] = &_ZN3WTF14aboutSrcDocURLEv();
        } catch (...) {
            errors[index + 2] = std::current_exception();
        }
    };
    std::thread first(call, 0);
    std::thread second(call, 1);
    first.join();
    second.join();
    for (const auto& error : errors) {
        if (!error) continue;
        try {
            std::rethrow_exception(error);
        } catch (const std::exception& exception) {
            std::fprintf(stderr, "Concurrent about URL exception: %s\n", exception.what());
        }
    }
    for (const auto& error : errors) if (error) std::rethrow_exception(error);
    Require(blanks[0] == blanks[1]);
    Require(srcdocs[0] == srcdocs[1]);
    Require(blanks[0] != srcdocs[0]);
    Require(blanks[0]->impl != srcdocs[0]->impl);
    for (unsigned i = 0; i < 10; ++i) {
        Require(blanks[0] == &_ZN3WTF13aboutBlankURLEv());
        Require(srcdocs[0] == &_ZN3WTF14aboutSrcDocURLEv());
        Check(*blanks[0], "about:blank", true);
        Check(*srcdocs[0], "about:srcdoc", false);
    }
}

}  // namespace

int main() {
    try {
        Run();
        std::puts("About URL checks passed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "About URL exception: %s\n", error.what());
        return 1;
    }
}
