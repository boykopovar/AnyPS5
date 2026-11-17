#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceJxr/libSceJxr.hpp"
#include "prx/libSceJxr/Factory.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

extern "C" {
std::size_t JxrWmpReferenceParameterSize();
void JxrWmpReferenceParameters(void* value, std::int32_t spatial, std::uint8_t alphaMode);
std::size_t JxrWmpReferenceTranscodeSize();
void JxrWmpReferenceTranscodeParameters(void* value, std::uint32_t width, std::uint32_t height, std::uint8_t alphaMode);
std::size_t JxrWmpReferenceMetadataSize();
void JxrWmpReferenceConfigureDecoder(void* value);
void JxrWmpReferenceMetadata(void* value, const char* software, std::uint16_t rating, std::int32_t wide);
const char* JxrWmpReferenceSoftware(const void* value);
std::uint32_t JxrWmpReferenceRating(const void* value);
std::int64_t JxrWmpReferenceEncode(const void* parameters, std::int32_t alpha, std::uint32_t width, std::uint32_t height,
                                   const std::uint8_t* pixels, std::uint32_t stride, std::int32_t banded,
                                   const std::uint8_t* colorContext, std::uint32_t colorContextSize,
                                   const void* metadata, std::uint8_t* temporary, std::uint8_t* output,
                                   std::size_t capacity, std::size_t* written);
std::int64_t JxrWmpReferenceDecode(const std::uint8_t* data, std::size_t size, std::uint8_t* pixels,
                                   std::uint32_t stride, std::int32_t* width, std::int32_t* height);
std::int64_t JxrWmpReferenceTranscode(const std::uint8_t* data, std::size_t size, const void* encodeParameters,
                                      const void* transcodeParameters, std::uint8_t* output, std::size_t capacity,
                                      std::size_t* written);
}

