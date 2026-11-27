#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJsc/libSceJsc.hpp"
#include "prx/libSceJsc/src/WtfStringAbi.hpp"

#include <algorithm>
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

using Jsc::WtfAbi::CStringBuffer;
using Jsc::WtfAbi::CStringBuffer;
using Jsc::WtfAbi::OptionalCString;
using Jsc::WtfAbi::OptionalSHA1Digest;
using Jsc::WtfAbi::OwnedCString;
using Jsc::WtfAbi::OptionalString;
using Jsc::WtfAbi::PersistenceAllocator;
using Jsc::WtfAbi::PersistenceDecoder;
using Jsc::WtfAbi::PersistenceEncoder;
using Jsc::WtfAbi::SHA1;
using Jsc::WtfAbi::SHA1Digest;
using Jsc::WtfAbi::StringImpl;

static_assert(std::is_same_v<decltype(&_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE),
                             OwnedCString (APS5_VABI*)(const SHA1Digest*)>);

namespace {

void Check(bool value, int line) {
    if (!value) {
        std::fprintf(stderr, "WTF SHA1 check failed at line %d\n", line);
        std::abort();
    }
}

constexpr std::array<std::uint32_t, 5> InitialHash{0x67452301U, 0xEFCDAB89U, 0x98BADCFEU, 0x10325476U, 0xC3D2E1F0U};

struct Case {
    std::array<std::uint32_t, 5> hash;
    std::vector<std::uint8_t> pending;
    std::uint64_t totalBytes;
    const char* digest;
};

std::vector<std::uint8_t> Bytes(const std::string& text) {
    return {text.begin(), text.end()};
}

std::vector<std::uint8_t> Sequence(std::size_t begin, std::size_t end, std::size_t multiplier, std::size_t offset) {
    std::vector<std::uint8_t> bytes;
    for (std::size_t i = begin; i < end; ++i) bytes.push_back(static_cast<std::uint8_t>(i * multiplier + offset));
    return bytes;
}

SHA1 Make(const Case& value) {
    SHA1 sha1;
    std::memset(&sha1, 0xA5, sizeof(sha1));
    if (!value.pending.empty()) std::memcpy(sha1.buffer, value.pending.data(), value.pending.size());
    sha1.cursor = value.pending.size();
    sha1.totalBytes = value.totalBytes;
    for (std::size_t i = 0; i < 5; ++i) sha1.hash[i] = value.hash[i];
    return sha1;
}

std::string Hex(const SHA1Digest& digest) {
    static constexpr char Digits[] = "0123456789ABCDEF";
    std::string text;
    for (auto byte : digest) {
        text += Digits[byte >> 4];
        text += Digits[byte & 15];
    }
    return text;
}

bool IsReset(const SHA1& sha1) {
    for (auto byte : sha1.buffer) if (byte) return false;
    for (std::size_t i = 0; i < 5; ++i) if (sha1.hash[i] != InitialHash[i]) return false;
    return sha1.cursor == 0 && sha1.totalBytes == 0;
}

template<typename ExceptionType, typename Function> bool ThrowsException(Function function) {
    try {
        function();
    } catch (const ExceptionType&) {
        return true;
    }
    return false;
}

template<typename Function> bool ThrowsInvalidArgument(Function function) {
    return ThrowsException<std::invalid_argument>(function);
}

void ResetEncoder(PersistenceEncoder& encoder, const SHA1& sha1, unsigned size = 0) {
    std::memset(&encoder, 0xA5, sizeof(encoder));
    encoder.buffer = encoder.inlineBuffer;
    encoder.capacity = sizeof(encoder.inlineBuffer);
    encoder.size = size;
    encoder.sha1 = sha1;
}

struct TestAllocatorState {
    std::size_t allocateCalls = 0;
    std::size_t deallocateCalls = 0;
    std::size_t lastAllocateSize = 0;
    void* lastFreed = nullptr;
    bool failNext = false;
    std::vector<void*> liveAllocations;
};

TestAllocatorState* activeAllocatorState = nullptr;

void* TestAllocate(std::size_t size) {
    ++activeAllocatorState->allocateCalls;
    activeAllocatorState->lastAllocateSize = size;
    if (activeAllocatorState->failNext) return nullptr;
    void* ptr = std::malloc(size);
    if (ptr) activeAllocatorState->liveAllocations.push_back(ptr);
    return ptr;
}

void TestDeallocate(void* ptr) {
    ++activeAllocatorState->deallocateCalls;
    activeAllocatorState->lastFreed = ptr;
    auto& live = activeAllocatorState->liveAllocations;
    const auto it = std::find(live.begin(), live.end(), ptr);
    Check(it != live.end(), __LINE__);
    live.erase(it);
    std::free(ptr);
}

std::vector<std::uint8_t> Stream(std::initializer_list<std::vector<std::uint8_t>> parts) {
    std::vector<std::uint8_t> bytes;
    for (const auto& part : parts) bytes.insert(bytes.end(), part.begin(), part.end());
    return bytes;
}

std::vector<std::uint8_t> Word(std::uint32_t value) {
    return {static_cast<std::uint8_t>(value), static_cast<std::uint8_t>(value >> 8), static_cast<std::uint8_t>(value >> 16),
        static_cast<std::uint8_t>(value >> 24)};
}

PersistenceDecoder Decoder(const std::vector<std::uint8_t>& bytes) {
    PersistenceDecoder decoder;
    std::memset(&decoder, 0xA5, sizeof(decoder));
    decoder.buffer = bytes.data();
    decoder.bufferPosition = bytes.data();
    decoder.bufferEnd = bytes.data() + bytes.size();
    decoder.sha1 = Make({InitialHash, {}, 0, ""});
    return decoder;
}

bool Hashed(const PersistenceDecoder& decoder, const std::vector<std::uint8_t>& bytes) {
    return decoder.sha1.cursor == bytes.size() && decoder.sha1.totalBytes == bytes.size() &&
        (bytes.empty() || std::memcmp(decoder.sha1.buffer, bytes.data(), bytes.size()) == 0);
}

OptionalCString DecodeCString(PersistenceDecoder& decoder) {
    OptionalCString result;
    std::memset(&result, 0x5A, sizeof(result));
    Check(_ZN3WTF11Persistence5CoderINS_7CStringEE6decodeERNS0_7DecoderE(&result, &decoder) == &result, __LINE__);
    return result;
}

OptionalString DecodeString(PersistenceDecoder& decoder) {
    OptionalString result;
    std::memset(&result, 0x5A, sizeof(result));
    Check(_ZN3WTF11Persistence5CoderINS_6StringEE6decodeERNS0_7DecoderE(&result, &decoder) == &result, __LINE__);
    return result;
}

void DecodeCStrings() {
    const auto abc = Stream({Word(3), Bytes("abcx")});
    {
        auto decoder = Decoder(abc);
        Check(ThrowsException<std::runtime_error>([&] { DecodeCString(decoder); }), __LINE__);
    }

    TestAllocatorState state;
    activeAllocatorState = &state;
    const PersistenceAllocator allocator{&TestAllocate, &TestDeallocate};
    RegisterPersistenceAllocator_nid_no_patch(&allocator);

    {
        auto decoder = Decoder(abc);
        const auto result = DecodeCString(decoder);
        Check(result.hasValue && result.value.buffer, __LINE__);
        Check(state.allocateCalls == 1 && state.lastAllocateSize == sizeof(CStringBuffer) + 4, __LINE__);
        Check(result.value.buffer->refCount == 1 && result.value.buffer->length == 3, __LINE__);
        Check(std::memcmp(result.value.buffer + 1, "abc", 4) == 0, __LINE__);
        Check(decoder.bufferPosition == abc.data() + 7, __LINE__);
        Check(Hashed(decoder, Stream({Word(11), Word(3), Word(101), Bytes("abc")})), __LINE__);
        TestDeallocate(result.value.buffer);
    }
    {
        const auto bytes = Word(0);
        auto decoder = Decoder(bytes);
        const auto result = DecodeCString(decoder);
        Check(result.hasValue && result.value.buffer && result.value.buffer->length == 0, __LINE__);
        Check(state.lastAllocateSize == sizeof(CStringBuffer) + 1, __LINE__);
        Check(*reinterpret_cast<const char*>(result.value.buffer + 1) == 0, __LINE__);
        Check(Hashed(decoder, Stream({Word(11), Word(0), Word(101)})), __LINE__);
        TestDeallocate(result.value.buffer);
    }
    {
        const auto bytes = Stream({Word(0xFFFFFFFFU), Bytes("a")});
        auto decoder = Decoder(bytes);
        const std::size_t calls = state.allocateCalls;
        const auto result = DecodeCString(decoder);
        Check(result.hasValue && !result.value.buffer && state.allocateCalls == calls, __LINE__);
        Check(decoder.bufferPosition == bytes.data() + 4 && Hashed(decoder, Stream({Word(11), Word(0xFFFFFFFFU)})), __LINE__);
    }
    {
        const std::vector<std::uint8_t> bytes{3, 0, 0};
        auto decoder = Decoder(bytes);
        const auto result = DecodeCString(decoder);
        Check(!result.hasValue && decoder.bufferPosition == bytes.data() && Hashed(decoder, {}), __LINE__);
    }
    {
        const auto bytes = Stream({Word(5), Bytes("ab")});
        auto decoder = Decoder(bytes);
        const std::size_t calls = state.allocateCalls;
        const auto result = DecodeCString(decoder);
        Check(!result.hasValue && state.allocateCalls == calls, __LINE__);
        Check(decoder.bufferPosition == bytes.data() + 4 && Hashed(decoder, Stream({Word(11), Word(5)})), __LINE__);
    }
    {
        auto decoder = Decoder(abc);
        state.failNext = true;
        Check(ThrowsException<std::bad_alloc>([&] { DecodeCString(decoder); }), __LINE__);
        state.failNext = false;
    }
    {
        auto decoder = Decoder(abc);
        OptionalCString result{};
        Check(ThrowsInvalidArgument([&] { _ZN3WTF11Persistence5CoderINS_7CStringEE6decodeERNS0_7DecoderE(nullptr, &decoder); }), __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF11Persistence5CoderINS_7CStringEE6decodeERNS0_7DecoderE(&result, nullptr); }), __LINE__);
        decoder.sha1.cursor = 64;
        Check(ThrowsInvalidArgument([&] { DecodeCString(decoder); }), __LINE__);
        decoder = Decoder(abc);
        decoder.bufferPosition = decoder.bufferEnd + 1;
        Check(ThrowsInvalidArgument([&] { DecodeCString(decoder); }), __LINE__);
    }

    Check(state.liveAllocations.empty(), __LINE__);
    RegisterPersistenceAllocator_nid_no_patch(nullptr);
    activeAllocatorState = nullptr;
}

