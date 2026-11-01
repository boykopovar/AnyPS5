#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"
#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using NKWebKitRequirements::GuestZStream;

namespace {

constexpr int Z_OK = 0;
constexpr int Z_STREAM_END = 1;
constexpr int Z_STREAM_ERROR = -2;
constexpr int Z_DATA_ERROR = -3;
constexpr int Z_BUF_ERROR = -5;
constexpr int Z_VERSION_ERROR = -6;
constexpr int Z_NO_FLUSH = 0;
constexpr int Z_FINISH = 4;
constexpr int Z_DEFLATED = 8;
constexpr int STREAM_SIZE = static_cast<int>(sizeof(GuestZStream));

void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "WebKit zlib assertion failed: %s\n", message);
    std::abort();
}

struct Allocations {
    int allocated;
    int freed;
};

void* APS5_VABI CountingAlloc(void* opaque, std::uint32_t items, std::uint32_t size) {
    ++static_cast<Allocations*>(opaque)->allocated;
    return std::calloc(items, size);
}

void APS5_VABI CountingFree(void* opaque, void* address) {
    ++static_cast<Allocations*>(opaque)->freed;
    std::free(address);
}

std::vector<std::uint8_t> Sample() {
    std::vector<std::uint8_t> data;
    for (int index = 0; index < 20000; ++index) data.push_back(static_cast<std::uint8_t>((index * 7 + index / 13) % 61 + 32));
    return data;
}

std::uint32_t Adler32(const std::vector<std::uint8_t>& data) {
    std::uint32_t low = 1;
    std::uint32_t high = 0;
    for (const auto value : data) {
        low = (low + value) % 65521;
        high = (high + low) % 65521;
    }
    return (high << 16) | low;
}

std::vector<std::uint8_t> Compress(GuestZStream& stream, const std::vector<std::uint8_t>& input, std::uint32_t chunk) {
    std::vector<std::uint8_t> output;
    std::vector<std::uint8_t> buffer(chunk);
    stream.next_in = input.data();
    stream.avail_in = static_cast<std::uint32_t>(input.size());
    int result = Z_OK;
    while (result == Z_OK) {
        stream.next_out = buffer.data();
        stream.avail_out = chunk;
        result = deflate_nid_postfix(&stream, Z_FINISH);
        Require(result == Z_OK || result == Z_STREAM_END, "deflate progresses");
        output.insert(output.end(), buffer.data(), buffer.data() + (chunk - stream.avail_out));
    }
    return output;
}

std::vector<std::uint8_t> Decompress(GuestZStream& stream, const std::vector<std::uint8_t>& input, std::uint32_t chunk) {
    std::vector<std::uint8_t> output;
    std::vector<std::uint8_t> buffer(chunk);
    std::size_t offset = 0;
    int result = Z_OK;
    while (result == Z_OK) {
        if (stream.avail_in == 0 && offset < input.size()) {
            stream.next_in = input.data() + offset;
            stream.avail_in = static_cast<std::uint32_t>(std::min<std::size_t>(chunk, input.size() - offset));
            offset += stream.avail_in;
        }
        stream.next_out = buffer.data();
        stream.avail_out = chunk;
        result = inflate_nid_postfix(&stream, Z_NO_FLUSH);
        Require(result == Z_OK || result == Z_STREAM_END, "inflate progresses");
        output.insert(output.end(), buffer.data(), buffer.data() + (chunk - stream.avail_out));
    }
    return output;
}