namespace {

void Check(bool value, int line) {
    if (value) return;
    std::fprintf(stderr, "GuestJxrWmp.cpp:%d: requirement failed\n", line);
    std::abort();
}
#define Require(value) Check((value), __LINE__)

template<typename TException, typename TAction>
void RequireThrows(TAction action, int line) {
    try {
        action();
    } catch (const TException&) {
        return;
    }
    Check(false, line);
}
#define RequireThrow(type, action) RequireThrows<type>(action, __LINE__)

constexpr std::int64_t OutOfMemory = -101;
constexpr std::int64_t InvalidArgument = -105;
constexpr std::int64_t OutOfSequence = -109;
constexpr std::int64_t NotYetImplemented = -2;
constexpr std::uint32_t Width = 32;
constexpr std::uint32_t Height = 32;
constexpr std::size_t Capacity = 1 << 20;

bool failCalloc = false;
long liveBlocks = 0;
std::size_t callocCalls = 0;

void* APS5_VABI Allocate(std::size_t size) {
    ++liveBlocks;
    return std::malloc(size);
}
void APS5_VABI Free(void* pointer) {
    if (pointer) --liveBlocks;
    std::free(pointer);
}
void* APS5_VABI Calloc(std::size_t count, std::size_t size) {
    ++callocCalls;
    if (failCalloc) return nullptr;
    ++liveBlocks;
    return std::calloc(count, size);
}
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI Align(std::size_t, std::size_t size) { return std::malloc(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return std::realloc(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    *pointer = std::malloc(size);
    return *pointer ? 0 : 12;
}

void InitializeHeap() {
    alignas(16) static std::array<std::byte, 0x40> process{};
    alignas(16) static std::array<std::byte, 0x38> libc{};
    alignas(16) static std::array<std::byte, 0x78> replacement{};
    const auto write = [](auto& data, std::size_t offset, auto value) {
        std::memcpy(data.data() + offset, &value, sizeof(value));
    };
    write(process, 0, std::uint64_t{0x40});
    write(process, 8, std::uint32_t{0x4942524f});
    write(process, 0x38, libc.data());
    write(libc, 0, std::uint64_t{0x38});
    write(libc, 0x30, replacement.data());
    write(replacement, 0, std::uint64_t{0x78});
    write(replacement, 8, std::uint64_t{2});
    write(replacement, 0x20, reinterpret_cast<void*>(&Allocate));
    write(replacement, 0x28, reinterpret_cast<void*>(&Free));
    write(replacement, 0x30, reinterpret_cast<void*>(&Calloc));
    write(replacement, 0x38, reinterpret_cast<void*>(&Reallocate));
    write(replacement, 0x40, reinterpret_cast<void*>(&Align));
    write(replacement, 0x48, reinterpret_cast<void*>(&Realign));
    write(replacement, 0x50, reinterpret_cast<void*>(&PosixAlign));
    ApplicationHeapInitialize_nid_no_patch(process.data());
}

struct Decoder {
    std::int64_t (APS5_VABI* initialize)(Decoder*, Jxr::Stream*);
    std::int64_t (APS5_VABI* getPixelFormat)(Decoder*, JxrGuestGuid*);
    std::int64_t (APS5_VABI* getSize)(Decoder*, std::int32_t*, std::int32_t*);
    std::int64_t (APS5_VABI* getResolution)(Decoder*, float*, float*);
    std::int64_t (APS5_VABI* getColorContext)(Decoder*, std::uint8_t*, std::uint32_t*);
    std::int64_t (APS5_VABI* getDescriptiveMetadata)(Decoder*, void*);
    std::int64_t (APS5_VABI* getRawStream)(Decoder*, Jxr::Stream**);
    std::int64_t (APS5_VABI* copy)(Decoder*, const void*, std::uint8_t*, std::uint32_t);
    std::int64_t (APS5_VABI* getFrameCount)(Decoder*, std::uint32_t*);
    std::int64_t (APS5_VABI* selectFrame)(Decoder*, std::uint32_t);
    std::int64_t (APS5_VABI* release)(Decoder**);
    Jxr::Stream* stream;
    std::int32_t streamOwner;
};

static_assert(offsetof(Decoder, stream) == 88 && offsetof(Decoder, streamOwner) == 96);

struct Rect {
    std::int32_t x;
    std::int32_t y;
    std::int32_t width;
    std::int32_t height;
};

std::vector<std::uint8_t> Parameters(std::int32_t spatial, std::uint8_t alphaMode) {
    std::vector<std::uint8_t> value(JxrWmpReferenceParameterSize());
    JxrWmpReferenceParameters(value.data(), spatial, alphaMode);
    return value;
}

std::vector<std::uint8_t> Pixels(std::uint32_t bytesPerPixel) {
    std::vector<std::uint8_t> pixels(Width * Height * bytesPerPixel);
    for (std::size_t i = 0; i < pixels.size(); ++i) pixels[i] = static_cast<std::uint8_t>((i * 37 + i / 97) & 0xff);
    return pixels;
}

const std::array<std::uint8_t, 6> colorContext{1, 2, 3, 4, 5, 6};
constexpr char software[] = "AnyPS5 WMP";

Jxr::ImageEncode* CreateEncoder() {
    Jxr::ImageEncode* encoder = nullptr;
    Require(PKImageEncode_Create_WMP_nid_postfix(&encoder) == 0 && encoder != nullptr);
    return encoder;
}

Decoder* CreateDecoder() {
    void* decoder = nullptr;
    Require(PKImageDecode_Create_WMP_nid_postfix(&decoder) == 0 && decoder != nullptr);
    return static_cast<Decoder*>(decoder);
}

Jxr::Stream* MemoryStream(std::vector<std::uint8_t>& buffer, std::size_t size) {
    Jxr::Stream* stream = nullptr;
    Require(CreateWS_Memory_nid_postfix(&stream, buffer.data(), size) == 0 && stream != nullptr);
    return stream;
}

void TestCreate() {
    Jxr::ImageEncode* generic = nullptr;
    Require(PKImageEncode_Create_nid_postfix(&generic) == 0);
    auto* encoder = CreateEncoder();
    Require(encoder->wmp == 1 && generic->wmp == 0);
    Require(encoder->setPixelFormat == generic->setPixelFormat && encoder->setSize == generic->setSize);
    Require(encoder->setResolution == generic->setResolution && encoder->writeSource == nullptr);
    Require(encoder->initialize != generic->initialize && encoder->terminate != generic->terminate);
    Require(encoder->setColorContext != generic->setColorContext);
    Require(encoder->setDescriptiveMetadata != generic->setDescriptiveMetadata);
    Require(encoder->writePixels != generic->writePixels && encoder->transcode != nullptr);
    Require(encoder->writePixelsBandedBegin != generic->writePixelsBandedBegin);
    Require(encoder->writePixelsBanded != generic->writePixelsBanded);
    Require(encoder->writePixelsBandedEnd != generic->writePixelsBandedEnd);
    Require(encoder->createNewFrame != generic->createNewFrame && encoder->release != generic->release);
    Require(encoder->stream == nullptr && encoder->width == 0);

    void* factory = nullptr;
    Require(PKCreateCodecFactory_nid_postfix(&factory, 0x0101) == 0);
    auto* codecs = static_cast<Jxr::CodecFactory*>(factory);
    void* created = nullptr;
    Require(codecs->createCodec(&JxrGuest_IID_PKImageWmpEncode_nid_no_patch, &created) == 0 && created != nullptr);
    auto* fromFactory = static_cast<Jxr::ImageEncode*>(created);
    Require(fromFactory->initialize == encoder->initialize && fromFactory->release == encoder->release);
    Require(fromFactory->transcode == encoder->transcode && fromFactory->wmp == 1);

    void* genericDecoder = nullptr;
    Require(PKImageDecode_Create_nid_postfix(&genericDecoder) == 0);
    auto* base = static_cast<Decoder*>(genericDecoder);
    auto* decoder = CreateDecoder();
    Require(decoder->getPixelFormat == base->getPixelFormat && decoder->getResolution == base->getResolution);
    Require(decoder->getFrameCount == base->getFrameCount && decoder->selectFrame == base->selectFrame);
    Require(decoder->initialize != base->initialize && decoder->getSize != base->getSize);
    Require(base->getRawStream == nullptr && decoder->getRawStream != nullptr);
    Require(decoder->copy != base->copy && decoder->getColorContext != base->getColorContext);
    Require(decoder->getDescriptiveMetadata != base->getDescriptiveMetadata && decoder->release != base->release);
    void* decoded = nullptr;
    Require(codecs->createCodec(&JxrGuest_IID_PKImageWmpDecode_nid_no_patch, &decoded) == 0 && decoded != nullptr);
    Require(static_cast<Decoder*>(decoded)->copy == decoder->copy);

    Require(decoder->release(reinterpret_cast<Decoder**>(&decoded)) == 0 && decoded == nullptr);
    Require(decoder->release(nullptr) == 0);
    Require(decoder->release(&decoder) == 0 && decoder == nullptr);
    Require(PKFree_nid_postfix(&genericDecoder) == 0);
    Require(PKFree_nid_postfix(&created) == 0);
    Require(PKFree_nid_postfix(reinterpret_cast<void**>(&encoder)) == 0);
    Require(PKFree_nid_postfix(reinterpret_cast<void**>(&generic)) == 0);
    Require(codecs->release(reinterpret_cast<Jxr::CodecFactory**>(&factory)) == 0);

    failCalloc = true;
    Jxr::ImageEncode* failedEncoder = reinterpret_cast<Jxr::ImageEncode*>(1);
    Require(PKImageEncode_Create_WMP_nid_postfix(&failedEncoder) == OutOfMemory && failedEncoder == nullptr);
    void* failedDecoder = nullptr;
    Require(PKImageDecode_Create_WMP_nid_postfix(&failedDecoder) == OutOfMemory && failedDecoder == nullptr);
    failCalloc = false;
    RequireThrow(std::invalid_argument, [] { PKImageEncode_Create_WMP_nid_postfix(nullptr); });
    RequireThrow(std::invalid_argument, [] { PKImageDecode_Create_WMP_nid_postfix(nullptr); });
}

std::vector<std::uint8_t> Encode(std::int32_t alpha, std::int32_t banded, bool metadata) {
    const auto bytesPerPixel = alpha ? 4u : 3u;
    const auto pixels = Pixels(bytesPerPixel);
    const auto stride = Width * bytesPerPixel;
    auto parameters = Parameters(0, alpha ? 2 : 0);
    std::vector<std::uint8_t> description(JxrWmpReferenceMetadataSize());
    JxrWmpReferenceMetadata(description.data(), software, 4, 0);

    const bool planarTemporary = alpha && banded;
    std::vector<std::uint8_t> expectedTemporary(planarTemporary ? Capacity : 0);
    std::vector<std::uint8_t> expected(Capacity);
    std::size_t expectedSize = 0;
    Require(JxrWmpReferenceEncode(parameters.data(), alpha, Width, Height, pixels.data(), stride, banded,
                                  metadata ? colorContext.data() : nullptr, static_cast<std::uint32_t>(colorContext.size()),
                                  metadata ? description.data() : nullptr,
                                  planarTemporary ? expectedTemporary.data() : nullptr, expected.data(),
                                  expected.size(), &expectedSize) == 0);
    Require(expectedSize > 0);

    const auto blocksBefore = liveBlocks;
    std::vector<std::uint8_t> output(Capacity);
    std::vector<std::uint8_t> temporaryBuffer(planarTemporary ? Capacity : 0);
    Jxr::Stream* temporary = planarTemporary ? MemoryStream(temporaryBuffer, temporaryBuffer.size()) : nullptr;
    auto* stream = MemoryStream(output, output.size());
    auto* encoder = CreateEncoder();
    Require(encoder->initialize(encoder, stream, parameters.data(), parameters.size()) == 0);
    Require(encoder->stream == stream);
    const auto format = alpha ? JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch : JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch;
    Require(encoder->setPixelFormat(encoder, format) == 0);
    Require(encoder->setSize(encoder, Width, Height) == 0);
    if (metadata) {
        const auto callocsBefore = callocCalls;
        Require(encoder->setColorContext(encoder, colorContext.data(), static_cast<std::uint32_t>(colorContext.size())) == 0);
        Require(encoder->setDescriptiveMetadata(encoder, description.data()) == 0);
        Require(callocCalls == callocsBefore + 2);
    }
    auto* writable = const_cast<std::uint8_t*>(pixels.data());
    if (banded) {
        Require(encoder->writePixelsBandedBegin(encoder, temporary) == 0);
        Require(encoder->writePixelsBanded(encoder, 15, writable, stride, 0) == -111);
        Require(encoder->writePixelsBanded(encoder, 16, writable, stride, 0) == 0);
        Require(encoder->writePixelsBanded(encoder, Height - 16, writable + 16 * stride, stride, 1) == 0);
        Require(encoder->writePixelsBandedEnd(encoder) == 0);
    } else {
        Require(encoder->writePixels(encoder, Height, writable, stride) == 0);
        RequireThrow(std::logic_error, [&] { encoder->writePixels(encoder, Height, writable, stride); });
        RequireThrow(std::logic_error, [&] { encoder->writePixelsBandedBegin(encoder, nullptr); });
    }
    Require(encoder->stream == stream);
    std::size_t written = 0;
    Require(stream->getPosition(stream, &written) == 0);
    if (metadata) {
        Require(encoder->setColorContext(encoder, colorContext.data(), 1) == OutOfSequence);
        Require(encoder->setDescriptiveMetadata(encoder, description.data()) == OutOfSequence);
    }
    Require(encoder->terminate(encoder) == 0);
    Require(encoder->createNewFrame(encoder, nullptr, 0) == NotYetImplemented);
    Require(encoder->release(&encoder) == 0 && encoder == nullptr);
    if (temporary != nullptr) Require(temporary->close(&temporary) == 0 && temporary == nullptr);
    Require(liveBlocks == blocksBefore);

    Require(written == expectedSize);
    Require(output == expected);
    Require(temporaryBuffer == expectedTemporary);
    return output;
}

void TestDecode(std::vector<std::uint8_t> encoded, std::uint32_t bytesPerPixel, bool metadata) {
    const auto stride = Width * bytesPerPixel;
    std::vector<std::uint8_t> expected(stride * Height);
    std::int32_t expectedWidth = 0;
    std::int32_t expectedHeight = 0;
    Require(JxrWmpReferenceDecode(encoded.data(), encoded.size(), expected.data(), stride, &expectedWidth,
                                  &expectedHeight) == 0);

    const auto blocksBefore = liveBlocks;
    auto* stream = MemoryStream(encoded, encoded.size());
    auto* decoder = CreateDecoder();
    Require(decoder->initialize(decoder, stream) == 0 && decoder->stream == stream);
    JxrGuestGuid format{};
    Require(decoder->getPixelFormat(decoder, &format) == 0);
    const auto& expectedFormat = bytesPerPixel == 4 ? JxrGuest_GUID_PKPixelFormat32bppBGRA_nid_no_patch
                                                    : JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch;
    Require(std::memcmp(&format, &expectedFormat, sizeof(format)) == 0);
    std::int32_t width = 0;
    std::int32_t height = 0;
    Require(decoder->getSize(decoder, &width, &height) == 0);
    Require(width == expectedWidth && height == expectedHeight && width == static_cast<std::int32_t>(Width));

    std::uint32_t size = 0;
    Require(decoder->getColorContext(decoder, nullptr, &size) == 0);
    Require(size == (metadata ? colorContext.size() : 0));
    if (metadata) {
        std::array<std::uint8_t, 6> context{};
        std::uint32_t small = 2;
        Require(decoder->getColorContext(decoder, context.data(), &small) == -103 && small == 0);
        Require(decoder->getColorContext(decoder, context.data(), &size) == 0 && context == colorContext);
        std::vector<std::uint8_t> description(JxrWmpReferenceMetadataSize());
        Require(decoder->getDescriptiveMetadata(decoder, description.data()) == 0);
        Require(std::string(JxrWmpReferenceSoftware(description.data())) == software);
        Require(JxrWmpReferenceRating(description.data()) == 4);
    }

    Jxr::Stream* raw = nullptr;
    Require(decoder->getRawStream(decoder, &raw) == 0 && raw == stream && decoder->stream == stream);

    JxrWmpReferenceConfigureDecoder(decoder);
    std::vector<std::uint8_t> pixels(stride * Height);
    const Rect rect{0, 0, width, height};
    Require(decoder->copy(decoder, &rect, pixels.data(), stride) == 0);
    Require(pixels == expected && pixels == Pixels(bytesPerPixel));

    Require(decoder->release(&decoder) == 0 && decoder == nullptr);
    Require(stream->close(&stream) == 0 && stream == nullptr);
    Require(liveBlocks == blocksBefore);
}

void TestTranscode(std::vector<std::uint8_t> encoded, std::uint8_t alphaMode) {
    auto parameters = Parameters(1, alphaMode);
    std::vector<std::uint8_t> transcode(JxrWmpReferenceTranscodeSize());
    JxrWmpReferenceTranscodeParameters(transcode.data(), Width, Height, alphaMode);
    std::vector<std::uint8_t> expected(Capacity);
    std::size_t expectedSize = 0;
    Require(JxrWmpReferenceTranscode(encoded.data(), encoded.size(), parameters.data(), transcode.data(),
                                     expected.data(), expected.size(), &expectedSize) == 0);

    const auto blocksBefore = liveBlocks;
    auto* input = MemoryStream(encoded, encoded.size());
    auto* decoder = CreateDecoder();
    Require(decoder->initialize(decoder, input) == 0);
    std::vector<std::uint8_t> output(Capacity);
    auto* stream = MemoryStream(output, output.size());
    auto* encoder = CreateEncoder();
    Require(encoder->initialize(encoder, stream, parameters.data(), parameters.size()) == 0);
    Require(encoder->transcode(encoder, decoder, transcode.data()) == 0);
    std::size_t written = 0;
    Require(stream->getPosition(stream, &written) == 0);
    Require(written == expectedSize && output == expected);
    Require(encoder->release(&encoder) == 0);
    Require(decoder->release(&decoder) == 0);
    Require(input->close(&input) == 0);
    Require(liveBlocks == blocksBefore);
    if (alphaMode != 0) return;

    void* generic = nullptr;
    Require(PKImageDecode_Create_nid_postfix(&generic) == 0);
    std::memcpy(static_cast<std::byte*>(generic) + 112, &JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch,
                sizeof(JxrGuestGuid));
    std::vector<std::uint8_t> unused(Capacity);
    auto* other = MemoryStream(unused, unused.size());
    encoder = CreateEncoder();
    Require(encoder->initialize(encoder, other, parameters.data(), parameters.size()) == 0);
    RequireThrow(std::invalid_argument, [&] { encoder->transcode(encoder, generic, transcode.data()); });
    Require(encoder->release(&encoder) == 0);
    Require(PKFree_nid_postfix(&generic) == 0);
    Require(liveBlocks == blocksBefore);
}

void TestRefusals() {
    const auto blocksBefore = liveBlocks;
    auto parameters = Parameters(0, 0);
    auto pixels = Pixels(3);
    auto* encoder = CreateEncoder();
    Require(encoder->initialize(encoder, nullptr, parameters.data(), parameters.size() - 1) == InvalidArgument);
    Require(encoder->stream == nullptr);
    RequireThrow(std::invalid_argument, [&] { encoder->initialize(encoder, nullptr, nullptr, parameters.size()); });
    RequireThrow(std::invalid_argument, [&] { encoder->writePixels(encoder, Height, pixels.data(), Width * 3); });
    RequireThrow(std::logic_error, [&] { encoder->writePixelsBandedEnd(encoder); });
    std::vector<std::uint8_t> description(JxrWmpReferenceMetadataSize());
    JxrWmpReferenceMetadata(description.data(), software, 1, 1);
    RequireThrow(std::runtime_error, [&] { encoder->setDescriptiveMetadata(encoder, description.data()); });
    RequireThrow(std::invalid_argument, [&] { encoder->setDescriptiveMetadata(encoder, nullptr); });
    RequireThrow(std::invalid_argument, [&] { encoder->release(&encoder); });

    std::vector<std::uint8_t> buffer(Capacity);
    auto* stream = MemoryStream(buffer, buffer.size());
    Require(encoder->initialize(encoder, stream, parameters.data(), parameters.size()) == 0);
    const JxrGuestGuid unknown{0x12345678, 0x1234, 0x5678, {1, 2, 3, 4, 5, 6, 7, 8}};
    Require(encoder->setPixelFormat(encoder, unknown) == 0);
    Require(encoder->setSize(encoder, Width, Height) == 0);
    RequireThrow(std::runtime_error, [&] { encoder->writePixels(encoder, Height, pixels.data(), Width * 3); });
    Require(encoder->release(&encoder) == 0 && encoder == nullptr);

    std::vector<std::uint8_t> sequenceBuffer(Capacity);
    auto* sequenceStream = MemoryStream(sequenceBuffer, sequenceBuffer.size());
    encoder = CreateEncoder();
    Require(encoder->initialize(encoder, sequenceStream, parameters.data(), parameters.size()) == 0);
    Require(encoder->setPixelFormat(encoder, JxrGuest_GUID_PKPixelFormat24bppBGR_nid_no_patch) == 0);
    Require(encoder->setSize(encoder, Width, Height) == 0);
    Require(encoder->writePixelsBandedBegin(encoder, nullptr) == 0);
    RequireThrow(std::logic_error, [&] { encoder->writePixelsBandedBegin(encoder, nullptr); });
    RequireThrow(std::logic_error, [&] { encoder->writePixels(encoder, Height, pixels.data(), Width * 3); });
    RequireThrow(std::logic_error, [&] { encoder->writePixelsBandedEnd(encoder); });
    Require(encoder->release(&encoder) == 0 && encoder == nullptr);

    auto* decoder = CreateDecoder();
    const Rect rect{0, 0, static_cast<std::int32_t>(Width), static_cast<std::int32_t>(Height)};
    RequireThrow(std::invalid_argument, [&] { decoder->copy(decoder, &rect, pixels.data(), Width * 3); });
    RequireThrow(std::invalid_argument, [&] { decoder->initialize(decoder, nullptr); });
    Jxr::Stream* raw = nullptr;
    RequireThrow(std::invalid_argument, [&] { decoder->getRawStream(decoder, &raw); });
    RequireThrow(std::invalid_argument, [&] { decoder->getColorContext(decoder, nullptr, nullptr); });
    Require(decoder->release(&decoder) == 0);
    Require(liveBlocks == blocksBefore);
}

}

int main() {
    InitializeHeap();
    TestCreate();
    const auto plain = Encode(0, 0, true);
    Encode(0, 1, false);
    const auto alpha = Encode(1, 0, false);
    TestDecode(plain, 3, true);
    TestDecode(alpha, 4, false);
    Encode(1, 1, false);
    TestTranscode(plain, 0);
    TestTranscode(alpha, 2);
    TestRefusals();
    Require(liveBlocks == 0);
    return 0;
}
