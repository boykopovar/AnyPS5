#include "prx/libSceLibreSsl3/libSceLibreSsl3.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/libc.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace {

constexpr std::uint32_t SupportedProtocols = 0x18;
constexpr std::array<std::pair<std::string_view, std::uint32_t>, 9> ProtocolNames{{
    {"all", SupportedProtocols}, {"legacy", SupportedProtocols},
    {"default", SupportedProtocols}, {"secure", SupportedProtocols},
    {"tlsv1", SupportedProtocols}, {"tlsv1.0", 0x02},
    {"tlsv1.1", 0x04}, {"tlsv1.2", 0x08}, {"tlsv1.3", 0x10}
}};

bool EqualName(std::string_view text, std::string_view name) {
    if (text.size() != name.size()) return false;
    for (std::size_t index = 0; index < text.size(); ++index) {
        const unsigned char byte = static_cast<unsigned char>(text[index]);
        const unsigned char folded = byte >= 'A' && byte <= 'Z' ? byte + ('a' - 'A') : byte;
        if (folded != static_cast<unsigned char>(name[index])) return false;
    }
    return true;
}

std::string ReadGuestText(const char* text, const char* caller) {
    std::string copied;
    int error = CopyGuestString_nid_no_patch(text, 65536, copied);
    if (error == 63) error = CopyGuestString_nid_no_patch(text, 1u << 20, copied);
    if (error != 0) NotImplemented_nid_no_patch(caller);
    return copied;
}

bool TryWriteProtocols(std::uint32_t* protocols, std::uint32_t value) {
    return GuestCopyOut_nid_no_patch(protocols, &value, sizeof(value));
}

}

extern "C" int APS5_VABI tls_config_parse_protocols_nid_postfix(std::uint32_t* protocols, const char* text) {
    if (!protocols) throw std::invalid_argument("tls_config_parse_protocols: null output");
    if (!text) {
        if (!TryWriteProtocols(protocols, SupportedProtocols)) NotImplemented_nid_no_patch(__func__);
        return 0;
    }
    const std::string guestText = ReadGuestText(text, __func__);
    char* const copy = strdup_nid_postfix(guestText.c_str());
    if (!copy) return -1;
    std::string_view remaining(copy);
    std::uint32_t result = 0;
    for (;;) {
        const std::size_t separator = remaining.find_first_of(",:");
        std::string_view token = remaining.substr(0, separator);
        while (!token.empty() && (token.front() == ' ' || token.front() == '\t')) token.remove_prefix(1);
        const bool exclude = !token.empty() && token.front() == '!';
        if (exclude) token.remove_prefix(1);
        std::uint32_t selected = 0;
        for (const auto& [name, mask] : ProtocolNames) {
            if (EqualName(token, name)) {
                selected = mask;
                break;
            }
        }
        if (!selected) {
            free_nid_postfix(copy);
            return -1;
        }
        if (exclude) {
            if (!result) result = SupportedProtocols;
            result &= ~selected;
        } else {
            result |= selected;
        }
        if (separator == std::string_view::npos) break;
        remaining.remove_prefix(separator + 1);
    }
    const bool written = TryWriteProtocols(protocols, result);
    free_nid_postfix(copy);
    if (!written) NotImplemented_nid_no_patch(__func__);
    return 0;
}
