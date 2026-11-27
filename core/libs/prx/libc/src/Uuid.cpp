/*-
 * Copyright (c) 2002,2005 Marcel Moolenaar
 * Copyright (c) 2002 Hiten Mahesh Pandya
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*-
 * Copyright (c) 2002 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Adapted from FreeBSD release/11.4.0 lib/libc/uuid: https://github.com/freebsd/freebsd-src/tree/8c639945a44588f6cfdddfb057bb5aad88735da0/lib/libc/uuid */
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {

constexpr std::uint32_t UuidStatusOk = 0;
constexpr std::uint32_t UuidStatusBadVersion = 1;
constexpr std::uint32_t UuidStatusInvalidString = 2;
constexpr std::uint32_t UuidStatusNoMemory = 3;
constexpr std::size_t UuidStringBytes = 37;

void SetStatus(std::uint32_t* status, std::uint32_t value) {
    if (status != nullptr) *status = value;
}

bool UuidIsNil(const GuestUuid* uuid) {
    if (uuid == nullptr) return true;
    const auto* words = reinterpret_cast<const std::uint32_t*>(uuid);
    return words[0] == 0 && words[1] == 0 && words[2] == 0 && words[3] == 0;
}

}

extern "C" std::int32_t APS5_VABI uuid_compare_nid_postfix(const GuestUuid* a, const GuestUuid* b, std::uint32_t* status) {
    SetStatus(status, UuidStatusOk);
    if (a == b) return 0;
    if (a == nullptr) return UuidIsNil(b) ? 0 : -1;
    if (b == nullptr) return UuidIsNil(a) ? 0 : 1;
    if (a->time_low != b->time_low) return a->time_low < b->time_low ? -1 : 1;
    if (a->time_mid != b->time_mid) return a->time_mid < b->time_mid ? -1 : 1;
    if (a->time_hi_and_version != b->time_hi_and_version) return a->time_hi_and_version < b->time_hi_and_version ? -1 : 1;
    if (a->clock_seq_hi_and_reserved != b->clock_seq_hi_and_reserved) return a->clock_seq_hi_and_reserved < b->clock_seq_hi_and_reserved ? -1 : 1;
    if (a->clock_seq_low != b->clock_seq_low) return a->clock_seq_low < b->clock_seq_low ? -1 : 1;
    const int result = std::memcmp(a->node, b->node, sizeof(a->node));
    if (result != 0) return result < 0 ? -1 : 1;
    return 0;
}

extern "C" void APS5_VABI uuid_create_nid_postfix(GuestUuid* uuid, std::uint32_t* status) {
    (void)uuid;
    (void)status;
    NotImplemented_nid_no_patch(__func__);
}

extern "C" void APS5_VABI uuid_create_nil_nid_postfix(GuestUuid* uuid, std::uint32_t* status) {
    SetStatus(status, UuidStatusOk);
    std::memset(uuid, 0, sizeof(*uuid));
}

extern "C" std::int32_t APS5_VABI uuid_equal_nid_postfix(const GuestUuid* a, const GuestUuid* b, std::uint32_t* status) {
    SetStatus(status, UuidStatusOk);
    if (a == b) return 1;
    if (a == nullptr) return UuidIsNil(b) ? 1 : 0;
    if (b == nullptr) return UuidIsNil(a) ? 1 : 0;
    return std::memcmp(a, b, sizeof(GuestUuid)) == 0 ? 1 : 0;
}

extern "C" void APS5_VABI uuid_from_string_nid_postfix(const char* text, GuestUuid* uuid, std::uint32_t* status) {
    if (text == nullptr || *text == '\0') {
        uuid_create_nil_nid_postfix(uuid, status);
        return;
    }
    SetStatus(status, UuidStatusInvalidString);
    if (std::strlen(text) != 36) return;
    if (text[8] != '-') return;
    const int converted = std::sscanf(text, "%8x-%4hx-%4hx-%2hhx%2hhx-%2hhx%2hhx%2hhx%2hhx%2hhx%2hhx",
        &uuid->time_low, &uuid->time_mid, &uuid->time_hi_and_version,
        &uuid->clock_seq_hi_and_reserved, &uuid->clock_seq_low, &uuid->node[0],
        &uuid->node[1], &uuid->node[2], &uuid->node[3], &uuid->node[4], &uuid->node[5]);
    if (converted != 11) return;
    const unsigned int variant = uuid->clock_seq_hi_and_reserved;
    if ((variant & 0x80) != 0x00 && (variant & 0xc0) != 0x80 && (variant & 0xe0) != 0xc0) {
        SetStatus(status, UuidStatusBadVersion);
    } else {
        SetStatus(status, UuidStatusOk);
    }
}

extern "C" std::uint16_t APS5_VABI uuid_hash_nid_postfix(const GuestUuid* uuid, std::uint32_t* status) {
    SetStatus(status, UuidStatusOk);
    return uuid != nullptr ? static_cast<std::uint16_t>(uuid->time_low & 0xffff) : 0;
}

