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
#include <cstdint>
#include "prx/libSceJxr/Stream.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "prx/libc/include/General.hpp"

namespace {
constexpr std::int64_t WmpErrSuccess = 0;
}

extern "C" {

std::int64_t APS5_VABI attach_SB_nid_postfix(Jxr::SimpleBitIo* io, Jxr::Stream* stream) {
    if (!io) APS5_INVALID_ARG_EX;
    io->stream = stream;
    io->bytesRead = 0;
    io->accumulator = 0;
    io->bitsLeft = 0;
    return WmpErrSuccess;
}

std::uint32_t APS5_VABI getBit32_SB_nid_postfix(Jxr::SimpleBitIo* io, std::uint32_t bits) {
    if (!io || io->bitsLeft > 8) APS5_INVALID_ARG_EX;
    std::uint32_t result = 0;
    while (io->bitsLeft < bits) {
        result <<= io->bitsLeft;
        result |= static_cast<std::uint32_t>(io->accumulator) >> (8 - io->bitsLeft);
        bits -= io->bitsLeft;
        auto* stream = io->stream;
        if (!stream || !stream->read) APS5_INVALID_ARG_EX;
        {
            GuestArena::HostWrite access(&io->accumulator, sizeof(io->accumulator));
            if (!access.Open()) APS5_INVALID_ARG_EX;
            stream->read(stream, &io->accumulator, 1);
        }
        ++io->bytesRead;
        io->bitsLeft = 8;
    }
    result <<= bits;
    result |= static_cast<std::uint32_t>(io->accumulator) >> (8 - bits);
    io->accumulator = static_cast<std::uint8_t>(io->accumulator << bits);
    io->bitsLeft -= bits;
    return result;
}

void APS5_VABI flushToByte_SB_nid_postfix(Jxr::SimpleBitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    io->accumulator = 0;
    io->bitsLeft = 0;
}

std::uint32_t APS5_VABI getByteRead_SB_nid_postfix(Jxr::SimpleBitIo* io) {
    if (!io) APS5_INVALID_ARG_EX;
    return io->bytesRead;
}

std::int64_t APS5_VABI detach_SB_nid_postfix(Jxr::SimpleBitIo* io) {
    if (!io || io->bitsLeft != 0) APS5_INVALID_ARG_EX;
    io->stream = nullptr;
    return WmpErrSuccess;
}

}
