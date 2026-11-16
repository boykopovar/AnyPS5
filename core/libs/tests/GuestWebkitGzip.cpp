#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"
#include "zlib.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using GuestZlibStream = NKWebKitRequirements::GuestZStream;

void check(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

struct Allocations {
    unsigned calls = 0;
    unsigned live = 0;
    unsigned failAt = std::numeric_limits<unsigned>::max();
    bool reenter = false;
    bool entered = false;
    bool nestedPassed = false;
};

void* APS5_VABI allocate(void* opaque, std::uint32_t items, std::uint32_t bytes) {
    auto& counts = *static_cast<Allocations*>(opaque);
    if (counts.calls++ == counts.failAt) return nullptr;
    if (counts.reenter && !counts.entered) {
        counts.entered = true;
        GuestZlibStream nested{};
        const int result = inflateInit2__nid_postfix(&nested, 15, "1.2.11", sizeof(nested));
        counts.nestedPassed = result == Z_OK && inflateEnd_nid_postfix(&nested) == Z_OK;
    }
    void* result = std::calloc(items, bytes);
    if (result != nullptr) ++counts.live;
    return result;
}

void APS5_VABI release(void* opaque, void* address) {
    auto& counts = *static_cast<Allocations*>(opaque);
    if (address != nullptr) --counts.live;
    std::free(address);
}

std::vector<unsigned char> encode(GuestZlibStream& stream, const std::vector<unsigned char>& input) {
    std::vector<unsigned char> result;
    std::array<unsigned char, 13> output{};
    std::size_t assigned = 0;
    int status = Z_OK;
    for (unsigned calls = 0; status != Z_STREAM_END; ++calls) {
        check(calls < 100000, "compression made no bounded progress");
        if (stream.avail_in == 0 && assigned < input.size()) {
            const auto bytes = std::min<std::size_t>(17, input.size() - assigned);
            stream.next_in = input.data() + assigned;
            stream.avail_in = static_cast<std::uint32_t>(bytes);
            assigned += bytes;
        }
        stream.next_out = output.data();
        stream.avail_out = output.size();
        status = deflate_nid_postfix(&stream, assigned == input.size() ? Z_FINISH : Z_NO_FLUSH);
        check(status == Z_OK || status == Z_STREAM_END, "streaming compression failed");
        result.insert(result.end(), output.begin(), output.begin() + (output.size() - stream.avail_out));
    }
    check(stream.total_in == input.size() && stream.total_out == result.size(), "compression totals or partial buffers are wrong");
    return result;
}

std::vector<unsigned char> decode(GuestZlibStream& stream, const std::vector<unsigned char>& input) {
    std::vector<unsigned char> result;
    std::array<unsigned char, 11> output{};
    std::size_t assigned = 0;
    int status = Z_OK;
    for (unsigned calls = 0; status != Z_STREAM_END; ++calls) {
        check(calls < 100000, "decompression made no bounded progress");
        if (stream.avail_in == 0 && assigned < input.size()) {
            const auto bytes = std::min<std::size_t>(23, input.size() - assigned);
            stream.next_in = input.data() + assigned;
            stream.avail_in = static_cast<std::uint32_t>(bytes);
            assigned += bytes;
        }
        stream.next_out = output.data();
        stream.avail_out = output.size();
        status = inflate_nid_postfix(&stream, Z_NO_FLUSH);
        check(status == Z_OK || status == Z_STREAM_END, "streaming decompression failed");
        result.insert(result.end(), output.begin(), output.begin() + (output.size() - stream.avail_out));
    }
    check(stream.total_in == input.size() && stream.total_out == result.size(), "decompression totals or partial buffers are wrong");
    return result;
}

void formatsAndCallbacks() {
    for (const int window : {15, -15, 31}) {
        Allocations counts;
        counts.reenter = true;
        GuestZlibStream encoder{};
        encoder.zalloc = allocate;
        encoder.zfree = release;
        encoder.opaque = &counts;
        check(deflateInit2__nid_postfix(&encoder, 6, Z_DEFLATED, window, 8, Z_DEFAULT_STRATEGY, "1.2.11", sizeof(encoder)) == Z_OK, "compression initialization failed");
        check(counts.nestedPassed && encoder.opaque == &counts, "allocator callback ABI, opaque value or reentrancy failed");
        GuestZlibStream decoder{};
        const int initialized = inflateInit2__nid_postfix(&decoder, window < 0 ? window : 47, "1.3.2", sizeof(decoder));
        if (initialized != Z_OK) throw std::runtime_error("decompression initialization failed: window=" + std::to_string(window) + " result=" + std::to_string(initialized));
        check(decoder.zalloc != nullptr && decoder.zfree != nullptr, "default callbacks were not published");
        auto* zero = static_cast<unsigned char*>(decoder.zalloc(decoder.opaque, 4, 8));
        check(zero != nullptr && std::all_of(zero, zero + 32, [](unsigned char byte) { return byte == 0; }), "default callback does not allocate zeroed memory");
        decoder.zfree(decoder.opaque, zero);
        for (unsigned round = 0; round < 2; ++round) {
            std::vector<unsigned char> input(4096 + round * 127);
            for (std::size_t i = 0; i < input.size(); ++i) input[i] = static_cast<unsigned char>((i * 31 + round) % 251);
            if (round != 0) {
                check(deflateReset_nid_postfix(&encoder) == Z_OK && inflateReset_nid_postfix(&decoder) == Z_OK, "reset failed");
                check(encoder.total_in == 0 && encoder.total_out == 0 && decoder.total_in == 0 && decoder.total_out == 0, "reset did not clear 64-bit totals");
            }
            const auto bound = deflateBound_nid_postfix(&encoder, input.size());
            const auto compressed = encode(encoder, input);
            check(compressed.size() <= bound, "compressed stream exceeds its reported bound");
            check(decode(decoder, compressed) == input, "guest streaming round trip changed data");
            z_stream native{};
            check(::inflateInit2_(&native, window < 0 ? window : 47, ZLIB_VERSION, sizeof(native)) == Z_OK, "reference inflater initialization failed");
            std::vector<unsigned char> reference(input.size() + 1);
            native.next_in = const_cast<Bytef*>(compressed.data());
            native.avail_in = static_cast<unsigned>(compressed.size());
            native.next_out = reference.data();
            native.avail_out = static_cast<unsigned>(reference.size());
            check(::inflate(&native, Z_FINISH) == Z_STREAM_END && native.total_out == input.size(), "native zlib rejected guest-produced stream");
            reference.resize(native.total_out);
            check(reference == input && ::inflateEnd(&native) == Z_OK, "native reference output disagrees");
        }
        check(inflateEnd_nid_postfix(&decoder) == Z_OK && deflateEnd_nid_postfix(&encoder) == Z_OK, "end failed");
        check(counts.live == 0 && encoder.state == nullptr && decoder.state == nullptr, "stream allocations survived end");
        check(deflate_nid_postfix(&encoder, Z_FINISH) == Z_STREAM_ERROR && inflateReset_nid_postfix(&decoder) == Z_STREAM_ERROR, "ended stream remained usable");
    }
}

void failures() {
    GuestZlibStream stream{};
    stream.msg = "unchanged";
    std::array<unsigned char, sizeof(stream)> before{};
    std::memcpy(before.data(), &stream, sizeof(stream));
    check(deflateInit2__nid_postfix(&stream, 6, Z_DEFLATED, 15, 8, 0, "2.0", sizeof(stream)) == Z_VERSION_ERROR, "incompatible version accepted");
    check(std::memcmp(before.data(), &stream, sizeof(stream)) == 0, "version failure changed stream");
    check(inflateInit2__nid_postfix(&stream, 15, "1.2.11", sizeof(stream) - 1) == Z_VERSION_ERROR, "wrong guest structure size accepted");
    check(inflateInit2__nid_postfix(nullptr, 15, "1.2.11", sizeof(stream)) == Z_STREAM_ERROR, "null stream accepted");
    for (unsigned failAt = 0; failAt < 5; ++failAt) {
        Allocations counts;
        counts.failAt = failAt;
        GuestZlibStream failing{};
        failing.zalloc = allocate;
        failing.zfree = release;
        failing.opaque = &counts;
        check(deflateInit2__nid_postfix(&failing, 6, Z_DEFLATED, 15, 8, 0, "1.2.11", sizeof(failing)) == Z_MEM_ERROR, "allocator failure was hidden");
        check(counts.live == 0 && failing.state == nullptr, "partial initialization leaked allocations");
    }
    stream = {};
    check(inflateInit2__nid_postfix(&stream, 15, "1.2.11", sizeof(stream)) == Z_OK, "error-case initialization failed");
    const std::array<unsigned char, 4> malformed{255, 255, 255, 255};
    std::array<unsigned char, 32> output{};
    stream.next_in = malformed.data();
    stream.avail_in = malformed.size();
    stream.next_out = output.data();
    stream.avail_out = output.size();
    check(inflate_nid_postfix(&stream, 0) == Z_DATA_ERROR && stream.msg != nullptr, "corrupt stream was not rejected");
    check(deflateEnd_nid_postfix(&stream) == Z_STREAM_ERROR, "wrong stream kind accepted");
    check(inflateEnd_nid_postfix(&stream) == Z_OK, "error stream cleanup failed");
}

void headersAndErrors() {
    GuestZlibStream encoder{};
    check(deflateInit2__nid_postfix(&encoder, 6, Z_DEFLATED, 31, 8, 0, "1.3.2", sizeof(encoder)) == Z_OK, "gzip fixture initialization failed");
    const std::vector<unsigned char> input{'g', 'z', 'i', 'p', '-', 'a', 'p', 'i'};
    const auto compressed = encode(encoder, input);
    check(deflateEnd_nid_postfix(&encoder) == Z_OK, "gzip fixture cleanup failed");
    check(compressed.size() >= 18 && compressed[0] == 31 && compressed[1] == 139 && compressed[2] == 8, "gzip header missing");
    std::vector<unsigned char> optional(compressed.begin(), compressed.begin() + 10);
    optional[3] = 0x1e;
    optional.insert(optional.end(), {3, 0, 1, 2, 3, 'a', '.', 'b', 0, 'n', 'o', 't', 'e', 0});
    const auto headerChecksum = ::crc32(0, optional.data(), static_cast<unsigned>(optional.size()));
    optional.push_back(static_cast<unsigned char>(headerChecksum));
    optional.push_back(static_cast<unsigned char>(headerChecksum >> 8));
    const auto crcPosition = optional.size() - 2;
    optional.insert(optional.end(), compressed.begin() + 10, compressed.end());
    for (const int window : {31, 47}) {
        GuestZlibStream decoder{};
        check(inflateInit2__nid_postfix(&decoder, window, "1.3.2", sizeof(decoder)) == Z_OK, "gzip/autodetect init failed");
        check(decode(decoder, optional) == input, "gzip optional headers were not decoded");
        check(inflateReset_nid_postfix(&decoder) == Z_OK && decode(decoder, compressed) == input, "gzip reset failed");
        check(inflateEnd_nid_postfix(&decoder) == Z_OK, "gzip decoder cleanup failed");
    }
    for (const auto position : {crcPosition, optional.size() - 8, optional.size() - 4}) {
        auto corrupt = optional;
        corrupt[position] ^= 1;
        GuestZlibStream decoder{};
        check(inflateInit2__nid_postfix(&decoder, 47, "1.3.2", sizeof(decoder)) == Z_OK, "corrupt fixture init failed");
        std::array<unsigned char, 64> output{};
        decoder.next_in = corrupt.data();
        decoder.avail_in = static_cast<std::uint32_t>(corrupt.size());
        decoder.next_out = output.data();
        decoder.avail_out = output.size();
        check(inflate_nid_postfix(&decoder, Z_FINISH) == Z_DATA_ERROR && decoder.msg != nullptr, "gzip CRC/header/ISIZE corruption was accepted");
        check(inflateEnd_nid_postfix(&decoder) == Z_OK, "corrupt stream cleanup failed");
    }
    GuestZlibStream invalid{};
    check(inflateInit2__nid_postfix(&invalid, 47, "2.0", sizeof(invalid)) == Z_VERSION_ERROR, "gzip path bypassed version validation");
    check(deflateInit2__nid_postfix(&invalid, 99, Z_DEFLATED, 31, 8, 0, "1.3.2", sizeof(invalid)) == Z_STREAM_ERROR, "invalid gzip level was accepted");
    check(inflateInit2__nid_postfix(&invalid, 48, "1.3.2", sizeof(invalid)) == Z_STREAM_ERROR, "invalid autodetect window was accepted");
}

void largeTotals(int windowBits = 15) {
    GuestZlibStream stream{};
    check(deflateInit2__nid_postfix(&stream, 0, Z_DEFLATED, windowBits, 8, 0, "1.2.11", sizeof(stream)) == Z_OK, "large-stream initialization failed");
    std::vector<unsigned char> input(1 << 20, 0x5a);
    std::vector<unsigned char> output(input.size() + 4096);
    std::uint64_t produced = 0;
    std::array<unsigned char, 8> tail{};
    uLong checksum = ::crc32(0, nullptr, 0);
    const auto consume = [&] {
        const auto bytes = output.size() - stream.avail_out;
        produced += bytes;
        if (bytes >= tail.size()) std::copy(output.begin() + bytes - tail.size(), output.begin() + bytes, tail.begin());
        else if (bytes != 0) {
            std::move(tail.begin() + bytes, tail.end(), tail.begin());
            std::copy(output.begin(), output.begin() + bytes, tail.end() - bytes);
        }
    };
    for (unsigned chunk = 0; chunk < 4097; ++chunk) {
        if (windowBits == 31) checksum = ::crc32(checksum, input.data(), static_cast<unsigned>(input.size()));
        stream.next_in = input.data();
        stream.avail_in = static_cast<std::uint32_t>(input.size());
        do {
            stream.next_out = output.data();
            stream.avail_out = static_cast<std::uint32_t>(output.size());
            check(deflate_nid_postfix(&stream, Z_NO_FLUSH) == Z_OK, "large-stream step failed");
            consume();
        } while (stream.avail_in != 0);
    }
    int result = Z_OK;
    while (result != Z_STREAM_END) {
        stream.next_out = output.data();
        stream.avail_out = static_cast<std::uint32_t>(output.size());
        result = deflate_nid_postfix(&stream, Z_FINISH);
        check(result == Z_OK || result == Z_STREAM_END, "large-stream finish failed");
        consume();
    }
    check(stream.total_in == (std::uint64_t{4097} << 20) && stream.total_out == produced && produced > UINT32_MAX, "guest totals wrapped at 32 bits");
    if (windowBits == 31) {
        const auto word = [&](unsigned offset) {
            return std::uint32_t(tail[offset]) | (std::uint32_t(tail[offset + 1]) << 8) | (std::uint32_t(tail[offset + 2]) << 16) | (std::uint32_t(tail[offset + 3]) << 24);
        };
        check(word(0) == checksum && word(4) == static_cast<std::uint32_t>(stream.total_in), "large gzip trailer CRC or modulo32 ISIZE is wrong");
    }
    check(deflateReset_nid_postfix(&stream) == Z_OK && stream.total_in == 0 && stream.total_out == 0, "large totals survived reset");
    check(deflateEnd_nid_postfix(&stream) == Z_OK, "large-stream cleanup failed");
}

}

int main(int argc, char** argv) {
    try {
        if (argc == 2 && std::string(argv[1]) == "--large-totals") largeTotals();
        else if (argc == 2 && std::string(argv[1]) == "--large-gzip") largeTotals(31);
        else {
            check(argc == 1, "unexpected test argument");
            formatsAndCallbacks();
            failures();
            headersAndErrors();
        }
        std::cout << "NKWebKit zlib stream tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
