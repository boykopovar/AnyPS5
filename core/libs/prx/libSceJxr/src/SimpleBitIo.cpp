//*@@@+++@@@@******************************************************************
//
// Copyright © Microsoft Corp.
// All rights reserved.
// 
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
// 
// • Redistributions of source code must retain the above copyright notice,
//   this list of conditions and the following disclaimer.
// • Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
// 
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
//
//*@@@---@@@@******************************************************************
#include "prx/libc/include/Api.hpp"
#include "prx/libSceJxr/include/Api.hpp"
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include "prx/libSceJxr/Stream.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;

bool CopyIn(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}

bool CopyOut(void* destination, const void* source, std::size_t bytes) {
    const int savedErrno = errno;
    const bool copied = GuestCopyOut_nid_no_patch(destination, source, bytes);
    errno = savedErrno;
    return copied;
}
}

extern "C" {

std::int64_t APS5_VABI attach_SB_nid_postfix(Jxr::SimpleBitIo* io, Jxr::Stream* stream) {
    Jxr::SimpleBitIo local{};
    if (!CopyIn(&local, io, sizeof(local))) APS5_INVALID_ARG_EX;
    local.stream = stream;
    local.bytesRead = 0;
    local.accumulator = 0;
    local.bitsLeft = 0;
    if (!CopyOut(io, &local, sizeof(local))) APS5_INVALID_ARG_EX;
    return WmpErrSuccess;
}

std::uint32_t APS5_VABI getBit32_SB_nid_postfix(Jxr::SimpleBitIo* io, std::uint32_t bits) {
    Jxr::SimpleBitIo local{};
    if (!CopyIn(&local, io, sizeof(local)) || local.bitsLeft > 8) APS5_INVALID_ARG_EX;
    std::uint32_t result = 0;
    while (local.bitsLeft < bits) {
        result <<= local.bitsLeft;
        result |= static_cast<std::uint32_t>(local.accumulator) >> (8 - local.bitsLeft);
        bits -= local.bitsLeft;
        auto* stream = local.stream;
        decltype(Jxr::Stream::read) read = nullptr;
        if (!stream || !CopyIn(&read, &stream->read, sizeof(read)) || !read) APS5_INVALID_ARG_EX;
        auto* const accumulator = reinterpret_cast<std::uint8_t*>(io) + offsetof(Jxr::SimpleBitIo, accumulator);
        {
            GuestArena::HostWrite access(accumulator, sizeof(local.accumulator));
            if (!access.Open()) APS5_INVALID_ARG_EX;
            read(stream, accumulator, 1);
        }
        if (!CopyIn(&local.accumulator, accumulator, sizeof(local.accumulator))) APS5_INVALID_ARG_EX;
        ++local.bytesRead;
        local.bitsLeft = 8;
    }
    result <<= bits;
    result |= static_cast<std::uint32_t>(local.accumulator) >> (8 - bits);
    local.accumulator = static_cast<std::uint8_t>(local.accumulator << bits);
    local.bitsLeft -= bits;
    if (!CopyOut(io, &local, sizeof(local))) APS5_INVALID_ARG_EX;
    return result;
}

void APS5_VABI flushToByte_SB_nid_postfix(Jxr::SimpleBitIo* io) {
    Jxr::SimpleBitIo local{};
    if (!CopyIn(&local, io, sizeof(local))) APS5_INVALID_ARG_EX;
    local.accumulator = 0;
    local.bitsLeft = 0;
    if (!CopyOut(io, &local, sizeof(local))) APS5_INVALID_ARG_EX;
}

std::uint32_t APS5_VABI getByteRead_SB_nid_postfix(Jxr::SimpleBitIo* io) {
    Jxr::SimpleBitIo local{};
    if (!CopyIn(&local, io, sizeof(local))) APS5_INVALID_ARG_EX;
    return local.bytesRead;
}

std::int64_t APS5_VABI detach_SB_nid_postfix(Jxr::SimpleBitIo* io) {
    Jxr::SimpleBitIo local{};
    if (!CopyIn(&local, io, sizeof(local)) || local.bitsLeft != 0) APS5_INVALID_ARG_EX;
    local.stream = nullptr;
    if (!CopyOut(io, &local, sizeof(local))) APS5_INVALID_ARG_EX;
    return WmpErrSuccess;
}

}