void RoundTrip(int windowBits, std::uint32_t chunk) {
    const auto data = Sample();
    Allocations counts{};
    GuestZStream deflater{};
    deflater.zalloc = CountingAlloc;
    deflater.zfree = CountingFree;
    deflater.opaque = &counts;
    Require(deflateInit2__nid_postfix(&deflater, 6, Z_DEFLATED, windowBits, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "deflate init");
    Require(deflater.state != nullptr && counts.allocated > 0, "deflate allocates through the guest allocator");
    Require(deflater.total_in == 0 && deflater.total_out == 0, "deflate init clears totals");
    const auto compressed = Compress(deflater, data, chunk);
    Require(deflater.total_in == data.size() && deflater.total_out == compressed.size(), "deflate totals");
    if (windowBits > 0 && windowBits < 16) Require(deflater.adler == Adler32(data), "deflate adler");
    Require(compressed.size() <= deflateBound_nid_postfix(nullptr, data.size()), "conservative bound holds");

    Require(deflateReset_nid_postfix(&deflater) == Z_OK, "deflate reset");
    Require(deflater.total_in == 0 && deflater.total_out == 0, "deflate reset clears totals");
    Require(Compress(deflater, data, chunk) == compressed, "deflate after reset is identical");
    Require(deflateEnd_nid_postfix(&deflater) == Z_OK, "deflate end");
    Require(deflater.state == nullptr, "deflate end clears state");
    Require(counts.allocated == counts.freed, "deflate frees everything it allocated");
    Require(deflateEnd_nid_postfix(&deflater) == Z_STREAM_ERROR, "second deflate end");

    GuestZStream inflater{};
    Require(inflateInit2__nid_postfix(&inflater, windowBits, "1.2.11", STREAM_SIZE) == Z_OK, "inflate init");
    Require(inflater.zalloc != nullptr && inflater.zfree != nullptr && inflater.opaque == nullptr, "inflate fills default allocators");
    Require(Decompress(inflater, compressed, chunk) == data, "inflate restores data");
    Require(inflater.total_in == compressed.size() && inflater.total_out == data.size(), "inflate totals");
    Require(inflateReset_nid_postfix(&inflater) == Z_OK, "inflate reset");
    Require(inflater.total_in == 0 && inflater.total_out == 0, "inflate reset clears totals");
    Require(Decompress(inflater, compressed, chunk) == data, "inflate after reset");
    Require(inflateEnd_nid_postfix(&inflater) == Z_OK && inflater.state == nullptr, "inflate end");
}

void Errors() {
    GuestZStream stream{};
    Require(deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 15, 8, 0, nullptr, STREAM_SIZE) == Z_VERSION_ERROR, "null version");
    Require(deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 15, 8, 0, "2.0", STREAM_SIZE) == Z_VERSION_ERROR, "major version");
    Require(inflateInit2__nid_postfix(&stream, 15, "1.3.1", STREAM_SIZE - 8) == Z_VERSION_ERROR, "stream size");
    Require(stream.zalloc == nullptr && stream.state == nullptr, "version errors leave the stream");
    Require(inflateInit2__nid_postfix(nullptr, 15, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "null inflate stream");
    Require(deflate_nid_postfix(nullptr, Z_FINISH) == Z_STREAM_ERROR, "null deflate stream");
    Require(inflate_nid_postfix(&stream, Z_FINISH) == Z_STREAM_ERROR, "uninitialized inflate");

    for (const int windowBits : {16, 31, 32, 47}) {
        bool threw = false;
        try { inflateInit2__nid_postfix(&stream, windowBits, "1.3.1", STREAM_SIZE); } catch (const std::runtime_error&) { threw = true; }
        Require(threw, "gzip inflate throws");
    }
    bool deflateThrew = false;
    try { deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 31, 8, 0, "1.3.1", STREAM_SIZE); } catch (const std::runtime_error&) { deflateThrew = true; }
    Require(deflateThrew, "gzip deflate throws");
    Require(stream.state == nullptr, "gzip rejection leaves the stream");
    Require(inflateInit2__nid_postfix(&stream, 20, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "invalid wrapped window");
    Require(deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 24, 8, 0, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "gzip window 8");
    stream = GuestZStream{};

    int sentinel = 0;
    stream.state = &sentinel;
    Require(deflateInit2__nid_postfix(&stream, 10, Z_DEFLATED, 15, 8, 0, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "bad level");
    Require(stream.state == &sentinel, "rejected deflate parameters leave state");
    Require(inflateInit2__nid_postfix(&stream, 7, "1.3.1", STREAM_SIZE) == Z_STREAM_ERROR, "bad window");
    Require(stream.state == nullptr, "rejected inflate window clears state");

    GuestZStream deflater{};
    Require(deflateInit2__nid_postfix(&deflater, 6, Z_DEFLATED, 15, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "deflate init");
    Require(deflate_nid_postfix(&deflater, 6) == Z_STREAM_ERROR, "bad flush");
    Require(inflate_nid_postfix(&deflater, Z_NO_FLUSH) == Z_STREAM_ERROR, "deflate stream passed to inflate");
    GuestZStream copy = deflater;
    Require(deflate_nid_postfix(&copy, Z_FINISH) == Z_STREAM_ERROR, "copied stream");
    Require(deflateBound_nid_postfix(&deflater, 1000) == 1013, "default parameter bound");
    Require(deflateBound_nid_postfix(nullptr, 1000) == 1151, "conservative bound");
    Require(deflateBound_nid_postfix(nullptr, ~0ULL) == ~0ULL, "saturated bound");
    auto saved = deflater.zfree;
    deflater.zfree = nullptr;
    Require(deflate_nid_postfix(&deflater, Z_FINISH) == Z_STREAM_ERROR, "missing free function");
    deflater.zfree = saved;
    Require(deflateEnd_nid_postfix(&deflater) == Z_OK, "deflate end");

    GuestZStream inflater{};
    Require(inflateInit2__nid_postfix(&inflater, 15, "1.3.1", STREAM_SIZE) == Z_OK, "inflate init");
    const std::uint8_t corrupt[] = {0x78, 0x9c, 0xff, 0xff};
    std::uint8_t out[64];
    inflater.next_in = corrupt;
    inflater.avail_in = sizeof(corrupt);
    inflater.next_out = out;
    inflater.avail_out = sizeof(out);
    Require(inflate_nid_postfix(&inflater, Z_NO_FLUSH) == Z_DATA_ERROR && inflater.msg != nullptr, "corrupt data");
    Require(inflateEnd_nid_postfix(&inflater) == Z_OK, "inflate end after error");
}

void Uncompress() {
    const auto data = Sample();
    GuestZStream deflater{};
    Require(deflateInit2__nid_postfix(&deflater, 9, Z_DEFLATED, 15, 8, 0, "1.3.1", STREAM_SIZE) == Z_OK, "deflate init");
    const auto compressed = Compress(deflater, data, 4096);
    Require(deflateEnd_nid_postfix(&deflater) == Z_OK, "deflate end");

    std::vector<std::uint8_t> output(data.size() + 16, 0xA5);
    std::uint64_t length = output.size();
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size()) == Z_OK, "uncompress");
    Require(length == data.size() && std::memcmp(output.data(), data.data(), data.size()) == 0, "uncompress data");

    length = 10;
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size()) == Z_BUF_ERROR, "small output");
    Require(length == 10, "small output length");

    length = 0;
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size()) == Z_DATA_ERROR, "zero output");
    Require(length == 0, "zero output length");

    const std::uint8_t empty[] = {0x78, 0x9c, 0x03, 0x00, 0x00, 0x00, 0x00, 0x01};
    Require(uncompress_nid_postfix(output.data(), &length, empty, sizeof(empty)) == Z_OK && length == 0, "empty stream");

    length = output.size();
    Require(uncompress_nid_postfix(output.data(), &length, compressed.data(), compressed.size() - 6) == Z_DATA_ERROR, "truncated");
    Require(length > 0 && length < data.size(), "truncated length");

    bool threw = false;
    try { uncompress_nid_postfix(output.data(), nullptr, compressed.data(), compressed.size()); } catch (const std::runtime_error&) { threw = true; }
    Require(threw, "null length throws");
}

}

int main() {
    for (const int windowBits : {15, -15, 9}) {
        for (const std::uint32_t chunk : {1u, 7u, 4096u}) RoundTrip(windowBits, chunk);
    }
    Errors();
    Uncompress();
    return 0;
}