extern "C" std::int32_t APS5_VABI uuid_is_nil_nid_postfix(const GuestUuid* uuid, std::uint32_t* status) {
    SetStatus(status, UuidStatusOk);
    return UuidIsNil(uuid) ? 1 : 0;
}

extern "C" void APS5_VABI uuid_to_string_nid_postfix(const GuestUuid* uuid, char** text, std::uint32_t* status) {
    SetStatus(status, UuidStatusOk);
    if (text == nullptr) return;
    GuestUuid nil{};
    if (uuid == nullptr) uuid = &nil;
    auto* buffer = static_cast<char*>(malloc_nid_postfix(UuidStringBytes));
    if (buffer != nullptr) {
        std::snprintf(buffer, UuidStringBytes, "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x",
            static_cast<unsigned int>(uuid->time_low), static_cast<unsigned int>(uuid->time_mid),
            static_cast<unsigned int>(uuid->time_hi_and_version), static_cast<unsigned int>(uuid->clock_seq_hi_and_reserved),
            static_cast<unsigned int>(uuid->clock_seq_low), static_cast<unsigned int>(uuid->node[0]),
            static_cast<unsigned int>(uuid->node[1]), static_cast<unsigned int>(uuid->node[2]),
            static_cast<unsigned int>(uuid->node[3]), static_cast<unsigned int>(uuid->node[4]),
            static_cast<unsigned int>(uuid->node[5]));
    }
    *text = buffer;
    if (buffer == nullptr) SetStatus(status, UuidStatusNoMemory);
}

extern "C" void APS5_VABI uuid_enc_le_nid_postfix(void* buffer, const GuestUuid* uuid) {
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    for (int i = 0; i < 4; ++i) bytes[i] = static_cast<std::uint8_t>(uuid->time_low >> (8 * i));
    for (int i = 0; i < 2; ++i) bytes[4 + i] = static_cast<std::uint8_t>(uuid->time_mid >> (8 * i));
    for (int i = 0; i < 2; ++i) bytes[6 + i] = static_cast<std::uint8_t>(uuid->time_hi_and_version >> (8 * i));
    bytes[8] = uuid->clock_seq_hi_and_reserved;
    bytes[9] = uuid->clock_seq_low;
    for (std::size_t i = 0; i < sizeof(uuid->node); ++i) bytes[10 + i] = uuid->node[i];
}

extern "C" void APS5_VABI uuid_dec_le_nid_postfix(const void* buffer, GuestUuid* uuid) {
    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    uuid->time_low = static_cast<std::uint32_t>(bytes[0]) | static_cast<std::uint32_t>(bytes[1]) << 8 |
        static_cast<std::uint32_t>(bytes[2]) << 16 | static_cast<std::uint32_t>(bytes[3]) << 24;
    uuid->time_mid = static_cast<std::uint16_t>(bytes[4] | bytes[5] << 8);
    uuid->time_hi_and_version = static_cast<std::uint16_t>(bytes[6] | bytes[7] << 8);
    uuid->clock_seq_hi_and_reserved = bytes[8];
    uuid->clock_seq_low = bytes[9];
    for (std::size_t i = 0; i < sizeof(uuid->node); ++i) uuid->node[i] = bytes[10 + i];
}

extern "C" void APS5_VABI uuid_enc_be_nid_postfix(void* buffer, const GuestUuid* uuid) {
    auto* bytes = static_cast<std::uint8_t*>(buffer);
    for (int i = 0; i < 4; ++i) bytes[i] = static_cast<std::uint8_t>(uuid->time_low >> (24 - 8 * i));
    for (int i = 0; i < 2; ++i) bytes[4 + i] = static_cast<std::uint8_t>(uuid->time_mid >> (8 - 8 * i));
    for (int i = 0; i < 2; ++i) bytes[6 + i] = static_cast<std::uint8_t>(uuid->time_hi_and_version >> (8 - 8 * i));
    bytes[8] = uuid->clock_seq_hi_and_reserved;
    bytes[9] = uuid->clock_seq_low;
    for (std::size_t i = 0; i < sizeof(uuid->node); ++i) bytes[10 + i] = uuid->node[i];
}

extern "C" void APS5_VABI uuid_dec_be_nid_postfix(const void* buffer, GuestUuid* uuid) {
    const auto* bytes = static_cast<const std::uint8_t*>(buffer);
    uuid->time_low = static_cast<std::uint32_t>(bytes[0]) << 24 | static_cast<std::uint32_t>(bytes[1]) << 16 |
        static_cast<std::uint32_t>(bytes[2]) << 8 | static_cast<std::uint32_t>(bytes[3]);
    uuid->time_mid = static_cast<std::uint16_t>(bytes[4] << 8 | bytes[5]);
    uuid->time_hi_and_version = static_cast<std::uint16_t>(bytes[6] << 8 | bytes[7]);
    uuid->clock_seq_hi_and_reserved = bytes[8];
    uuid->clock_seq_low = bytes[9];
    for (std::size_t i = 0; i < sizeof(uuid->node); ++i) uuid->node[i] = bytes[10 + i];
}
