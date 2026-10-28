#include "prx/libSceLibreSsl/libSceLibreSsl.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

struct Ipv6Text {
    unsigned char bytes[16];
    int total;
    int zeroPosition;
    int zeroCount;
};

bool Ipv4FromText(unsigned char* out, const char* text) {
    int parts[4];
    if (sscanf_nid_postfix(text, "%d.%d.%d.%d", &parts[0], &parts[1], &parts[2], &parts[3]) != 4) return false;
    for (int part : parts) {
        if (part < 0 || part > 255) return false;
    }
    for (int index = 0; index < 4; ++index) out[index] = static_cast<unsigned char>(parts[index]);
    return true;
}

bool Ipv6Hex(unsigned char* out, const char* text, int length) {
    if (length > 4) return false;
    unsigned int number = 0;
    while (length--) {
        const unsigned char character = static_cast<unsigned char>(*text++);
        number <<= 4;
        if (character >= '0' && character <= '9') number |= character - '0';
        else if (character >= 'A' && character <= 'F') number |= character - 'A' + 10;
        else if (character >= 'a' && character <= 'f') number |= character - 'a' + 10;
        else return false;
    }
    out[0] = static_cast<unsigned char>(number >> 8);
    out[1] = static_cast<unsigned char>(number & 0xff);
    return true;
}

bool Ipv6Element(const char* element, int length, Ipv6Text& state) {
    if (state.total == 16) return false;
    if (length == 0) {
        if (state.zeroPosition == -1) state.zeroPosition = state.total;
        else if (state.zeroPosition != state.total) return false;
        ++state.zeroCount;
        return true;
    }
    if (length > 4) {
        if (state.total > 12 || element[length]) return false;
        if (!Ipv4FromText(state.bytes + state.total, element)) return false;
        state.total += 4;
        return true;
    }
    if (!Ipv6Hex(state.bytes + state.total, element, length)) return false;
    state.total += 2;
    return true;
}

bool Ipv6Elements(const char* text, Ipv6Text& state) {
    const char* start = text;
    for (;;) {
        const char* separator = std::strchr(start, ':');
        bool accepted;
        if (separator == start || !*start) {
            accepted = Ipv6Element(nullptr, 0, state);
        } else {
            const char* last = separator ? separator - 1 : start + std::strlen(start) - 1;
            accepted = Ipv6Element(start, static_cast<int>(last - start + 1), state);
        }
        if (!accepted) return false;
        if (!separator) return true;
        start = separator + 1;
    }
}

bool Ipv6FromText(unsigned char* out, const char* text) {
    Ipv6Text state{{}, 0, -1, 0};
    if (!Ipv6Elements(text, state)) return false;
    if (state.zeroPosition == -1) {
        if (state.total != 16) return false;
        std::memcpy(out, state.bytes, 16);
        return true;
    }
    if (state.total == 16 || state.zeroCount > 3) return false;
    if (state.zeroCount == 3 && state.total > 0) return false;
    if (state.zeroCount == 2 && state.zeroPosition != 0 && state.zeroPosition != state.total) return false;
    if (state.zeroCount == 1 && (state.zeroPosition == 0 || state.zeroPosition == state.total)) return false;
    std::memcpy(out, state.bytes, state.zeroPosition);
    std::memset(out + state.zeroPosition, 0, 16 - state.total);
    if (state.total != state.zeroPosition)
        std::memcpy(out + state.zeroPosition + 16 - state.total, state.bytes + state.zeroPosition, state.total - state.zeroPosition);
    return true;
}

std::size_t AddressFromText(unsigned char* out, const char* text) {
    if (std::strchr(text, ':')) return Ipv6FromText(out, text) ? 16 : 0;
    return Ipv4FromText(out, text) ? 4 : 0;
}

LibreSslX509VerifyParamId* Identity(LibreSslX509VerifyParam* param, const char* function) {
    if (!param || !param->id) NotImplemented_nid_no_patch(function);
    return param->id;
}

} // namespace

extern "C" {

void APS5_VABI X509_VERIFY_PARAM_set_hostflags_nid_postfix(LibreSslX509VerifyParam* param, std::uint32_t flags) {
    Identity(param, __func__)->hostFlags = flags;
}

int APS5_VABI X509_VERIFY_PARAM_set1_ip_asc_nid_postfix(LibreSslX509VerifyParam* param, const char* text) {
    if (!text) NotImplemented_nid_no_patch(__func__);
    unsigned char address[16];
    const std::size_t length = AddressFromText(address, text);
    if (length == 0) return 0;
    auto* id = Identity(param, __func__);
    auto* copy = static_cast<unsigned char*>(malloc_nid_postfix(length));
    if (!copy) return 0;
    std::memcpy(copy, address, length);
    free_nid_postfix(id->ip);
    id->ip = copy;
    id->ipLength = length;
    return 1;
}

}
