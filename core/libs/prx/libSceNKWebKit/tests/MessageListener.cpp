#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/include/WebKitValueExports.hpp"
#include "prx/libSceNKWebKit/src/Types.hpp"

#include <cstdio>
#include <cstdlib>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (!condition) {
        std::fprintf(stderr, "WKMessageListener check failed at line %u\n", location.line());
        std::abort();
    }
}

template<class Function>
void RequireInvalidArgument(Function function) {
    bool threw = false;
    try {
        function();
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    Require(threw);
}

}

int main() {
    Require(WKMessageListenerGetTypeID() == 71);

    auto listener1 = NKWebKit::Make<NKWebKit::MessageListener>();
    const void* ref1 = NKWebKit::ToRef(listener1);
    Require(!listener1->replied);
    Require(!listener1->reply);

    auto replyStr = NKWebKit::String::FromUtf8("hello reply");
    const void* replyRef = NKWebKit::ToRef(replyStr);

    WKMessageListenerSendReply(ref1, replyRef);
    Require(listener1->replied);
    Require(static_cast<bool>(listener1->reply));
    Require(WKGetTypeID(static_cast<const void*>(listener1->reply.Get())) == WKStringGetTypeID());

    auto listener2 = NKWebKit::Make<NKWebKit::MessageListener>();
    const void* ref2 = NKWebKit::ToRef(listener2);
    WKMessageListenerSendReply(ref2, nullptr);
    Require(listener2->replied);
    Require(!listener2->reply);

    auto invalidObj = NKWebKit::Make<NKWebKit::URLRequest>();
    const void* invalidRef = NKWebKit::ToRef(invalidObj);

    RequireInvalidArgument([&] { WKMessageListenerSendReply(nullptr, replyRef); });
    RequireInvalidArgument([&] { WKMessageListenerSendReply(invalidRef, replyRef); });

    WKRelease(ref1);
    WKRelease(ref2);
    WKRelease(replyRef);
    WKRelease(invalidRef);
    return 0;
}
