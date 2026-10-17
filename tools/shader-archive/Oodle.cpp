#include "Oodle.hpp"
#include <bit>
#include <deque>
#include <limits>

namespace ShaderArchive {
namespace {
enum class Buf { Input, Output, Tmp, Scratch };
struct Ptr {
    Buf buf = Buf::Input;
    std::ptrdiff_t at = 0;
    Ptr operator+(std::ptrdiff_t n) const {
        return {buf, at + n};
    }
    Ptr operator-(std::ptrdiff_t n) const {
        return {buf, at - n};
    }
    Ptr &operator+=(std::ptrdiff_t n) {
        at += n;
        return *this;
    }
    Ptr &operator-=(std::ptrdiff_t n) {
        at -= n;
        return *this;
    }
    std::ptrdiff_t operator-(Ptr other) const {
        Check(buf == other.buf, "Oodle pointer buffer mismatch");
        return at - other.at;
    }
    bool operator==(Ptr other) const {
        return buf == other.buf && at == other.at;
    }
    bool operator<(Ptr other) const {
        Check(buf == other.buf, "Oodle pointer buffer mismatch");
        return at < other.at;
    }
    bool operator<=(Ptr other) const {
        return *this < other || *this == other;
    }
};
struct Core {
    const Bytes &input;
    Bytes &output;
    Bytes &tmp;
    Bytes &scratch;
    const Bytes &BufferOf(Buf buf) const {
        switch (buf) {
        case Buf::Input:
            return input;
        case Buf::Output:
            return output;
        case Buf::Tmp:
            return tmp;
        case Buf::Scratch:
            return scratch;
        }
        throw std::runtime_error("Oodle buffer");
    }
    Bytes &MutableBufferOf(Buf buf) {
        switch (buf) {
        case Buf::Output:
            return output;
        case Buf::Tmp:
            return tmp;
        case Buf::Scratch:
            return scratch;
        case Buf::Input:
            break;
        }
        throw std::runtime_error("Oodle read-only buffer");
    }
    std::size_t Size(Buf buf) const {
        return BufferOf(buf).size();
    }
    std::uint8_t ByteAt(Ptr p) const {
        const auto &b = BufferOf(p.buf);
        Check(p.at >= 0 && static_cast<std::size_t>(p.at) < b.size(), "Oodle read out of range");
        return b[static_cast<std::size_t>(p.at)];
    }
    void SetByte(Ptr p, std::uint8_t v) {
        auto &b = MutableBufferOf(p.buf);
        Check(p.at >= 0 && static_cast<std::size_t>(p.at) < b.size(), "Oodle write out of range");
        b[static_cast<std::size_t>(p.at)] = v;
    }
    std::uint64_t ReadInt(Ptr p, unsigned n, bool big) const {
        Check(n <= 8, "Oodle integer width");
        std::uint64_t v = 0;
        for (unsigned i = 0; i < n; ++i)
            v |= std::uint64_t(ByteAt(p + i)) << (8 * (big ? n - i - 1 : i));
        return v;
    }
    std::size_t ReadLE(Ptr p, unsigned n) const {
        return static_cast<std::size_t>(ReadInt(p, n, false));
    }
    std::size_t ReadBE(Ptr p, unsigned n) const {
        return static_cast<std::size_t>(ReadInt(p, n, true));
    }
    void RangeCheck(Ptr p, std::size_t n) const {
        const auto &b = BufferOf(p.buf);
        Check(p.at >= 0 && static_cast<std::size_t>(p.at) <= b.size() && n <= b.size() - static_cast<std::size_t>(p.at),
              "Oodle range out of bounds");
    }
    void CopyBytes(Ptr dst, Ptr src, std::size_t n) {
        if (!n)
            return;
        RangeCheck(src, n);
        RangeCheck(dst, n);
        if (dst.buf == src.buf) {
            auto &b = MutableBufferOf(dst.buf);
            std::memmove(b.data() + dst.at, b.data() + src.at, n);
        } else {
            std::memcpy(MutableBufferOf(dst.buf).data() + dst.at, BufferOf(src.buf).data() + src.at, n);
        }
    }
    void Memset(Ptr p, std::uint8_t v, std::size_t n) {
        if (!n)
            return;
        RangeCheck(p, n);
        std::memset(MutableBufferOf(p.buf).data() + p.at, v, n);
    }
    std::uint64_t Load64(Ptr p) const {
        return ReadInt(p, 8, false);
    }
    void Store64(Ptr p, std::uint64_t v) {
        for (unsigned i = 0; i < 8; ++i)
            SetByte(p + i, static_cast<std::uint8_t>(v >> (8 * i)));
    }
    void RepeatCopy64(Ptr dst, Ptr src, std::size_t n) {
        const auto dist = dst.at >= src.at ? std::size_t(dst.at - src.at) : std::size_t(src.at - dst.at);
        if (dst.buf != src.buf || n < dist) {
            CopyBytes(dst, src, n);
            return;
        }
        RangeCheck(dst, n);
        for (std::size_t done = 0; done < n; done += 8)
            Store64(dst + std::ptrdiff_t(done), Load64(src + std::ptrdiff_t(done)));
    }
    void Copy64Add(Ptr dst, Ptr lhs, Ptr rhs, std::size_t n) {
        if (!n)
            return;
        const auto dist = rhs.at >= dst.at ? std::size_t(rhs.at - dst.at) : std::size_t(dst.at - rhs.at);
        const std::size_t step = dist < 16 || n <= 8 ? 8 : 16;
        for (std::size_t base = 0; base < n; base += step) {
            std::array<std::uint8_t, 16> sum{};
            for (std::size_t i = 0; i < step; ++i)
                sum[i] = static_cast<std::uint8_t>(ByteAt(lhs + std::ptrdiff_t(base + i)) +
                                                   ByteAt(rhs + std::ptrdiff_t(base + i)));
            for (std::size_t i = 0; i < step; ++i)
                SetByte(dst + std::ptrdiff_t(base + i), sum[i]);
        }
    }
};
struct BitReader {
    Ptr p{};
    Ptr pEnd{};
    std::uint32_t bits = 0;
    int bitpos = 24;
    void Refill(const Core &core) {
        Check(bitpos <= 24, "Oodle bit reader state");
        while (bitpos > 0) {
            if (p < pEnd)
                bits |= std::uint32_t(core.ByteAt(p)) << bitpos;
            bitpos -= 8;
            p += 1;
        }
    }
    bool ReadBit(const Core &core) {
        Refill(core);
        return ReadBitNoRefill();
    }
    bool ReadBitNoRefill() {
        const bool r = (bits >> 31) != 0;
        bits <<= 1;
        bitpos += 1;
        return r;
    }
    std::int32_t ReadBitsNoRefill(int n) {
        Check(n >= 0 && n <= 32, "Oodle bit count");
        if (!n)
            return 0;
        const auto r = static_cast<std::int32_t>(n == 32 ? bits : bits >> (32 - n));
        bits = n == 32 ? 0 : bits << n;
        bitpos += n;
        return r;
    }
    std::int32_t ReadBitsNoRefillZero(int n) {
        Check(n >= 0 && n <= 31, "Oodle bit count");
        if (!n)
            return 0;
        const auto r = static_cast<std::int32_t>((bits >> 1) >> (31 - n));
        bits <<= n;
        bitpos += n;
        return r;
    }
    int LeadingZeros() const {
        return std::countl_zero(bits);
    }
    std::size_t ReadFluff(std::int32_t numSymbols) {
        if (numSymbols == 256)
            return 0;
        std::int32_t x = 257 - numSymbols;
        if (x > numSymbols)
            x = numSymbols;
        x *= 2;
        const int y = 31 - std::countl_zero(static_cast<std::uint32_t>(x - 1)) + 1;
        const auto v = bits >> (32 - y);
        const auto z = static_cast<std::uint32_t>((1 << y) - x);
        if ((v >> 1) >= z) {
            bits <<= y;
            bitpos += y;
            return v - z;
        }
        bits <<= y - 1;
        bitpos += y - 1;
        return v >> 1;
    }
};
struct BitReader2 {
    Ptr p{};
    Ptr pEnd{};
    std::uint32_t bitpos = 0;
};
void DecodeGolombRiceLengths(Core &core, std::uint8_t *dst, std::size_t count, BitReader2 &br) {
    Check(br.p < br.pEnd, "Oodle Golomb-Rice input");
    Ptr p = br.p;
    std::uint32_t bitInByte = br.bitpos;
    Check(bitInByte < 8, "Oodle Golomb-Rice bit position");
    std::size_t written = 0;
    std::uint32_t zeros = 0;
    while (written < count) {
        Check(p < br.pEnd, "Oodle Golomb-Rice input");
        const auto byte = core.ByteAt(p);
        for (std::uint32_t i = bitInByte; i < 8; ++i) {
            if (byte & (0x80u >> i)) {
                Check(zeros <= 0xff, "Oodle Golomb-Rice value");
                dst[written++] = static_cast<std::uint8_t>(zeros);
                zeros = 0;
                if (written == count) {
                    bitInByte = i + 1;
                    break;
                }
            } else {
                zeros += 1;
            }
        }
        if (written == count)
            break;
        p += 1;
        bitInByte = 0;
    }
    if (bitInByte == 8) {
        p += 1;
        bitInByte = 0;
    }
    br.p = p;
    br.bitpos = bitInByte;
}
void DecodeGolombRiceBits(Core &core, std::uint8_t *dst, std::size_t count, unsigned bitcount, BitReader2 &br) {
    if (!bitcount)
        return;
    Check(bitcount <= 3, "Oodle Golomb-Rice bit count");
    const std::size_t bitsRequired = br.bitpos + bitcount * count;
    const std::size_t bytesRequired = (bitsRequired + 7) >> 3;
    Check(bytesRequired < static_cast<std::size_t>(br.pEnd - br.p), "Oodle Golomb-Rice bits input");
    Ptr p = br.p;
    std::uint32_t bitInByte = br.bitpos;
    for (std::size_t k = 0; k < count; ++k) {
        std::uint32_t v = 0;
        for (unsigned i = 0; i < bitcount; ++i) {
            v = (v << 1) | ((core.ByteAt(p) >> (7 - bitInByte)) & 1u);
            bitInByte += 1;
            if (bitInByte == 8) {
                bitInByte = 0;
                p += 1;
            }
        }
        dst[k] = static_cast<std::uint8_t>((dst[k] << bitcount) + v);
    }
    br.p = br.p + std::ptrdiff_t(bitsRequired >> 3);
    br.bitpos = static_cast<std::uint32_t>(bitsRequired & 7);
}
struct HuffRange {
    std::uint16_t symbol = 0;
    std::uint16_t num = 0;
};
std::vector<HuffRange> ConvertToRanges(Core &core, std::int32_t numSymbols, std::size_t fluff,
                                       const std::uint8_t *syms, BitReader &bits) {
    std::int32_t symIdx = 0;
    std::size_t symLen = static_cast<std::size_t>(numSymbols);
    if (fluff & 1) {
        bits.Refill(core);
        const std::int32_t v = syms[symLen++];
        Check(v < 8, "Oodle Huffman range bits");
        symIdx = bits.ReadBitsNoRefill(v + 1) + (1 << (v + 1)) - 1;
    }
    std::int32_t symsUsed = 0;
    const std::size_t numRanges = fluff >> 1;
    std::vector<HuffRange> ranges;
    for (std::size_t i = 0; i < numRanges; ++i) {
        bits.Refill(core);
        const std::int32_t vNum = syms[symLen++];
        Check(vNum < 9, "Oodle Huffman range bits");
        const std::int32_t num = bits.ReadBitsNoRefillZero(vNum) + (1 << vNum);
        const std::int32_t vSpace = syms[symLen++];
        Check(vSpace < 8, "Oodle Huffman range bits");
        const std::int32_t space = bits.ReadBitsNoRefill(vSpace + 1) + (1 << (vSpace + 1)) - 1;
        ranges.push_back({static_cast<std::uint16_t>(symIdx), static_cast<std::uint16_t>(num)});
        symsUsed += num;
        symIdx += num + space;
    }
    Check(symIdx < 256 && symsUsed < numSymbols && symIdx + numSymbols - symsUsed <= 256, "Oodle Huffman ranges");
    ranges.push_back(
        {static_cast<std::uint16_t>(symIdx), static_cast<std::uint16_t>(numSymbols - symsUsed)});
    return ranges;
}
constexpr std::array<std::size_t, 12> kBasePrefix{0x0, 0x0, 0x2, 0x6, 0xE, 0x1E, 0x3E, 0x7E, 0xFE, 0x1FE, 0x2FE, 0x3FE};
std::int32_t HuffReadCodeLengthsOld(Core &core, BitReader &bits, std::uint8_t *syms,
                                    std::array<std::size_t, 12> &codePrefix) {
    if (bits.ReadBitNoRefill()) {
        std::int32_t sym = 0;
        std::int32_t numSymbols = 0;
        std::int32_t avgBitsX4 = 32;
        const std::int32_t forcedBits = bits.ReadBitsNoRefill(2);
        const auto thres = std::uint32_t(1) << (31 - (20 >> forcedBits));
        bool skipInitialZeros = bits.ReadBit(core);
        while (sym != 256) {
            if (skipInitialZeros) {
                skipInitialZeros = false;
            } else {
                Check(bits.bits & 0xff000000, "Oodle Huffman gamma");
                sym += bits.ReadBitsNoRefill(2 * (bits.LeadingZeros() + 1)) - 1;
                if (sym >= 256)
                    break;
            }
            bits.Refill(core);
            Check(bits.bits & 0xff000000, "Oodle Huffman gamma");
            std::int32_t n = bits.ReadBitsNoRefill(2 * (bits.LeadingZeros() + 1)) - 1;
            Check(sym + n <= 256, "Oodle Huffman symbol run");
            bits.Refill(core);
            numSymbols += n;
            while (true) {
                Check(bits.bits >= thres, "Oodle Huffman gamma value");
                const int lz = bits.LeadingZeros();
                const std::int32_t v = bits.ReadBitsNoRefill(lz + forcedBits + 1) + ((lz - 1) << forcedBits);
                const std::int32_t codeLen = (-(v & 1) ^ (v >> 1)) + ((avgBitsX4 + 2) >> 2);
                Check(codeLen >= 1 && codeLen <= 11, "Oodle Huffman code length");
                avgBitsX4 = codeLen + ((3 * avgBitsX4 + 2) >> 2);
                bits.Refill(core);
                syms[codePrefix[static_cast<std::size_t>(codeLen)]] = static_cast<std::uint8_t>(sym);
                codePrefix[static_cast<std::size_t>(codeLen)] += 1;
                sym += 1;
                n -= 1;
                if (!n)
                    break;
            }
        }
        Check(sym == 256 && numSymbols >= 2, "Oodle Huffman symbol count");
        return numSymbols;
    }
    const std::int32_t numSymbols = bits.ReadBitsNoRefill(8);
    Check(numSymbols != 0, "Oodle Huffman symbol count");
    if (numSymbols == 1) {
        syms[0] = static_cast<std::uint8_t>(bits.ReadBitsNoRefill(8));
    } else {
        const std::int32_t codeLenBits = bits.ReadBitsNoRefill(3);
        Check(codeLenBits <= 4, "Oodle Huffman code length bits");
        for (std::int32_t i = 0; i < numSymbols; ++i) {
            bits.Refill(core);
            const auto sym = static_cast<std::uint8_t>(bits.ReadBitsNoRefill(8));
            const std::int32_t codeLen = bits.ReadBitsNoRefillZero(codeLenBits) + 1;
            Check(codeLen <= 11, "Oodle Huffman code length");
            syms[codePrefix[static_cast<std::size_t>(codeLen)]] = sym;
            codePrefix[static_cast<std::size_t>(codeLen)] += 1;
        }
    }
    return numSymbols;
}
std::int32_t HuffReadCodeLengthsNew(Core &core, BitReader &bits, std::uint8_t *syms,
                                    std::array<std::size_t, 12> &codePrefix) {
    const std::int32_t forcedBits = bits.ReadBitsNoRefill(2);
    const std::int32_t numSymbols = bits.ReadBitsNoRefill(8) + 1;
    const std::size_t fluff = bits.ReadFluff(numSymbols);
    std::array<std::uint8_t, 512> codeLen{};
    BitReader2 br2;
    br2.bitpos = static_cast<std::uint32_t>((bits.bitpos - 24) & 7);
    br2.pEnd = bits.pEnd;
    br2.p = bits.p - ((24 - bits.bitpos + 7) >> 3);
    const std::size_t num = static_cast<std::size_t>(numSymbols);
    Check(num + fluff <= codeLen.size(), "Oodle Huffman lengths");
    DecodeGolombRiceLengths(core, codeLen.data(), num + fluff, br2);
    DecodeGolombRiceBits(core, codeLen.data(), num, static_cast<unsigned>(forcedBits), br2);
    bits.bitpos = 24;
    bits.p = br2.p;
    bits.bits = 0;
    bits.Refill(core);
    bits.bits <<= br2.bitpos;
    bits.bitpos += static_cast<int>(br2.bitpos);
    std::int32_t runningSum = 0x1e;
    for (std::size_t i = 0; i < num; ++i) {
        std::int32_t v = codeLen[i];
        v = (-(v & 1)) ^ (v >> 1);
        const std::int32_t len = v + (runningSum >> 2) + 1;
        Check(len >= 1 && len <= 11, "Oodle Huffman code length");
        codeLen[i] = static_cast<std::uint8_t>(len);
        runningSum += v;
    }
    const auto ranges = ConvertToRanges(core, numSymbols, fluff, codeLen.data(), bits);
    std::size_t cp = 0;
    for (const auto &range : ranges) {
        std::int32_t sym = range.symbol;
        for (std::uint16_t i = 0; i < range.num; ++i) {
            const auto len = codeLen[cp + i];
            syms[codePrefix[len]] = static_cast<std::uint8_t>(sym);
            codePrefix[len] += 1;
            sym += 1;
        }
        cp += range.num;
    }
    return numSymbols;
}
struct HuffLut {
    std::array<std::uint8_t, 2048> len{};
    std::array<std::uint8_t, 2048> sym{};
};
std::uint32_t Reverse11(std::uint32_t i) {
    std::uint32_t r = 0;
    for (int b = 0; b < 11; ++b)
        r |= ((i >> b) & 1u) << (10 - b);
    return r;
}
HuffLut MakeLut(const std::array<std::size_t, 12> &prefixCur, const std::uint8_t *syms) {
    std::array<std::uint8_t, 2048> lenMsb{};
    std::array<std::uint8_t, 2048> symMsb{};
    std::size_t slot = 0;
    for (std::uint8_t i = 1; i < 11; ++i) {
        const std::size_t start = kBasePrefix[i];
        const std::size_t count = prefixCur[i] - start;
        if (!count)
            continue;
        const std::size_t stepSize = 1u << (11 - i);
        const std::size_t numToSet = count << (11 - i);
        Check(slot + numToSet <= 2048, "Oodle Huffman table");
        std::fill_n(lenMsb.begin() + static_cast<std::ptrdiff_t>(slot), numToSet, i);
        for (std::size_t j = 0; j < count; ++j)
            std::fill_n(symMsb.begin() + static_cast<std::ptrdiff_t>(slot + stepSize * j), stepSize, syms[start + j]);
        slot += numToSet;
    }
    const std::size_t count11 = prefixCur[11] - kBasePrefix[11];
    if (count11) {
        Check(slot + count11 <= 2048, "Oodle Huffman table");
        std::fill_n(lenMsb.begin() + static_cast<std::ptrdiff_t>(slot), count11, std::uint8_t(11));
        std::copy_n(syms + kBasePrefix[11], count11, symMsb.begin() + static_cast<std::ptrdiff_t>(slot));
        slot += count11;
    }
    Check(slot == 2048, "Oodle Huffman table size");
    HuffLut lut;
    for (std::uint32_t i = 0; i < 2048; ++i) {
        lut.len[i] = lenMsb[Reverse11(i)];
        lut.sym[i] = symMsb[Reverse11(i)];
    }
    return lut;
}
void HuffDecode(Core &core, Ptr output, Ptr outputEnd, Ptr srcFirst, Ptr srcMidOrg, Ptr srcEnd, const HuffLut &lut) {
    Ptr src = srcFirst;
    Ptr srcMid = srcMidOrg;
    Ptr end = srcEnd;
    Ptr dst = output;
    std::uint32_t srcBits = 0;
    std::uint32_t midBits = 0;
    std::uint32_t endBits = 0;
    int srcBitpos = 0;
    int midBitpos = 0;
    int endBitpos = 0;
    Check(src <= srcMid, "Oodle Huffman streams");
    while (dst < outputEnd) {
        if (srcMid - src <= 1) {
            if (srcMid - src == 1)
                srcBits |= std::uint32_t(core.ByteAt(src)) << srcBitpos;
        } else {
            srcBits |= std::uint32_t(core.ReadLE(src, 2)) << srcBitpos;
        }
        std::size_t k = srcBits & 0x7ff;
        std::int32_t n = lut.len[k];
        Check(n != 0, "Oodle Huffman code");
        srcBitpos -= n;
        srcBits >>= n;
        core.SetByte(dst, lut.sym[k]);
        dst += 1;
        src += (7 - srcBitpos) >> 3;
        srcBitpos &= 7;
        if (dst < outputEnd) {
            if (end - srcMid <= 1) {
                if (end - srcMid == 1) {
                    const auto mid = std::uint32_t(core.ByteAt(srcMid));
                    endBits |= mid << endBitpos;
                    midBits |= mid << midBitpos;
                }
            } else {
                const auto v = static_cast<std::uint32_t>(core.ReadLE(end - 2, 2));
                endBits |= (((v >> 8) | (v << 8)) & 0xffff) << endBitpos;
                midBits |= std::uint32_t(core.ReadLE(srcMid, 2)) << midBitpos;
            }
            core.SetByte(dst, lut.sym[endBits & 0x7ff]);
            dst += 1;
            n = lut.len[endBits & 0x7ff];
            Check(n != 0, "Oodle Huffman code");
            endBitpos -= n;
            endBits >>= n;
            end -= (7 - endBitpos) >> 3;
            endBitpos &= 7;
            if (dst < outputEnd) {
                core.SetByte(dst, lut.sym[midBits & 0x7ff]);
                dst += 1;
                n = lut.len[midBits & 0x7ff];
                Check(n != 0, "Oodle Huffman code");
                midBitpos -= n;
                midBits >>= n;
                srcMid += (7 - midBitpos) >> 3;
                midBitpos &= 7;
            }
        }
        Check(src <= srcMid && srcMid <= end, "Oodle Huffman streams");
    }
    Check(src == srcMidOrg && end == srcMid, "Oodle Huffman stream ends");
}
std::size_t DecodeBytes(Core &core, Ptr &output, Ptr src, Ptr srcEnd, std::size_t &decodedSize,
                        std::size_t outputCapacity, bool forceMemmove, Ptr scratch);
std::size_t DecodeBytesType12(Core &core, Ptr src, std::size_t srcSize, Ptr output, std::size_t outputSize,
                              unsigned kind) {
    const Ptr srcEnd = src + static_cast<std::ptrdiff_t>(srcSize);
    BitReader bits;
    bits.p = src;
    bits.pEnd = srcEnd;
    bits.Refill(core);
    auto codePrefix = kBasePrefix;
    std::array<std::uint8_t, 1280> syms{};
    std::int32_t numSyms = 0;
    if (!bits.ReadBitNoRefill()) {
        numSyms = HuffReadCodeLengthsOld(core, bits, syms.data(), codePrefix);
    } else if (!bits.ReadBitNoRefill()) {
        numSyms = HuffReadCodeLengthsNew(core, bits, syms.data(), codePrefix);
    } else {
        throw std::runtime_error("Oodle Huffman header");
    }
    src = bits.p - ((24 - bits.bitpos) / 8);
    if (numSyms == 1) {
        core.Memset(output, syms[0], outputSize);
        return srcSize;
    }
    const HuffLut lut = MakeLut(codePrefix, syms.data());
    if (kind == 1) {
        Check(srcEnd - src >= 3, "Oodle Huffman split");
        const std::size_t splitMid = core.ReadLE(src, 2);
        src += 2;
        HuffDecode(core, output, output + static_cast<std::ptrdiff_t>(outputSize), src,
                   src + static_cast<std::ptrdiff_t>(splitMid), srcEnd, lut);
    } else {
        Check(srcEnd - src >= 6, "Oodle Huffman split");
        const std::size_t half = (outputSize + 1) >> 1;
        const std::size_t splitMid = core.ReadLE(src, 3);
        src += 3;
        Check(splitMid <= static_cast<std::size_t>(srcEnd - src), "Oodle Huffman split");
        const Ptr srcMidPos = src + static_cast<std::ptrdiff_t>(splitMid);
        const std::size_t splitLeft = core.ReadLE(src, 2);
        src += 2;
        Check(splitLeft + 2 <= static_cast<std::size_t>(srcEnd - src), "Oodle Huffman split");
        Check(srcEnd - srcMidPos >= 3, "Oodle Huffman split");
        const std::size_t splitRight = core.ReadLE(srcMidPos, 2);
        Check(splitRight + 2 <= static_cast<std::size_t>(srcEnd - (srcMidPos + 2)), "Oodle Huffman split");
        HuffDecode(core, output, output + static_cast<std::ptrdiff_t>(half), src,
                   src + static_cast<std::ptrdiff_t>(splitLeft), srcMidPos, lut);
        HuffDecode(core, output + static_cast<std::ptrdiff_t>(half),
                   output + static_cast<std::ptrdiff_t>(outputSize), srcMidPos + 2,
                   srcMidPos + 2 + static_cast<std::ptrdiff_t>(splitRight), srcEnd, lut);
    }
    return srcSize;
}
void DecodeRleUnpacked(Core &core, Ptr cmdPtr, Ptr cmdPtrEnd, Ptr dst, std::size_t dstSize) {
    const Ptr dstEnd = dst + static_cast<std::ptrdiff_t>(dstSize);
    std::uint8_t rleByte = 0;
    while (cmdPtr < cmdPtrEnd) {
        const std::size_t cmd = core.ByteAt(cmdPtrEnd - 1);
        if (cmd == 0 || cmd > 0x2f) {
            cmdPtrEnd -= 1;
            const std::size_t bytesToCopy = ~cmd & 0xf;
            const std::size_t bytesToRle = cmd >> 4;
            Check(bytesToCopy + bytesToRle <= static_cast<std::size_t>(dstEnd - dst), "Oodle RLE output");
            Check(bytesToCopy <= static_cast<std::size_t>(cmdPtrEnd - cmdPtr), "Oodle RLE input");
            core.CopyBytes(dst, cmdPtr, bytesToCopy);
            cmdPtr += static_cast<std::ptrdiff_t>(bytesToCopy);
            dst += static_cast<std::ptrdiff_t>(bytesToCopy);
            core.Memset(dst, rleByte, bytesToRle);
            dst += static_cast<std::ptrdiff_t>(bytesToRle);
        } else if (cmd >= 0x10) {
            cmdPtrEnd -= 2;
            const std::size_t data = core.ReadLE(cmdPtrEnd, 2) - 4096;
            const std::size_t bytesToCopy = data & 0x3f;
            const std::size_t bytesToRle = data >> 6;
            Check(bytesToCopy + bytesToRle <= static_cast<std::size_t>(dstEnd - dst), "Oodle RLE output");
            Check(bytesToCopy <= static_cast<std::size_t>(cmdPtrEnd - cmdPtr), "Oodle RLE input");
            core.CopyBytes(dst, cmdPtr, bytesToCopy);
            cmdPtr += static_cast<std::ptrdiff_t>(bytesToCopy);
            dst += static_cast<std::ptrdiff_t>(bytesToCopy);
            core.Memset(dst, rleByte, bytesToRle);
            dst += static_cast<std::ptrdiff_t>(bytesToRle);
        } else if (cmd == 1) {
            rleByte = core.ByteAt(cmdPtr);
            cmdPtr += 1;
            cmdPtrEnd -= 1;
        } else if (cmd >= 9) {
            cmdPtrEnd -= 2;
            const std::size_t bytesToRle = (core.ReadLE(cmdPtrEnd, 2) - 0x8ff) * 128;
            Check(bytesToRle <= static_cast<std::size_t>(dstEnd - dst), "Oodle RLE output");
            core.Memset(dst, rleByte, bytesToRle);
            dst += static_cast<std::ptrdiff_t>(bytesToRle);
        } else {
            cmdPtrEnd -= 2;
            const std::size_t bytesToCopy = (core.ReadLE(cmdPtrEnd, 2) - 511) * 64;
            Check(bytesToCopy <= static_cast<std::size_t>(cmdPtrEnd - cmdPtr), "Oodle RLE input");
            Check(bytesToCopy <= static_cast<std::size_t>(dstEnd - dst), "Oodle RLE output");
            core.CopyBytes(dst, cmdPtr, bytesToCopy);
            dst += static_cast<std::ptrdiff_t>(bytesToCopy);
            cmdPtr += static_cast<std::ptrdiff_t>(bytesToCopy);
        }
    }
    Check(cmdPtr == cmdPtrEnd && dst == dstEnd, "Oodle RLE stream ends");
}
std::size_t DecodeRle(Core &core, Ptr src, std::size_t srcSize, Ptr dst, std::size_t dstSize, Ptr scratch) {
    Check(srcSize != 0, "Oodle RLE input");
    if (srcSize == 1) {
        core.Memset(dst, core.ByteAt(src), dstSize);
        return 1;
    }
    if (core.ByteAt(src) != 0) {
        Ptr dstPtr = scratch;
        std::size_t decSize = 0;
        const std::size_t n =
            DecodeBytes(core, dstPtr, src, src + static_cast<std::ptrdiff_t>(srcSize), decSize,
                        std::numeric_limits<std::size_t>::max(), true, scratch);
        Check(n != 0 && n <= srcSize, "Oodle RLE command stream");
        const std::size_t cmdLen = srcSize - n + decSize;
        core.CopyBytes(dstPtr + static_cast<std::ptrdiff_t>(decSize), src + static_cast<std::ptrdiff_t>(n),
                       srcSize - n);
        DecodeRleUnpacked(core, dstPtr, dstPtr + static_cast<std::ptrdiff_t>(cmdLen), dst, dstSize);
    } else {
        DecodeRleUnpacked(core, src + 1, src + static_cast<std::ptrdiff_t>(srcSize), dst, dstSize);
    }
    return srcSize;
}
std::size_t DecodeBytes(Core &core, Ptr &output, Ptr src, Ptr srcEnd, std::size_t &decodedSize,
                        std::size_t outputCapacity, bool forceMemmove, Ptr scratch) {
    const Ptr srcOrg = src;
    Check(srcEnd - src >= 2, "Oodle entropy array header");
    const std::size_t chunkType = (core.ByteAt(src) >> 4) & 0x7;
    if (chunkType == 0) {
        std::size_t srcSize = 0;
        if (core.ByteAt(src) >= 0x80) {
            srcSize = ((std::size_t(core.ByteAt(src)) << 8) | core.ByteAt(src + 1)) & 0xfff;
            src += 2;
        } else {
            Check(srcEnd - src >= 3, "Oodle entropy array header");
            srcSize = core.ReadBE(src, 3);
            Check((srcSize & ~std::size_t(0x3ffff)) == 0, "Oodle entropy array reserved bits");
            src += 3;
        }
        Check(srcSize <= outputCapacity, "Oodle entropy array size");
        Check(srcSize <= static_cast<std::size_t>(srcEnd - src), "Oodle entropy array size");
        decodedSize = srcSize;
        if (forceMemmove || output.buf != src.buf)
            core.CopyBytes(output, src, srcSize);
        else
            output.at = src.at;
        return static_cast<std::size_t>(src + static_cast<std::ptrdiff_t>(srcSize) - srcOrg);
    }
    std::size_t srcSize = 0;
    std::size_t dstSize = 0;
    if (core.ByteAt(src) >= 0x80) {
        Check(srcEnd - src >= 3, "Oodle entropy array header");
        const std::size_t bits = core.ReadBE(src, 3);
        srcSize = bits & 0x3ff;
        dstSize = srcSize + ((bits >> 10) & 0x3ff) + 1;
        src += 3;
    } else {
        Check(srcEnd - src >= 5, "Oodle entropy array header");
        const std::size_t bits = core.ReadBE(src + 1, 4);
        srcSize = bits & 0x3ffff;
        dstSize = (((bits >> 18) | (std::size_t(core.ByteAt(src)) << 14)) & 0x3ffff) + 1;
        Check(srcSize < dstSize, "Oodle entropy array sizes");
        src += 5;
    }
    Check(srcSize <= static_cast<std::size_t>(srcEnd - src), "Oodle entropy array size");
    Check(dstSize <= outputCapacity, "Oodle entropy array size");
    const Ptr dst = output;
    Ptr innerScratch = scratch;
    if (dst.buf == Buf::Scratch)
        innerScratch += static_cast<std::ptrdiff_t>(dstSize);
    std::size_t srcUsed = 0;
    switch (chunkType) {
    case 2:
    case 4:
        srcUsed = DecodeBytesType12(core, src, srcSize, dst, dstSize, static_cast<unsigned>(chunkType >> 1));
        break;
    case 3:
        srcUsed = DecodeRle(core, src, srcSize, dst, dstSize, innerScratch);
        break;
    default:
        throw std::runtime_error("unsupported Oodle entropy array type " + std::to_string(chunkType));
    }
    Check(srcUsed == srcSize, "Oodle entropy array length");
    decodedSize = dstSize;
    return static_cast<std::size_t>(src + static_cast<std::ptrdiff_t>(srcSize) - srcOrg);
}
struct MermaidTable {
    Ptr cmdStream{};
    Ptr lengthStream{};
    Ptr litStream{};
    Ptr litStreamEnd{};
    std::deque<std::uint16_t> off16Stream;
    std::vector<std::uint32_t> off32Stream1;
    std::vector<std::uint32_t> off32Stream2;
    std::size_t cmdStream2Offs = 0;
    std::size_t cmdStream2OffsEnd = 0;
};
std::size_t DecodeFarOffsets(Core &core, std::vector<std::uint32_t> &out, Ptr src, Ptr srcEnd, std::size_t count,
                             std::size_t offset) {
    Ptr cur = src;
    for (std::size_t i = 0; i < count; ++i) {
        Check(srcEnd - cur >= 3, "Oodle far offset input");
        std::size_t off = core.ReadLE(cur, 3);
        cur += 3;
        if (offset >= 0xc00000 - 1 && off >= 0xc00000) {
            Check(cur < srcEnd, "Oodle far offset input");
            off += std::size_t(core.ByteAt(cur)) << 22;
            cur += 1;
        }
        Check(off <= offset, "Oodle far offset range");
        out.push_back(static_cast<std::uint32_t>(off));
    }
    return static_cast<std::size_t>(cur - src);
}
void ReadMermaidTable(Core &core, MermaidTable &lz, unsigned mode, Ptr src, Ptr srcEnd, Ptr dst, std::size_t dstSize,
                      std::size_t offset) {
    Check(mode <= 1, "Oodle Mermaid mode");
    Check(srcEnd - src >= 10, "Oodle Mermaid input");
    if (offset == 0) {
        core.CopyBytes(dst, src, 8);
        dst += 8;
        src += 8;
    }
    Ptr scratch{Buf::Tmp, 0};
    Ptr out = scratch;
    std::size_t decodeCount = 0;
    src += static_cast<std::ptrdiff_t>(
        DecodeBytes(core, out, src, srcEnd, decodeCount, dstSize, false, Ptr{Buf::Scratch, 0}));
    lz.litStream = out;
    lz.litStreamEnd = out + static_cast<std::ptrdiff_t>(decodeCount);
    scratch += static_cast<std::ptrdiff_t>(decodeCount);
    out = scratch;
    src += static_cast<std::ptrdiff_t>(
        DecodeBytes(core, out, src, srcEnd, decodeCount, dstSize, false, Ptr{Buf::Scratch, 0}));
    lz.cmdStream = out;
    scratch += static_cast<std::ptrdiff_t>(decodeCount);
    lz.cmdStream2OffsEnd = decodeCount;
    if (dstSize <= 0x10000) {
        lz.cmdStream2Offs = decodeCount;
    } else {
        Check(srcEnd - src >= 2, "Oodle Mermaid input");
        lz.cmdStream2Offs = core.ReadLE(src, 2);
        src += 2;
        Check(lz.cmdStream2Offs <= lz.cmdStream2OffsEnd, "Oodle Mermaid command split");
    }
    Check(srcEnd - src >= 2, "Oodle Mermaid input");
    const std::size_t off16Count = core.ReadLE(src, 2);
    src += 2;
    if (off16Count == 0xffff) {
        Ptr off16Hi = scratch;
        std::size_t hiCount = 0;
        src += static_cast<std::ptrdiff_t>(
            DecodeBytes(core, off16Hi, src, srcEnd, hiCount, dstSize >> 1, false, Ptr{Buf::Scratch, 0}));
        scratch += static_cast<std::ptrdiff_t>(hiCount);
        Ptr off16Lo = scratch;
        std::size_t loCount = 0;
        src += static_cast<std::ptrdiff_t>(
            DecodeBytes(core, off16Lo, src, srcEnd, loCount, dstSize >> 1, false, Ptr{Buf::Scratch, 0}));
        scratch += static_cast<std::ptrdiff_t>(loCount);
        Check(loCount == hiCount, "Oodle Mermaid near offsets");
        for (std::size_t i = 0; i < loCount; ++i)
            lz.off16Stream.push_back(static_cast<std::uint16_t>(
                core.ByteAt(off16Lo + static_cast<std::ptrdiff_t>(i)) +
                core.ByteAt(off16Hi + static_cast<std::ptrdiff_t>(i)) * 256));
    } else {
        Check(static_cast<std::size_t>(srcEnd - src) >= off16Count * 2, "Oodle Mermaid near offsets");
        for (std::size_t i = 0; i < off16Count; ++i)
            lz.off16Stream.push_back(
                static_cast<std::uint16_t>(core.ReadLE(src + static_cast<std::ptrdiff_t>(i * 2), 2)));
        src += static_cast<std::ptrdiff_t>(off16Count * 2);
    }
    Check(srcEnd - src >= 3, "Oodle Mermaid input");
    const std::size_t tmp = core.ReadLE(src, 3);
    src += 3;
    if (tmp != 0) {
        std::size_t off32Size1 = tmp >> 12;
        std::size_t off32Size2 = tmp & 0xfff;
        if (off32Size1 == 4095) {
            Check(srcEnd - src >= 2, "Oodle Mermaid input");
            off32Size1 = core.ReadLE(src, 2);
            src += 2;
        }
        if (off32Size2 == 4095) {
            Check(srcEnd - src >= 2, "Oodle Mermaid input");
            off32Size2 = core.ReadLE(src, 2);
            src += 2;
        }
        src += static_cast<std::ptrdiff_t>(DecodeFarOffsets(core, lz.off32Stream1, src, srcEnd, off32Size1, offset));
        src += static_cast<std::ptrdiff_t>(
            DecodeFarOffsets(core, lz.off32Stream2, src, srcEnd, off32Size2, offset + 0x10000));
    }
    lz.lengthStream = src;
}
std::uint16_t PopOff16(MermaidTable &lz) {
    Check(!lz.off16Stream.empty(), "Oodle Mermaid near offset stream");
    const auto v = lz.off16Stream.front();
    lz.off16Stream.pop_front();
    return v;
}
void ProcessMermaidChunk(Core &core, MermaidTable &lz, bool addMode, const std::vector<std::uint32_t> &off32,
                         Ptr cmdBegin, Ptr cmdEnd, Ptr dstBase, std::size_t dstSize, Ptr srcEnd,
                         std::int32_t &savedDist, std::int32_t startoff) {
    Ptr dst = dstBase;
    const Ptr dstEnd = dst + static_cast<std::ptrdiff_t>(dstSize);
    Ptr cmdStream = cmdBegin;
    std::size_t off32At = 0;
    std::int32_t recentOffs = savedDist;
    dst += startoff;
    while (cmdStream < cmdEnd) {
        const std::size_t cmd = core.ByteAt(cmdStream);
        cmdStream += 1;
        if (cmd >= 24) {
            const std::size_t litlen = cmd & 7;
            if (addMode)
                core.Copy64Add(dst, lz.litStream, dst + recentOffs, litlen);
            else
                core.RepeatCopy64(dst, lz.litStream, litlen);
            dst += static_cast<std::ptrdiff_t>(litlen);
            lz.litStream += static_cast<std::ptrdiff_t>(litlen);
            if ((cmd >> 7) == 0)
                recentOffs = -static_cast<std::int32_t>(PopOff16(lz));
            const std::size_t matchLen = (cmd >> 3) & 0xf;
            core.RepeatCopy64(dst, dst + recentOffs, matchLen);
            dst += static_cast<std::ptrdiff_t>(matchLen);
        } else if (cmd > 2) {
            const std::size_t length = cmd + 5;
            Check(off32At < off32.size(), "Oodle Mermaid far offset stream");
            const Ptr offsPtr = dstBase - static_cast<std::ptrdiff_t>(off32[off32At]);
            off32At += 1;
            recentOffs = static_cast<std::int32_t>(offsPtr.at - dst.at);
            Check(static_cast<std::size_t>(dstEnd - dst) >= length, "Oodle Mermaid match length");
            core.RepeatCopy64(dst, offsPtr, length);
            dst += static_cast<std::ptrdiff_t>(length);
        } else if (cmd == 0) {
            Check(lz.lengthStream < srcEnd, "Oodle Mermaid length stream");
            std::size_t length = core.ByteAt(lz.lengthStream);
            if (length > 251) {
                Check(srcEnd - lz.lengthStream >= 3, "Oodle Mermaid length stream");
                length += core.ReadLE(lz.lengthStream + 1, 2) * 4;
                lz.lengthStream += 2;
            }
            lz.lengthStream += 1;
            length += 64;
            Check(static_cast<std::size_t>(dstEnd - dst) >= length, "Oodle Mermaid literal length");
            Check(static_cast<std::size_t>(lz.litStreamEnd - lz.litStream) >= length,
                  "Oodle Mermaid literal stream");
            if (addMode)
                core.Copy64Add(dst, lz.litStream, dst + recentOffs, length);
            else
                core.RepeatCopy64(dst, lz.litStream, length);
            dst += static_cast<std::ptrdiff_t>(length);
            lz.litStream += static_cast<std::ptrdiff_t>(length);
        } else if (cmd == 1) {
            Check(lz.lengthStream < srcEnd, "Oodle Mermaid length stream");
            std::size_t length = core.ByteAt(lz.lengthStream);
            if (length > 251) {
                Check(srcEnd - lz.lengthStream >= 3, "Oodle Mermaid length stream");
                length += core.ReadLE(lz.lengthStream + 1, 2) * 4;
                lz.lengthStream += 2;
            }
            lz.lengthStream += 1;
            length += 91;
            const Ptr offsPtr = dst - static_cast<std::ptrdiff_t>(PopOff16(lz));
            recentOffs = static_cast<std::int32_t>(offsPtr.at - dst.at);
            core.RepeatCopy64(dst, offsPtr, length);
            dst += static_cast<std::ptrdiff_t>(length);
        } else {
            Check(lz.lengthStream < srcEnd, "Oodle Mermaid length stream");
            std::size_t length = core.ByteAt(lz.lengthStream);
            if (length > 251) {
                Check(srcEnd - lz.lengthStream >= 3, "Oodle Mermaid length stream");
                length += core.ReadLE(lz.lengthStream + 1, 2) * 4;
                lz.lengthStream += 2;
            }
            lz.lengthStream += 1;
            length += 29;
            Check(off32At < off32.size(), "Oodle Mermaid far offset stream");
            const Ptr offsPtr = dstBase - static_cast<std::ptrdiff_t>(off32[off32At]);
            off32At += 1;
            recentOffs = static_cast<std::int32_t>(offsPtr.at - dst.at);
            core.RepeatCopy64(dst, offsPtr, length);
            dst += static_cast<std::ptrdiff_t>(length);
        }
    }
    const std::size_t tail = static_cast<std::size_t>(dstEnd - dst);
    if (addMode)
        core.Copy64Add(dst, lz.litStream, dst + recentOffs, tail);
    else
        core.RepeatCopy64(dst, lz.litStream, tail);
    lz.litStream += static_cast<std::ptrdiff_t>(tail);
    savedDist = recentOffs;
}
void ProcessMermaid(Core &core, unsigned mode, Ptr src, std::size_t srcUsed, Ptr dstStart, Ptr dst,
                    std::size_t dstSize) {
    const std::size_t offset = static_cast<std::size_t>(dst - dstStart);
    MermaidTable lz;
    ReadMermaidTable(core, lz, mode, src, src + static_cast<std::ptrdiff_t>(srcUsed), dst, dstSize, offset);
    const Ptr srcEnd = src + static_cast<std::ptrdiff_t>(srcUsed);
    std::int32_t savedDist = -8;
    Ptr dstCur = dst;
    std::size_t remaining = dstSize;
    for (int iteration = 0; iteration < 2; ++iteration) {
        std::size_t chunkSize = remaining;
        if (chunkSize > 0x10000)
            chunkSize = 0x10000;
        Ptr cmdBegin = lz.cmdStream;
        Ptr cmdEnd{};
        const std::vector<std::uint32_t> *off32 = nullptr;
        if (iteration == 0) {
            off32 = &lz.off32Stream1;
            cmdEnd = lz.cmdStream + static_cast<std::ptrdiff_t>(lz.cmdStream2Offs);
        } else {
            off32 = &lz.off32Stream2;
            cmdBegin = lz.cmdStream + static_cast<std::ptrdiff_t>(lz.cmdStream2Offs);
            cmdEnd = lz.cmdStream + static_cast<std::ptrdiff_t>(lz.cmdStream2OffsEnd);
        }
        const std::int32_t startoff = offset == 0 && iteration == 0 ? 8 : 0;
        ProcessMermaidChunk(core, lz, mode == 0, *off32, cmdBegin, cmdEnd, dstCur, chunkSize, srcEnd, savedDist,
                            startoff);
        dstCur += static_cast<std::ptrdiff_t>(chunkSize);
        remaining -= chunkSize;
        if (!remaining)
            break;
    }
    Check(lz.lengthStream == srcEnd, "Oodle Mermaid length stream end");
}
void DecodeQuantum(Core &core, Ptr src, Ptr srcEnd, Ptr dstStart, Ptr dst, Ptr dstEnd) {
    Ptr cur = src;
    Ptr out = dst;
    while (out < dstEnd) {
        const std::size_t dstCount =
            std::min(static_cast<std::size_t>(dstEnd - out), std::size_t(0x20000));
        Check(srcEnd - cur >= 4, "Oodle chunk header");
        const std::size_t chunkHdr = core.ReadBE(cur, 3);
        std::size_t srcUsed = 0;
        if ((chunkHdr & 0x800000) == 0) {
            Ptr entropyOut = out;
            std::size_t written = 0;
            srcUsed = DecodeBytes(core, entropyOut, cur, srcEnd, written, dstCount, false, Ptr{Buf::Scratch, 0});
            Check(written == dstCount, "Oodle entropy chunk size");
        } else {
            cur += 3;
            srcUsed = chunkHdr & 0x7ffff;
            const unsigned mode = static_cast<unsigned>((chunkHdr >> 19) & 0xf);
            Check(srcUsed <= static_cast<std::size_t>(srcEnd - cur), "Oodle chunk size");
            if (srcUsed < dstCount) {
                ProcessMermaid(core, mode, cur, srcUsed, dstStart, out, dstCount);
            } else if (srcUsed > dstCount || mode != 0) {
                throw std::runtime_error("Oodle chunk header");
            } else {
                core.CopyBytes(out, cur, dstCount);
            }
        }
        cur += static_cast<std::ptrdiff_t>(srcUsed);
        out += static_cast<std::ptrdiff_t>(dstCount);
    }
    Check(cur == srcEnd, "Oodle quantum size");
}
} // namespace
Bytes DecompressOodle(View input, std::size_t size) {
    Check(size > 0 && size <= 512ull * 1024 * 1024, "invalid decompressed size");
    if (input.size() == size)
        return {input.begin(), input.end()};
    Check(!input.empty(), "empty compressed input");
    Bytes padded(input.size() + 64, 0);
    std::memcpy(padded.data(), input.data(), input.size());
    Bytes output(size + 64, 0);
    Bytes tmp(0x70000 + 64, 0);
    Bytes scratch(0x90000 + 64, 0);
    Core core{padded, output, tmp, scratch};
    Ptr in{Buf::Input, 0};
    const Ptr inEnd{Buf::Input, static_cast<std::ptrdiff_t>(input.size())};
    const Ptr outStart{Buf::Output, 0};
    unsigned type = 0;
    bool stored = false;
    for (std::size_t at = 0; at < size;) {
        if ((at & 0x3ffff) == 0) {
            Check(inEnd - in >= 2, "truncated Oodle block header");
            const auto h0 = core.ByteAt(in);
            const auto h1 = core.ByteAt(in + 1);
            in += 2;
            Check((h0 & 63) == 12, "invalid Oodle block header");
            type = h1 & 15;
            stored = (h0 & 64) != 0;
            Check(type == 10, "unsupported Oodle decoder " + std::to_string(type));
            Check((h1 & 128) == 0, "Oodle checksum mode is not supported");
        }
        const auto n = std::min(size - at, std::size_t(0x40000));
        std::size_t compressed = n;
        if (!stored) {
            Check(inEnd - in >= 3, "truncated Oodle quantum header");
            const std::size_t h = core.ReadBE(in, 3);
            in += 3;
            if ((h & 0x3ffff) == 0x3ffff) {
                Check((h >> 18) == 1, "unsupported Oodle quantum header");
                Check(inEnd - in >= 1, "truncated Oodle quantum");
                core.Memset(outStart + static_cast<std::ptrdiff_t>(at), core.ByteAt(in), n);
                in += 1;
                at += n;
                continue;
            }
            compressed = (h & 0x3ffff) + 1;
        }
        Check(compressed <= n, "invalid Oodle quantum length");
        Check(static_cast<std::size_t>(inEnd - in) >= compressed, "truncated Oodle quantum");
        const Ptr data = in;
        in += static_cast<std::ptrdiff_t>(compressed);
        if (compressed == n)
            core.CopyBytes(outStart + static_cast<std::ptrdiff_t>(at), data, n);
        else
            DecodeQuantum(core, data, data + static_cast<std::ptrdiff_t>(compressed), outStart,
                          outStart + static_cast<std::ptrdiff_t>(at),
                          outStart + static_cast<std::ptrdiff_t>(at + n));
        at += n;
    }
    Check(in == inEnd, "trailing compressed bytes");
    output.resize(size);
    return output;
}
} // namespace ShaderArchive
