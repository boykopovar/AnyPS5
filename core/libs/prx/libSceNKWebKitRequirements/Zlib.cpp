/* uncompress_nid_postfix is adapted from uncompress2() in uncompr.c of zlib 1.3.1
 * (https://github.com/madler/zlib/blob/51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf/uncompr.c).
 * Altered for AnyPS5: it takes 64-bit guest lengths, counts the produced bytes itself
 * instead of reading total_out, and does not report the consumed source length.
 */

/* uncompr.c -- decompress a memory buffer
 * Copyright (C) 1995-2003, 2010, 2014, 2016 Jean-loup Gailly, Mark Adler
 * For conditions of distribution and use, see copyright notice in zlib.h
 */

/* zlib.h -- interface of the 'zlib' general purpose compression library
  version 1.3.1, January 22nd, 2024

  Copyright (C) 1995-2024 Jean-loup Gailly and Mark Adler

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.

  Jean-loup Gailly        Mark Adler
  jloup@gzip.org          madler@alumni.caltech.edu
*/

#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include "zlib.h"
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <new>
#include <stdexcept>

namespace {

using NKWebKitRequirements::GuestZStream;

static_assert(sizeof(z_size_t) >= sizeof(std::uint64_t));

constexpr int GUEST_STREAM_SIZE = static_cast<int>(sizeof(GuestZStream));
constexpr uInt MAX_CHUNK = std::numeric_limits<uInt>::max();

struct HostStream {
    GuestZStream* guest;
    bool inflating;
    NKWebKitRequirements::GuestAlloc alloc;
    NKWebKitRequirements::GuestFree free;
    void* opaque;
    z_stream stream;
};

void* APS5_VABI DefaultAlloc_nid_no_patch(void*, std::uint32_t items, std::uint32_t size) {
    return std::calloc(items, size);
}

void APS5_VABI DefaultFree_nid_no_patch(void*, void* address) {
    std::free(address);
}

voidpf HostAlloc(voidpf opaque, uInt items, uInt size) {
    auto* host = static_cast<HostStream*>(opaque);
    return host->alloc(host->opaque, items, size);
}

void HostFree(voidpf opaque, voidpf address) {
    auto* host = static_cast<HostStream*>(opaque);
    host->free(host->opaque, address);
}

void Load(HostStream& host) {
    const GuestZStream& guest = *host.guest;
    z_stream& stream = host.stream;
    stream.next_in = const_cast<Bytef*>(guest.next_in);
    stream.avail_in = guest.avail_in;
    stream.total_in = static_cast<uLong>(guest.total_in);
    stream.next_out = guest.next_out;
    stream.avail_out = guest.avail_out;
    stream.total_out = static_cast<uLong>(guest.total_out);
    stream.msg = const_cast<char*>(guest.msg);
    stream.data_type = guest.data_type;
    stream.adler = static_cast<uLong>(guest.adler);
    host.alloc = guest.zalloc;
    host.free = guest.zfree;
    host.opaque = guest.opaque;
}

void Store(HostStream& host, bool totalsReset) {
    GuestZStream& guest = *host.guest;
    const z_stream& stream = host.stream;
    guest.next_in = stream.next_in;
    guest.avail_in = stream.avail_in;
    guest.next_out = stream.next_out;
    guest.avail_out = stream.avail_out;
    if (totalsReset) {
        guest.total_in = stream.total_in;
        guest.total_out = stream.total_out;
    } else {
        guest.total_in += static_cast<uLong>(stream.total_in - static_cast<uLong>(guest.total_in));
        guest.total_out += static_cast<uLong>(stream.total_out - static_cast<uLong>(guest.total_out));
    }
    guest.msg = stream.msg;
    guest.data_type = stream.data_type;
    if (guest.adler != static_cast<std::uint64_t>(stream.adler)) guest.adler = stream.adler;
}

HostStream* Find(GuestZStream* guest, bool inflating) {
    if (!guest || !guest->zalloc || !guest->zfree || !guest->state) return nullptr;
    auto* host = static_cast<HostStream*>(guest->state);
    if (host->guest != guest || host->inflating != inflating) return nullptr;
    return host;
}

template<typename TInit>
int Initialize(GuestZStream* guest, const char* version, int streamSize, bool inflating, TInit init) {
    if (!version || version[0] != ZLIB_VERSION[0] || streamSize != GUEST_STREAM_SIZE) return Z_VERSION_ERROR;
    if (!guest) return Z_STREAM_ERROR;
    guest->msg = nullptr;
    if (!guest->zalloc) {
        guest->zalloc = DefaultAlloc_nid_no_patch;
        guest->opaque = nullptr;
    }
    if (!guest->zfree) guest->zfree = DefaultFree_nid_no_patch;
    auto* host = new (std::nothrow) HostStream{};
    if (!host) return Z_MEM_ERROR;
    host->guest = guest;
    host->inflating = inflating;
    Load(*host);
    void* const previous = guest->state;
    host->stream.zalloc = HostAlloc;
    host->stream.zfree = HostFree;
    host->stream.opaque = host;
    host->stream.state = static_cast<internal_state*>(previous);
    const int result = init(host->stream);
    Store(*host, result == Z_OK);
    void* const current = host->stream.state;
    if (result == Z_OK) {
        guest->state = host;
        return result;
    }
    if (current != previous && current) throw std::runtime_error("zlib: failed initialization kept a stream state");
    if (!current) guest->state = nullptr;
    delete host;
    return result;
}

template<typename TCall>
int Process(GuestZStream* guest, bool inflating, bool resets, TCall call) {
    HostStream* host = Find(guest, inflating);
    if (!host) return Z_STREAM_ERROR;
    Load(*host);
    const int result = call(host->stream);
    Store(*host, resets && result == Z_OK);
    return result;
}

template<typename TEnd>
int End(GuestZStream* guest, bool inflating, TEnd end) {
    HostStream* host = Find(guest, inflating);
    if (!host) return Z_STREAM_ERROR;
    Load(*host);
    const int result = end(host->stream);
    Store(*host, false);
    if (!host->stream.state) {
        guest->state = nullptr;
        delete host;
    }
    return result;
}

} // namespace

