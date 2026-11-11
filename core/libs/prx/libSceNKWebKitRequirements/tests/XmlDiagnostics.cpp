#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <libxml/parser.h>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace {

struct Messages {
    std::vector<std::string> text;
    bool valid = true;
};

void NativeMessage(void* context, const char* format, ...) {
    auto& messages = *static_cast<Messages*>(context);
    char buffer[32768];
    std::va_list args;
    va_start(args, format);
    const int length = std::vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    if (length < 0 || length >= static_cast<int>(sizeof(buffer))) {
        messages.valid = false;
        return;
    }
    messages.text.emplace_back(buffer, static_cast<std::size_t>(length));
}

void APS5_VABI GuestMessage(void* context, const char* format, ...) {
    auto& messages = *static_cast<Messages*>(context);
    if (std::strcmp(format, "%s") != 0) {
        messages.valid = false;
        return;
    }
#ifdef _WIN32
    __builtin_sysv_va_list args;
    __builtin_sysv_va_start(args, format);
    const char* text = __builtin_va_arg(args, const char*);
    __builtin_sysv_va_end(args);
#else
    std::va_list args;
    va_start(args, format);
    const char* text = va_arg(args, const char*);
    va_end(args);
#endif
    messages.text.emplace_back(text);
}

void NativeStartElement(void*, const xmlChar*, const xmlChar*, const xmlChar*, int, const xmlChar**, int, int, const xmlChar**) {}
void APS5_VABI GuestStartElement(void*, const uint8_t*, const uint8_t*, const uint8_t*, int32_t, const uint8_t**, int32_t, int32_t, const uint8_t**) {}

bool Check(const char* kind, const std::string& input) {
    Messages expected;
    xmlSAXHandler nativeSax{};
    nativeSax.initialized = XML_SAX2_MAGIC;
    nativeSax.startElementNs = NativeStartElement;
    nativeSax.warning = NativeMessage;
    nativeSax.error = NativeMessage;
    nativeSax.fatalError = NativeMessage;
    xmlParserCtxtPtr native = xmlCreatePushParserCtxt(&nativeSax, &expected, nullptr, 0, "diagnostic.xml");
    if (!native) return false;
    const int nativeResult = xmlParseChunk(native, input.data(), static_cast<int>(input.size()), 1);

    Messages observed;
    NKWebKit::GuestSAXHandler guestSax{};
    guestSax.initialized = XML_SAX2_MAGIC;
    guestSax.startElementNs = GuestStartElement;
    guestSax.warning = GuestMessage;
    guestSax.error = GuestMessage;
    guestSax.fatalError = GuestMessage;
    auto* guest = xmlCreatePushParserCtxt_nid_postfix(&guestSax, &observed, nullptr, 0, "diagnostic.xml");
    if (!guest) {
        xmlFreeParserCtxt(native);
        return false;
    }
    const int guestResult = xmlParseChunk_nid_postfix(guest, input.data(), static_cast<int>(input.size()), 1);
    const bool passed = expected.valid && observed.valid && !expected.text.empty()
        && guestResult == nativeResult && observed.text == expected.text;
    if (!passed) {
        std::fprintf(stderr, "XML %s (%zu input bytes): expected %zu messages, observed %zu\n", kind, input.size(), expected.text.size(), observed.text.size());
        for (std::size_t i = 0; i < expected.text.size() && i < observed.text.size(); ++i)
            std::fprintf(stderr, "message %zu: expected %zu bytes, observed %zu bytes\n", i, expected.text[i].size(), observed.text[i].size());
    }
    xmlFreeParserCtxt_nid_postfix(guest);
    xmlFreeParserCtxt(native);
    return passed;
}

} // namespace

int main() {
    constexpr int lengths[]{1, 64, 480, 490, 500, 512, 1400, 4096};
    int matched = 0;
    int checked = 0;
    for (const int length : lengths) {
        const std::string open(length, 'a');
        const std::string close(length, 'b');
        ++checked;
        if (Check("tag mismatch", "<" + open + "></" + close + ">")) ++matched;
        ++checked;
        if (Check("relative namespace warning", "<root xmlns=\"" + open + "\"/>")) ++matched;
    }
    std::printf("XML diagnostic messages: %d/%d cases match bundled libxml2\n", matched, checked);
    return matched == checked ? 0 : 1;
}