void DecodeStrings() {
    const auto latin1 = Stream({Word(2), {1}, Bytes("hix")});
    {
        auto decoder = Decoder(latin1);
        Check(ThrowsException<std::runtime_error>([&] { DecodeString(decoder); }), __LINE__);
    }

    TestAllocatorState state;
    activeAllocatorState = &state;
    const PersistenceAllocator allocator{&TestAllocate, &TestDeallocate};
    RegisterPersistenceAllocator_nid_no_patch(&allocator);
    constexpr std::size_t charactersOffset = 20;

    {
        auto decoder = Decoder(latin1);
        const auto result = DecodeString(decoder);
        Check(result.hasValue && result.value, __LINE__);
        Check(state.allocateCalls == 1 && state.lastAllocateSize == std::max<std::size_t>(sizeof(StringImpl), charactersOffset + 2), __LINE__);
        const StringImpl& string = *result.value;
        Check(string.refCount == 2 && string.length == 2 && string.hashAndFlags == Jsc::WtfAbi::StringImplFlag8Bit, __LINE__);
        Check(string.data == reinterpret_cast<const std::uint8_t*>(result.value) + charactersOffset, __LINE__);
        Check(std::memcmp(string.data, "hi", 2) == 0, __LINE__);
        Check(decoder.bufferPosition == latin1.data() + 7, __LINE__);
        Check(Hashed(decoder, Stream({Word(11), Word(2), Word(3), {1}, Word(101), Bytes("hi")})), __LINE__);
        TestDeallocate(result.value);
    }
    {
        const auto bytes = Stream({Word(2), {0}, {0x41, 0x00, 0x3A, 0x26}});
        auto decoder = Decoder(bytes);
        const auto result = DecodeString(decoder);
        Check(result.hasValue && result.value, __LINE__);
        Check(state.lastAllocateSize == charactersOffset + 4, __LINE__);
        const StringImpl& string = *result.value;
        Check(string.refCount == 2 && string.length == 2 && string.hashAndFlags == 0, __LINE__);
        const char16_t expected[] = {u'A', static_cast<char16_t>(0x263A)};
        Check(std::memcmp(string.data, expected, sizeof(expected)) == 0, __LINE__);
        Check(decoder.bufferPosition == bytes.data() + 9, __LINE__);
        Check(Hashed(decoder, Stream({Word(11), Word(2), Word(3), {0}, Word(101), {0x41, 0x00, 0x3A, 0x26}})), __LINE__);
        TestDeallocate(result.value);
    }
    {
        const auto bytes = Stream({Word(0xFFFFFFFFU), {1}});
        auto decoder = Decoder(bytes);
        const std::size_t calls = state.allocateCalls;
        const auto result = DecodeString(decoder);
        Check(result.hasValue && !result.value && state.allocateCalls == calls, __LINE__);
        Check(decoder.bufferPosition == bytes.data() + 4 && Hashed(decoder, Stream({Word(11), Word(0xFFFFFFFFU)})), __LINE__);
    }
    {
        const auto bytes = Word(2);
        auto decoder = Decoder(bytes);
        const auto result = DecodeString(decoder);
        Check(!result.hasValue && decoder.bufferPosition == bytes.data() + 4, __LINE__);
        Check(Hashed(decoder, Stream({Word(11), Word(2)})), __LINE__);
    }
    {
        const auto bytes = Stream({Word(2), {0}, Bytes("abc")});
        auto decoder = Decoder(bytes);
        const std::size_t calls = state.allocateCalls;
        const auto result = DecodeString(decoder);
        Check(!result.hasValue && state.allocateCalls == calls, __LINE__);
        Check(decoder.bufferPosition == bytes.data() + 5 && Hashed(decoder, Stream({Word(11), Word(2), Word(3), {0}})), __LINE__);
    }
    {
        const std::vector<std::uint8_t> bytes{2, 0};
        auto decoder = Decoder(bytes);
        const auto result = DecodeString(decoder);
        Check(!result.hasValue && decoder.bufferPosition == bytes.data() && Hashed(decoder, {}), __LINE__);
    }
    {
        const auto bytes = Stream({Word(1), {2}, Bytes("a")});
        auto decoder = Decoder(bytes);
        Check(ThrowsInvalidArgument([&] { DecodeString(decoder); }), __LINE__);
    }
    {
        const auto bytes = Stream({Word(0), {1}});
        auto decoder = Decoder(bytes);
        const std::size_t calls = state.allocateCalls;
        Check(ThrowsException<std::runtime_error>([&] { DecodeString(decoder); }), __LINE__);
        Check(state.allocateCalls == calls, __LINE__);
    }
    {
        auto decoder = Decoder(latin1);
        state.failNext = true;
        Check(ThrowsException<std::bad_alloc>([&] { DecodeString(decoder); }), __LINE__);
        state.failNext = false;
    }
    {
        auto decoder = Decoder(latin1);
        OptionalString result{};
        Check(ThrowsInvalidArgument([&] { _ZN3WTF11Persistence5CoderINS_6StringEE6decodeERNS0_7DecoderE(nullptr, &decoder); }), __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF11Persistence5CoderINS_6StringEE6decodeERNS0_7DecoderE(&result, nullptr); }), __LINE__);
        decoder.sha1.cursor = 64;
        Check(ThrowsInvalidArgument([&] { DecodeString(decoder); }), __LINE__);
    }

    Check(state.liveAllocations.empty(), __LINE__);
    RegisterPersistenceAllocator_nid_no_patch(nullptr);
    activeAllocatorState = nullptr;
}

}

