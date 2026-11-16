// SPDX-FileCopyrightText: Copyright 2026 shadPS4 Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later
// Derived from shadPS4 src/core/libraries/np/np_web_api/np_web_api.cpp@2183807e592b36d4440384878165b43d930c1dee

#include "prx/libSceNpWebApi/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "SceTypes.hpp"
#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>
#include <string>
#include <string_view>

namespace {

constexpr int SCE_NP_ERROR_INVALID_ARGUMENT = static_cast<int>(0x80550003);

bool CopyGuestCString(const char* source, std::string& destination) {
    if (!source) return false;
    destination.clear();
    const int savedErrno = errno;
    constexpr std::size_t kMinPageSize = 4096;
    std::array<char, kMinPageSize> chunk{};
    std::uintptr_t address = reinterpret_cast<std::uintptr_t>(source);
    for (;;) {
        const std::size_t toPageEnd = kMinPageSize - (address & (kMinPageSize - 1));
        const std::uintptr_t remaining = std::numeric_limits<std::uintptr_t>::max() - address;
        const std::size_t chunkBytes = remaining < (toPageEnd - 1) ? static_cast<std::size_t>(remaining) + 1 : toPageEnd;
        if (GuestCopyIn_nid_no_patch(chunk.data(), reinterpret_cast<const void*>(address), chunkBytes)) {
            const void* nul = std::memchr(chunk.data(), '\0', chunkBytes);
            if (nul != nullptr) {
                destination.append(chunk.data(), static_cast<std::size_t>(static_cast<const char*>(nul) - chunk.data()));
                errno = savedErrno;
                return true;
            }
            destination.append(chunk.data(), chunkBytes);
            if (remaining < chunkBytes) {
                errno = savedErrno;
                return false;
            }
            address += chunkBytes;
            continue;
        }
        for (;;) {
            char ch = '\0';
            if (!GuestCopyIn_nid_no_patch(&ch, reinterpret_cast<const void*>(address), 1)) {
                errno = savedErrno;
                return false;
            }
            if (ch == '\0') {
                errno = savedErrno;
                return true;
            }
            destination.push_back(ch);
            if (address == std::numeric_limits<std::uintptr_t>::max()) {
                errno = savedErrno;
                return false;
            }
            ++address;
        }
    }
}

std::string DecodeBase64(std::string_view in) {
    auto val = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;
    };
    std::string out;
    int buf = 0;
    int bits = 0;
    for (char c : in) {
        if (c == '=' || c == '\0') break;
        const int v = val(c);
        if (v < 0) continue;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

}

extern "C" {

int APS5_VABI sceNpWebApiUtilityParseNpId(const char* jsonNpId, NpId* npId) {
    std::string encoded;
    if (!CopyGuestCString(jsonNpId, encoded)) return SCE_NP_ERROR_INVALID_ARGUMENT;
    const std::string decoded = DecodeBase64(encoded);
    std::string handle;
    std::string opt;
    const std::size_t at = decoded.find('@');
    if (at == std::string::npos) {
        handle = decoded;
    } else {
        handle = decoded.substr(0, at);
        for (std::size_t i = at + 1; i < decoded.size(); ++i) {
            const char c = decoded[i];
            if (c != '/' && c != '.') {
                opt.push_back(c);
            }
        }
    }
    if (npId) {
        NpId local{};
        std::memcpy(local.handle.data, handle.data(), std::min<std::size_t>(handle.size(), 16));
        std::memcpy(local.opt, opt.data(), std::min<std::size_t>(opt.size(), sizeof(local.opt)));
        local.reserved[0] = 0x01;
        const int savedErrno = errno;
        const bool copied = GuestCopyOut_nid_no_patch(npId, &local, sizeof(local));
        errno = savedErrno;
        if (!copied) return SCE_NP_ERROR_INVALID_ARGUMENT;
    }
    return 0;
}

}