extern "C" {

int APS5_VABI deflateInit2__nid_postfix(GuestZStream* stream, int level, int method, int windowBits, int memLevel, int strategy, const char* version, int streamSize) {
    if (windowBits >= 25 && windowBits <= 31) throw std::runtime_error("deflateInit2_: gzip wrapper is not implemented");
    return Initialize(stream, version, streamSize, false, [&](z_stream& host) {
        return deflateInit2(&host, level, method, windowBits, memLevel, strategy);
    });
}

int APS5_VABI deflate_nid_postfix(GuestZStream* stream, int flush) {
    return Process(stream, false, false, [flush](z_stream& host) { return deflate(&host, flush); });
}

int APS5_VABI deflateReset_nid_postfix(GuestZStream* stream) {
    return Process(stream, false, true, [](z_stream& host) { return deflateReset(&host); });
}

int APS5_VABI deflateEnd_nid_postfix(GuestZStream* stream) {
    return End(stream, false, [](z_stream& host) { return deflateEnd(&host); });
}

std::uint64_t APS5_VABI deflateBound_nid_postfix(GuestZStream* stream, std::uint64_t sourceLength) {
    HostStream* host = Find(stream, false);
    if (!host) return deflateBound_z(Z_NULL, sourceLength);
    Load(*host);
    return deflateBound_z(&host->stream, sourceLength);
}

int APS5_VABI inflateInit2__nid_postfix(GuestZStream* stream, int windowBits, const char* version, int streamSize) {
    if (windowBits >= 16 && windowBits < 48 && ((windowBits & 15) == 0 || (windowBits & 15) >= 8))
        throw std::runtime_error("inflateInit2_: gzip decoding is not implemented");
    return Initialize(stream, version, streamSize, true, [windowBits](z_stream& host) {
        return inflateInit2(&host, windowBits);
    });
}

int APS5_VABI inflate_nid_postfix(GuestZStream* stream, int flush) {
    return Process(stream, true, false, [flush](z_stream& host) { return inflate(&host, flush); });
}

int APS5_VABI inflateReset_nid_postfix(GuestZStream* stream) {
    return Process(stream, true, true, [](z_stream& host) { return inflateReset(&host); });
}

int APS5_VABI inflateEnd_nid_postfix(GuestZStream* stream) {
    return End(stream, true, [](z_stream& host) { return inflateEnd(&host); });
}

int APS5_VABI uncompress_nid_postfix(std::uint8_t* destination, std::uint64_t* destinationLength, const std::uint8_t* source, std::uint64_t sourceLength) {
    if (!destinationLength) throw std::runtime_error("uncompress: null destination length");
    std::uint8_t probe = 0;
    std::uint64_t left = *destinationLength;
    std::uint64_t remaining = sourceLength;
    std::uint8_t* output = destination;
    if (left) *destinationLength = 0;
    else {
        left = 1;
        output = &probe;
    }
    z_stream stream{};
    stream.next_in = const_cast<Bytef*>(source);
    int result = inflateInit(&stream);
    if (result != Z_OK) return result;
    stream.next_out = output;
    std::uint64_t produced = 0;
    do {
        if (stream.avail_out == 0) {
            stream.avail_out = left > MAX_CHUNK ? MAX_CHUNK : static_cast<uInt>(left);
            left -= stream.avail_out;
        }
        if (stream.avail_in == 0) {
            stream.avail_in = remaining > MAX_CHUNK ? MAX_CHUNK : static_cast<uInt>(remaining);
            remaining -= stream.avail_in;
        }
        const uInt space = stream.avail_out;
        result = inflate(&stream, Z_NO_FLUSH);
        produced += space - stream.avail_out;
    } while (result == Z_OK);
    if (output != &probe) *destinationLength = produced;
    else if (produced && result == Z_BUF_ERROR) left = 1;
    const std::uint64_t unused = left + stream.avail_out;
    inflateEnd(&stream);
    if (result == Z_STREAM_END) return Z_OK;
    if (result == Z_NEED_DICT) return Z_DATA_ERROR;
    if (result == Z_BUF_ERROR && unused) return Z_DATA_ERROR;
    return result;
}

}