int main() {
    const std::array<Case, 8> cases{{
        {InitialHash, {}, 0, "DA39A3EE5E6B4B0D3255BFEF95601890AFD80709"},
        {InitialHash, Bytes("abc"), 3, "A9993E364706816ABA3E25717850C26C9CD0D89D"},
        {InitialHash, Bytes(std::string(55, 'a')), 55, "C1C8BBDC22796E28C0E15163D20899B65621D65A"},
        {InitialHash, Bytes("abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq"), 56, "84983E441C3BD26EBAAE4AA1F95129E5E54670F1"},
        {InitialHash, Sequence(0, 63, 1, 0), 63, "6D942DA0C4392B123528F2905C713A3CE28364BD"},
        {{0x10802D0AU, 0x3AE914A9U, 0x2CF9155BU, 0xFF4CFF7DU, 0x7A598402U}, Sequence(960, 1000, 7, 3), 1000, "4231A8A50A10FA9758DB8EC71FDEF855B751048A"},
        {{0xA048FAD6U, 0x1AFDE325U, 0xFDF0CDD0U, 0x4E6302CFU, 0x6A2344DDU}, {}, 1000000, "34AA973CD4C4DAA4F61EEB2BDBAD27316534016F"},
        {InitialHash, {}, 0x2000000000000001ULL, "72DC4AAEBF1CE74F1159D176D15CE840E50EC0B5"},
    }};
    for (const auto& value : cases) {
        auto sha1 = Make(value);
        SHA1Digest digest;
        digest.fill(0xA5);
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &digest);
        Check(Hex(digest) == value.digest, __LINE__);
        Check(IsReset(sha1), __LINE__);
    }

    {
        auto sha1 = Make(cases[1]);
        SHA1Digest first;
        SHA1Digest second;
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &first);
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &second);
        Check(Hex(first) == cases[1].digest, __LINE__);
        Check(Hex(second) == cases[0].digest, __LINE__);
    }

    {
        auto sha1 = Make(cases[0]);
        sha1.cursor = 64;
        SHA1Digest digest;
        digest.fill(0xA5);
        const auto before = sha1;
        Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &digest); }), __LINE__);
        Check(std::memcmp(&before, &sha1, sizeof(sha1)) == 0, __LINE__);
        for (auto byte : digest) Check(byte == 0xA5, __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(nullptr, &digest); }), __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, nullptr); }), __LINE__);
    }

    const auto encoded = Sequence(0, 25, 7, 3);
    {
        PersistenceDecoder decoder{encoded.data(), encoded.data() + 2, encoded.data() + encoded.size(), Make(cases[0])};
        const OptionalSHA1Digest result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
        Check(result.hasValue, __LINE__);
        Check(std::memcmp(result.value.data(), encoded.data() + 2, 20) == 0, __LINE__);
        Check(decoder.buffer == encoded.data() && decoder.bufferPosition == encoded.data() + 22, __LINE__);
        Check(decoder.sha1.cursor == 24 && decoder.sha1.totalBytes == 24, __LINE__);
        const std::uint8_t salt[4] = {101, 0, 0, 0};
        Check(std::memcmp(decoder.sha1.buffer, salt, 4) == 0, __LINE__);
        SHA1Digest checksum;
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&decoder.sha1, &checksum);
        Check(Hex(checksum) == "6FED4A05DD4D2D6D8271FAE31449335D648D60A9", __LINE__);
    }

    {
        Case pending{InitialHash, Sequence(0, 50, 1, 0), 50, nullptr};
        PersistenceDecoder decoder{encoded.data(), encoded.data() + 2, encoded.data() + 22, Make(pending)};
        const OptionalSHA1Digest result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
        Check(result.hasValue && decoder.bufferPosition == decoder.bufferEnd, __LINE__);
        Check(decoder.sha1.cursor == 10 && decoder.sha1.totalBytes == 74, __LINE__);
        SHA1Digest checksum;
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&decoder.sha1, &checksum);
        Check(Hex(checksum) == "32A66B256525A031D02A2F31DCC62AB1099703EF", __LINE__);
    }

    {
        PersistenceDecoder decoder{encoded.data(), encoded.data() + 6, encoded.data() + encoded.size(), Make(cases[1])};
        const auto before = decoder;
        const OptionalSHA1Digest result = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
        Check(!result.hasValue, __LINE__);
        for (auto byte : result.value) Check(byte == 0, __LINE__);
        Check(std::memcmp(&before, &decoder, sizeof(decoder)) == 0, __LINE__);
    }

    {
        PersistenceDecoder decoder{encoded.data(), encoded.data(), encoded.data() + encoded.size(), Make(cases[0])};
        decoder.sha1.cursor = 64;
        const auto before = decoder;
        Check(ThrowsInvalidArgument([&] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder); }), __LINE__);
        Check(std::memcmp(&before, &decoder, sizeof(decoder)) == 0, __LINE__);
        decoder.sha1.cursor = 0;
        decoder.bufferPosition = decoder.bufferEnd + 1;
        Check(ThrowsInvalidArgument([&] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder); }), __LINE__);
        Check(ThrowsInvalidArgument([&] { _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(nullptr); }), __LINE__);
    }

    SHA1Digest digest;
    std::memcpy(digest.data(), encoded.data() + 2, digest.size());

    {
        PersistenceEncoder encoder;
        ResetEncoder(encoder, Make(cases[0]));
        _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        Check(encoder.buffer == encoder.inlineBuffer, __LINE__);
        Check(encoder.capacity == sizeof(encoder.inlineBuffer), __LINE__);
        Check(encoder.size == digest.size(), __LINE__);
        Check(std::memcmp(encoder.buffer, digest.data(), digest.size()) == 0, __LINE__);
        Check(encoder.sha1.cursor == 24 && encoder.sha1.totalBytes == 24, __LINE__);
        const std::uint8_t salt[4] = {101, 0, 0, 0};
        Check(std::memcmp(encoder.sha1.buffer, salt, 4) == 0, __LINE__);
        SHA1Digest checksum;
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&encoder.sha1, &checksum);
        Check(Hex(checksum) == "6FED4A05DD4D2D6D8271FAE31449335D648D60A9", __LINE__);

        PersistenceDecoder decoder{encoder.buffer, encoder.buffer, encoder.buffer + encoder.size, Make(cases[0])};
        const OptionalSHA1Digest decoded = _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6decodeERNS0_7DecoderE(&decoder);
        Check(decoded.hasValue && decoded.value == digest, __LINE__);
        Check(decoder.bufferPosition == decoder.bufferEnd, __LINE__);
        SHA1Digest decodedChecksum;
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&decoder.sha1, &decodedChecksum);
        Check(decodedChecksum == checksum, __LINE__);
    }

    {
        Case pending{InitialHash, Sequence(0, 50, 1, 0), 50, nullptr};
        PersistenceEncoder encoder;
        ResetEncoder(encoder, Make(pending), 4076);
        for (unsigned i = 0; i < 4076; ++i) encoder.inlineBuffer[i] = static_cast<std::uint8_t>(i);
        _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        Check(encoder.buffer == encoder.inlineBuffer && encoder.capacity == 4096 && encoder.size == 4096, __LINE__);
        Check(std::memcmp(encoder.buffer + 4076, digest.data(), digest.size()) == 0, __LINE__);
        Check(encoder.sha1.cursor == 10 && encoder.sha1.totalBytes == 74, __LINE__);
        SHA1Digest checksum;
        _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&encoder.sha1, &checksum);
        Check(Hex(checksum) == "32A66B256525A031D02A2F31DCC62AB1099703EF", __LINE__);
    }

    {
        PersistenceEncoder encoder;
        ResetEncoder(encoder, Make(cases[0]), 4080);
        for (unsigned i = 0; i < 4080; ++i) encoder.inlineBuffer[i] = static_cast<std::uint8_t>(i * 3 + 1);
        const SHA1 sha1Before = encoder.sha1;
        Check(ThrowsException<std::runtime_error>([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        }), __LINE__);
        Check(encoder.buffer == encoder.inlineBuffer && encoder.capacity == 4096 && encoder.size == 4080, __LINE__);
        Check(std::memcmp(&encoder.sha1, &sha1Before, sizeof(sha1Before)) == 0, __LINE__);
    }

    {
        TestAllocatorState state;
        activeAllocatorState = &state;
        const PersistenceAllocator allocator{&TestAllocate, &TestDeallocate};
        RegisterPersistenceAllocator_nid_no_patch(&allocator);

        PersistenceEncoder encoder;
        ResetEncoder(encoder, Make(cases[0]), 4080);
        for (unsigned i = 0; i < 4080; ++i) encoder.inlineBuffer[i] = static_cast<std::uint8_t>(i * 3 + 1);

        _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        Check(state.allocateCalls == 1 && state.lastAllocateSize == 8192, __LINE__);
        Check(state.deallocateCalls == 0, __LINE__);
        Check(encoder.buffer != encoder.inlineBuffer && encoder.capacity == 8192 && encoder.size == 4100, __LINE__);
        for (unsigned i = 0; i < 4080; ++i) Check(encoder.buffer[i] == static_cast<std::uint8_t>(i * 3 + 1), __LINE__);
        Check(std::memcmp(encoder.buffer + 4080, digest.data(), digest.size()) == 0, __LINE__);

        std::uint8_t* firstHeapBuffer = encoder.buffer;
        encoder.size = 8180;
        for (unsigned i = 4100; i < 8180; ++i) encoder.buffer[i] = static_cast<std::uint8_t>(i * 5 + 2);
        const SHA1Digest* aliasedDigest = reinterpret_cast<const SHA1Digest*>(encoder.buffer + 100);
        const SHA1Digest expectedAliased = *aliasedDigest;
        _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, aliasedDigest);
        Check(state.allocateCalls == 2 && state.lastAllocateSize == 16384, __LINE__);
        Check(state.deallocateCalls == 1 && state.lastFreed == firstHeapBuffer, __LINE__);
        Check(encoder.buffer != firstHeapBuffer && encoder.capacity == 16384 && encoder.size == 8200, __LINE__);
        Check(std::memcmp(encoder.buffer + 8180, expectedAliased.data(), expectedAliased.size()) == 0, __LINE__);

        std::uint8_t* secondHeapBuffer = encoder.buffer;
        encoder.size = encoder.capacity;
        state.failNext = true;
        const SHA1 sha1BeforeFail = encoder.sha1;
        Check(ThrowsException<std::bad_alloc>([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        }), __LINE__);
        Check(encoder.buffer == secondHeapBuffer && encoder.capacity == 16384 && encoder.size == 16384, __LINE__);
        Check(std::memcmp(&encoder.sha1, &sha1BeforeFail, sizeof(sha1BeforeFail)) == 0, __LINE__);

        TestDeallocate(secondHeapBuffer);
        Check(state.liveAllocations.empty(), __LINE__);
        RegisterPersistenceAllocator_nid_no_patch(nullptr);
        activeAllocatorState = nullptr;
    }

    {
        PersistenceEncoder encoder;
        ResetEncoder(encoder, Make(cases[0]));
        Check(ThrowsInvalidArgument([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(nullptr, &digest);
        }), __LINE__);
        Check(ThrowsInvalidArgument([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, nullptr);
        }), __LINE__);

        encoder.sha1.cursor = 64;
        const SHA1 sha1Cursor = encoder.sha1;
        Check(ThrowsInvalidArgument([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        }), __LINE__);
        Check(encoder.size == 0 && std::memcmp(&encoder.sha1, &sha1Cursor, sizeof(sha1Cursor)) == 0, __LINE__);

        encoder.sha1.cursor = 0;
        encoder.buffer = nullptr;
        Check(ThrowsInvalidArgument([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        }), __LINE__);

        encoder.buffer = encoder.inlineBuffer;
        encoder.size = encoder.capacity + 1;
        Check(ThrowsInvalidArgument([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        }), __LINE__);

        encoder.capacity = std::numeric_limits<unsigned>::max();
        encoder.size = std::numeric_limits<unsigned>::max() - 19;
        Check(ThrowsException<std::length_error>([&] {
            _ZN3WTF11Persistence5CoderISt5arrayIhLm20EEE6encodeERNS0_7EncoderERKS3_(&encoder, &digest);
        }), __LINE__);
        Check(encoder.size == std::numeric_limits<unsigned>::max() - 19, __LINE__);
    }

    {
        SHA1Digest sample{};
        for (std::size_t i = 0; i < sample.size(); ++i) sample[i] = static_cast<std::uint8_t>(i);

        errno = 53;
        Check(ThrowsInvalidArgument([] {
            (void)_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE(nullptr);
        }), __LINE__);
        Check(errno == 53, __LINE__);

        errno = 54;
        const auto* unreadable = reinterpret_cast<const SHA1Digest*>(std::uintptr_t{0x10});
        Check(ThrowsInvalidArgument([&] {
            (void)_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE(unreadable);
        }), __LINE__);
        Check(errno == 54, __LINE__);

        Check(ThrowsException<std::runtime_error>([&] {
            (void)_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE(&sample);
        }), __LINE__);

        TestAllocatorState state{};
        activeAllocatorState = &state;
        constexpr PersistenceAllocator allocator{&TestAllocate, &TestDeallocate};
        RegisterPersistenceAllocator_nid_no_patch(&allocator);

        errno = 55;
        Check(ThrowsInvalidArgument([&] {
            (void)_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE(unreadable);
        }), __LINE__);
        Check(errno == 55 && state.allocateCalls == 0, __LINE__);

        auto CheckHexResult = [&](const SHA1Digest& input, const char* expected) {
            errno = 56;
            OwnedCString result = _ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE(&input);
            Check(errno == 56, __LINE__);
            Check(result.buffer != nullptr, __LINE__);
            Check(result.buffer->refCount == 1u, __LINE__);
            Check(result.buffer->length == 40u, __LINE__);
            const char* text = reinterpret_cast<const char*>(result.buffer + 1);
            Check(std::memcmp(text, expected, 41) == 0, __LINE__);
            Check(state.lastAllocateSize == sizeof(CStringBuffer) + 41, __LINE__);
            TestDeallocate(result.buffer);
        };

        CheckHexResult(sample, "000102030405060708090A0B0C0D0E0F10111213");

        SHA1Digest zeroes{};
        CheckHexResult(zeroes, "0000000000000000000000000000000000000000");

        SHA1Digest ones{};
        ones.fill(0xFF);
        CheckHexResult(ones, "FFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFFF");

        SHA1Digest highNibbles{};
        for (std::size_t i = 0; i < highNibbles.size(); ++i)
            highNibbles[i] = static_cast<std::uint8_t>(0xA0 + (i % 6) * 0x10 + (15 - (i % 16)));
        CheckHexResult(highNibbles, Hex(highNibbles).c_str());

        for (const auto& entry : cases) {
            SHA1 sha1 = Make(entry);
            SHA1Digest computed{};
            _ZN3WTF4SHA111computeHashERSt5arrayIhLm20EE(&sha1, &computed);
            CheckHexResult(computed, entry.digest);
        }

        using RawSretCall = void* (APS5_VABI*)(CStringBuffer**, const SHA1Digest*);
        const auto rawCall = reinterpret_cast<RawSretCall>(&_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE);
        CStringBuffer* sretSlot = nullptr;
        void* returned = rawCall(&sretSlot, &sample);
        Check(returned == &sretSlot, __LINE__);
        Check(sretSlot != nullptr && sretSlot->refCount == 1u && sretSlot->length == 40u, __LINE__);
        Check(std::memcmp(reinterpret_cast<const char*>(sretSlot + 1), "000102030405060708090A0B0C0D0E0F10111213", 41) == 0, __LINE__);
        TestDeallocate(sretSlot);

        state.failNext = true;
        Check(ThrowsException<std::bad_alloc>([&] {
            (void)_ZN3WTF4SHA19hexDigestERKSt5arrayIhLm20EE(&sample);
        }), __LINE__);

        Check(state.liveAllocations.empty(), __LINE__);
        RegisterPersistenceAllocator_nid_no_patch(nullptr);
        activeAllocatorState = nullptr;
    }
    DecodeCStrings();
    DecodeStrings();
    return 0;
}
