#include <algorithm>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <mutex>
#include <new>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>
#include "Decoder/Jpeg.hpp"
#include "SceTypes.hpp"
#include "prx/libc/include/General.hpp"

static_assert(sizeof(JpegDecCreateParam) == 0xC);
static_assert(sizeof(JpegDecParseParam) == 0x10);
static_assert(sizeof(JpegDecDecodeParam) == 0x30);
static_assert(sizeof(JpegDecImageInfo) == 0x24);
static_assert(sizeof(JpegDecInputChunk) == 0x10);
static_assert(sizeof(JpegDecInputControlParam) == 0x38 && offsetof(JpegDecInputControlParam, operation) == 0x28 && offsetof(JpegDecInputControlParam, reserved) == 0x34);

namespace {

constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_ADDR = static_cast<std::int32_t>(0x80650001);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_SIZE = static_cast<std::int32_t>(0x80650002);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_PARAM = static_cast<std::int32_t>(0x80650003);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_HANDLE = static_cast<std::int32_t>(0x80650004);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_IMAGE_TOO_WIDE = static_cast<std::int32_t>(0x80650006);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_INVALID_DATA = static_cast<std::int32_t>(0x80650010);
constexpr std::int32_t SCE_JPEG_DEC_ERROR_DECODE_ERROR = static_cast<std::int32_t>(0x80650012);

constexpr std::uint32_t ATTRIBUTE_CSC_ORDINARY = 1;
constexpr std::uint32_t ATTRIBUTE_CSC_FULL = 2;
constexpr std::uint32_t MEMORY_OVERHEAD = 29536;
constexpr std::uintptr_t HANDLE_ALIGNMENT = 32;

constexpr std::uint16_t DECODE_MODE_NORMAL = 0;
constexpr std::uint16_t DOWN_SCALE_NONE = 1;
constexpr std::uint16_t DOWN_SCALE_HALF = 2;
constexpr std::uint16_t DOWN_SCALE_QUARTER = 4;
constexpr std::uint16_t DOWN_SCALE_EIGHTH = 8;

constexpr std::uint16_t PIXEL_FORMAT_R8G8B8A8 = 0;
constexpr std::uint16_t PIXEL_FORMAT_B8G8R8A8 = 1;

constexpr std::uint16_t COLOR_SPACE_UNKNOWN = 0;
constexpr std::uint16_t COLOR_SPACE_YCBCR = 1;
constexpr std::uint16_t COLOR_SPACE_GRAY = 2;

constexpr std::uint32_t BYTES_PER_PIXEL = 4;
constexpr std::uint32_t MAX_PACKED_DIMENSION = 32767;
constexpr std::uint32_t INPUT_BLOCK_SIZE = 0x800;

constexpr std::uint16_t OPERATION_PARSE = 0x10;
constexpr std::uint16_t OPERATION_DECODE = 0x20;

struct Context {
    Context* self;
    std::uint32_t attribute;
    std::uint32_t maxImageWidth;
    std::uint32_t rowBufferCapacity;
};

static_assert(sizeof(Context) + HANDLE_ALIGNMENT - 1 <= MEMORY_OVERHEAD);

std::int32_t validateCreateParam(const JpegDecCreateParam* param) {
    if (!param) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->this_size != sizeof(JpegDecCreateParam)) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (param->attribute != ATTRIBUTE_CSC_ORDINARY && param->attribute != ATTRIBUTE_CSC_FULL) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->max_image_width == 0 || param->max_image_width > 65535) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    return 0;
}

std::uint32_t workMemorySize(const JpegDecCreateParam& param) {
    const std::uint32_t alignedWidth = (param.max_image_width + 31) & ~std::uint32_t{31};
    return MEMORY_OVERHEAD + (alignedWidth + 16) * (param.attribute == ATTRIBUTE_CSC_ORDINARY ? 54 : 166);
}

bool validDecodeOptions(std::uint16_t decodeMode, std::uint16_t downScale) {
    return decodeMode <= 1 && (downScale == DOWN_SCALE_NONE || downScale == DOWN_SCALE_HALF || downScale == DOWN_SCALE_QUARTER || downScale == DOWN_SCALE_EIGHTH);
}

void requireDecodeMode(const char* function, std::uint16_t decodeMode) {
    if (decodeMode != DECODE_MODE_NORMAL) throw std::runtime_error(std::string(function) + ": decode mode " + std::to_string(decodeMode) + " is not implemented");
}

Context* toContext(void* handle) {
    const auto address = reinterpret_cast<std::uintptr_t>(handle);
    if (address == 0 || address % HANDLE_ALIGNMENT != 0) return nullptr;
    auto* context = reinterpret_cast<Context*>(handle);
    return context->self == context ? context : nullptr;
}

void fillImageInfo(const Decoder::Jpeg::Header& header, std::uint32_t downScale, JpegDecImageInfo* info) {
    *info = {};
    info->image_width = header.width;
    info->image_height = header.height;
    info->color_space = COLOR_SPACE_UNKNOWN;
    if (header.channels == 1) info->color_space = COLOR_SPACE_GRAY;
    if (header.channels == 3 && (header.app0BeforeFrame || (header.componentIds[0] == 1 && header.componentIds[1] == 2 && header.componentIds[2] == 3))) info->color_space = COLOR_SPACE_YCBCR;
    info->component_number = static_cast<std::uint16_t>(header.channels);
    std::uint32_t maxHorizontal = 1;
    std::uint32_t maxVertical = 1;
    std::uint32_t blocksPerMcu = 0;
    for (std::uint32_t i = 0; i < header.channels; ++i) {
        info->component_id[i] = header.componentIds[i];
        info->sampling_factor[i] = header.samplingFactors[i];
        const std::uint32_t horizontal = header.channels == 1 ? 1 : header.samplingFactors[i] >> 4;
        const std::uint32_t vertical = header.channels == 1 ? 1 : header.samplingFactors[i] & 15;
        if (horizontal > maxHorizontal) maxHorizontal = horizontal;
        if (vertical > maxVertical) maxVertical = vertical;
        blocksPerMcu += horizontal * vertical;
    }
    if (info->color_space == COLOR_SPACE_GRAY) info->suitable_csc_attribute = ATTRIBUTE_CSC_ORDINARY;
    if (info->color_space == COLOR_SPACE_YCBCR) info->suitable_csc_attribute = (12 / maxHorizontal) * blocksPerMcu < 37 ? ATTRIBUTE_CSC_ORDINARY : ATTRIBUTE_CSC_FULL;
    if (header.progressive) {
        const std::uint32_t mcuColumns = (header.width + 8 * maxHorizontal - 1) / (8 * maxHorizontal);
        const std::uint32_t mcuRows = (header.height + 8 * maxVertical - 1) / (8 * maxVertical);
        info->coefficient_mem_size = mcuColumns * mcuRows * blocksPerMcu * 128 + 64;
    }
    info->output_image_width = Decoder::Jpeg::ScaledDimension(header.width, downScale);
    info->output_image_height = Decoder::Jpeg::ScaledDimension(header.height, downScale);
}

std::uint32_t rowBufferSize(const Decoder::Jpeg::Header& header, std::uint32_t downScale) {
    std::uint32_t maxHorizontal = 1;
    if (header.channels != 1) {
        for (std::uint32_t i = 0; i < header.channels; ++i) {
            const std::uint32_t horizontal = header.samplingFactors[i] >> 4;
            if (horizontal > maxHorizontal) maxHorizontal = horizontal;
        }
    }
    const std::uint32_t mcuColumns = (header.width + 8 * maxHorizontal - 1) / (8 * maxHorizontal);
    const std::uint32_t blockSize = 8 / downScale;
    std::uint32_t size = 0;
    for (std::uint32_t i = 0; i < header.channels; ++i) {
        const std::uint32_t horizontal = header.channels == 1 ? 1 : header.samplingFactors[i] >> 4;
        const std::uint32_t vertical = header.channels == 1 ? 1 : header.samplingFactors[i] & 15;
        const std::uint32_t stride = (mcuColumns * blockSize * horizontal + 7) & ~std::uint32_t{7};
        size += stride * vertical * blockSize;
    }
    return (size + 31) & ~std::uint32_t{31};
}

std::span<const std::uint8_t> toBytes(const void* data, std::uint32_t size) {
    return {static_cast<const std::uint8_t*>(data), size};
}

struct DecodeTarget {
    void* image;
    std::uint32_t imageSize;
    const void* coefficient;
    std::uint32_t coefficientSize;
    std::uint32_t downScale;
    std::uint16_t pixelFormat;
    std::uint16_t alpha;
    std::uint32_t pitch;
};

std::uint64_t imagePitch(const DecodeTarget& target, std::uint32_t outputWidth) {
    const std::uint64_t rowSize = static_cast<std::uint64_t>(outputWidth) * BYTES_PER_PIXEL;
    return target.pitch == 0 ? rowSize : target.pitch;
}

std::int32_t checkTarget(const Context& context, const Decoder::Jpeg::Header& header, const DecodeTarget& target) {
    const std::uint32_t downScale = target.downScale;
    JpegDecImageInfo parsedInfo{};
    fillImageInfo(header, downScale, &parsedInfo);
    if (parsedInfo.coefficient_mem_size != 0 && (!target.coefficient || target.coefficientSize < parsedInfo.coefficient_mem_size)) return static_cast<std::int32_t>(0x80650005);
    if (parsedInfo.color_space == COLOR_SPACE_UNKNOWN) return static_cast<std::int32_t>(0x80650011);
    const std::uint32_t outputWidth = Decoder::Jpeg::ScaledDimension(header.width, downScale);
    const std::uint32_t outputHeight = Decoder::Jpeg::ScaledDimension(header.height, downScale);

    const std::uint64_t rowSize = static_cast<std::uint64_t>(outputWidth) * BYTES_PER_PIXEL;
    const std::uint64_t pitch = imagePitch(target, outputWidth);
    if (target.pitch != 0) {
        if (pitch < rowSize || pitch % 4 != 0 || pitch * outputHeight >= 0x80000000 || pitch * outputHeight > target.imageSize) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    } else if (pitch * outputHeight > target.imageSize) {
        return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    }
    if (((outputWidth + 31) & ~std::uint32_t{31}) > context.maxImageWidth || rowBufferSize(header, downScale) > context.rowBufferCapacity) return SCE_JPEG_DEC_ERROR_IMAGE_TOO_WIDE;
    return 0;
}

std::int32_t writeImage(const Decoder::Jpeg::Header& header, std::span<const std::uint8_t> jpeg, const DecodeTarget& target, JpegDecImageInfo* imageInfo) {
    const std::uint32_t downScale = target.downScale;
    const std::uint32_t outputWidth = Decoder::Jpeg::ScaledDimension(header.width, downScale);
    const std::uint32_t outputHeight = Decoder::Jpeg::ScaledDimension(header.height, downScale);
    const std::uint64_t pitch = imagePitch(target, outputWidth);

    const std::optional<Decoder::Jpeg::Image> image = downScale == DOWN_SCALE_NONE ? Decoder::Jpeg::Decode(jpeg) : Decoder::Jpeg::DecodeScaled(jpeg, downScale);
    if (!image || image->width != outputWidth || image->height != outputHeight) return SCE_JPEG_DEC_ERROR_DECODE_ERROR;

    const bool swapRedBlue = target.pixelFormat == PIXEL_FORMAT_B8G8R8A8;
    const auto alpha = static_cast<std::uint8_t>(target.alpha);
    auto* output = static_cast<std::uint8_t*>(target.image);
    for (std::uint32_t y = 0; y < image->height; ++y) {
        std::uint8_t* out = output + y * pitch;
        for (std::uint32_t x = 0; x < image->width; ++x) {
            const std::uint8_t* in = image->pixels.data() + (static_cast<std::size_t>(y) * image->width + x) * image->channels;
            std::uint8_t* pixel = out + x * BYTES_PER_PIXEL;
            if (image->channels == 1) {
                pixel[0] = in[0];
                pixel[1] = in[0];
                pixel[2] = in[0];
            } else {
                pixel[0] = in[0];
                pixel[1] = in[1];
                pixel[2] = in[2];
            }
            if (swapRedBlue) std::swap(pixel[0], pixel[2]);
            pixel[3] = alpha;
        }
    }

    if (imageInfo) fillImageInfo(header, downScale, imageInfo);
    if (outputWidth > MAX_PACKED_DIMENSION || outputHeight > MAX_PACKED_DIMENSION) return 0;
    return static_cast<std::int32_t>(outputWidth << 16 | outputHeight);
}

struct InputStream {
    JpegDecInputCallback input = nullptr;
    void* argument = nullptr;
    const std::uint8_t* chunk = nullptr;
    std::uint32_t chunkSize = 0;
    std::int32_t status = 0;
    std::vector<std::uint8_t> data;
    std::optional<Decoder::Jpeg::Header> header;
    std::uint32_t downScale = 0;
    std::size_t scanOffset = 2;
    bool inScan = false;
    std::optional<std::size_t> end;
};

std::mutex streamMutex;
std::unordered_map<const Context*, InputStream> streams;

void forgetStream(const Context* context) {
    std::lock_guard lock(streamMutex);
    streams.erase(context);
}

void pull(InputStream& stream, std::uint32_t size) {
    while (size > 0 && stream.status == 0) {
        if (stream.chunkSize == 0) {
            JpegDecInputChunk chunk{};
            const std::int32_t result = stream.input(&chunk, stream.argument);
            if (result < 0 || !chunk.data || chunk.size == 0) {
                stream.status = result < 0 ? result : 1;
                return;
            }
            stream.chunk = static_cast<const std::uint8_t*>(chunk.data);
            stream.chunkSize = chunk.size;
        }
        const std::uint32_t count = std::min(size, stream.chunkSize);
        stream.data.insert(stream.data.end(), stream.chunk, stream.chunk + count);
        stream.chunk += count;
        stream.chunkSize -= count;
        size -= count;
    }
}

bool endFound(InputStream& stream) {
    const std::vector<std::uint8_t>& data = stream.data;
    std::size_t& at = stream.scanOffset;
    while (true) {
        if (stream.inScan) {
            while (at + 1 < data.size() && !(data[at] == 0xFF && data[at + 1] != 0x00 && data[at + 1] != 0xFF && (data[at + 1] < 0xD0 || data[at + 1] > 0xD7))) ++at;
            if (at + 1 >= data.size()) return false;
            stream.inScan = false;
        }
        if (at + 1 >= data.size()) return false;
        if (data[at] != 0xFF) return true;
        const std::uint8_t marker = data[at + 1];
        if (marker == 0xFF) {
            ++at;
            continue;
        }
        if (marker == 0xD9) {
            stream.end = at + 2;
            return true;
        }
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD8)) {
            at += 2;
            continue;
        }
        if (at + 3 >= data.size()) return false;
        const std::size_t length = (static_cast<std::size_t>(data[at + 2]) << 8) | data[at + 3];
        if (length < 2) return true;
        if (at + 2 + length > data.size()) return false;
        at += 2 + length;
        if (marker == 0xDA) stream.inScan = true;
    }
}

bool mayBecomeHeader(InputStream& stream) {
    if (stream.data.size() < 2) return true;
    if (stream.data[0] != 0xFF || stream.data[1] != 0xD8) return false;
    return !endFound(stream);
}

std::int32_t parseStream(const Context* context, const JpegDecInputControlParam* param, JpegDecImageInfo* imageInfo) {
    if (!imageInfo) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    requireDecodeMode("sceJpegDecDecodeWithInputControl", param->operation & 1);
    if (!validDecodeOptions(0, param->down_scale)) throw std::runtime_error("sceJpegDecDecodeWithInputControl: the result for this down scale is not known");
    InputStream& stream = [&]() -> InputStream& {
        std::lock_guard lock(streamMutex);
        return streams[context] = InputStream{};
    }();
    stream.input = param->input;
    stream.argument = param->input_argument;
    while (!(stream.header = Decoder::Jpeg::ParseHeader(stream.data)) && stream.status == 0 && mayBecomeHeader(stream)) pull(stream, INPUT_BLOCK_SIZE);
    if (stream.status < 0) return stream.status;
    if (!stream.header) return SCE_JPEG_DEC_ERROR_INVALID_DATA;
    stream.downScale = param->down_scale;
    fillImageInfo(*stream.header, stream.downScale, imageInfo);
    return 0;
}

std::int32_t decodeStream(const Context* context, const JpegDecInputControlParam* param) {
    if (!param->image_mem_addr) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    const std::uint16_t format = param->pixel_format;
    if ((format == 0 || format == 1 || format == 9 || format == 10) && (reinterpret_cast<std::uintptr_t>(param->image_mem_addr) & 3) != 0) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->image_mem_size == 0) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (!validDecodeOptions(0, param->down_scale)) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (format != 0 && format != 1 && format != 9 && format != 10 && format != 11) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->alpha_value > 255) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    requireDecodeMode("sceJpegDecDecodeWithInputControl", param->operation & 1);
    if (format != PIXEL_FORMAT_R8G8B8A8 && format != PIXEL_FORMAT_B8G8R8A8) throw std::runtime_error("sceJpegDecDecodeWithInputControl: pixel format is not implemented");
    InputStream* found = nullptr;
    {
        std::lock_guard lock(streamMutex);
        const auto entry = streams.find(context);
        if (entry != streams.end()) found = &entry->second;
    }
    if (!found || !found->header) throw std::runtime_error("sceJpegDecDecodeWithInputControl: decoding without a parsed header is not implemented");
    InputStream& stream = *found;
    if (param->down_scale != stream.downScale) throw std::runtime_error("sceJpegDecDecodeWithInputControl: decoding at another down scale than the parse is not implemented");
    stream.status = 0;
    stream.input = param->input;
    stream.argument = param->input_argument;
    const DecodeTarget target{param->image_mem_addr, param->image_mem_size, param->coefficient_mem_addr, param->coefficient_mem_size,
                              param->down_scale, format, param->alpha_value, param->image_pitch};
    const std::int32_t checked = checkTarget(*context, *stream.header, target);
    if (checked != 0) return checked;
    while (!stream.end && !endFound(stream) && stream.status == 0) pull(stream, INPUT_BLOCK_SIZE);
    if (stream.status < 0) return stream.status;
    const std::span<const std::uint8_t> jpeg(stream.data.data(), stream.end.value_or(stream.data.size()));
    return writeImage(*stream.header, jpeg, target, nullptr);
}

}  // namespace

extern "C" {

int32_t APS5_VABI sceJpegDecCreate(const JpegDecCreateParam* param, void* memoryAddress, uint32_t memorySize, void** handle) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    if (!memoryAddress) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (memorySize < workMemorySize(*param)) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (!handle) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    const auto address = reinterpret_cast<std::uintptr_t>(memoryAddress);
    const std::uintptr_t aligned = (address + HANDLE_ALIGNMENT - 1) & ~(HANDLE_ALIGNMENT - 1);
    auto* context = new (reinterpret_cast<void*>(aligned)) Context{};
    context->self = context;
    context->attribute = param->attribute;
    context->maxImageWidth = (param->max_image_width + 31) & ~std::uint32_t{31};
    context->rowBufferCapacity = (context->maxImageWidth + 16) * (param->attribute == ATTRIBUTE_CSC_ORDINARY ? 24 : 80);
    forgetStream(context);
    *handle = context;
    return 0;
}

int32_t APS5_VABI sceJpegDecDecode(void* handle, const JpegDecDecodeParam* param, JpegDecImageInfo* imageInfo) {
    const Context* context = toContext(handle);
    if (!context) return SCE_JPEG_DEC_ERROR_INVALID_HANDLE;
    if (!param) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (!param->jpeg_mem_addr || !param->image_mem_addr) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if ((reinterpret_cast<std::uintptr_t>(param->image_mem_addr) & 3) != 0 && (param->pixel_format == 0 || param->pixel_format == 1 || param->pixel_format == 9 || param->pixel_format == 10)) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->jpeg_mem_size == 0 || param->image_mem_size == 0) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (!validDecodeOptions(param->decode_mode, param->down_scale)) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->pixel_format != 0 && param->pixel_format != 1 && param->pixel_format != 9 && param->pixel_format != 10 && param->pixel_format != 11) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (param->alpha_value > 255) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    requireDecodeMode(__func__, param->decode_mode);
    if (param->pixel_format != PIXEL_FORMAT_R8G8B8A8 && param->pixel_format != PIXEL_FORMAT_B8G8R8A8) throw std::runtime_error("sceJpegDecDecode: pixel format is not implemented");

    const std::span<const std::uint8_t> jpeg = toBytes(param->jpeg_mem_addr, param->jpeg_mem_size);
    const std::optional<Decoder::Jpeg::Header> header = Decoder::Jpeg::ParseHeader(jpeg);
    if (!header) return SCE_JPEG_DEC_ERROR_INVALID_DATA;
    const DecodeTarget target{param->image_mem_addr, param->image_mem_size, param->coefficient_mem_addr, param->coefficient_mem_size,
                              param->down_scale, param->pixel_format, param->alpha_value, param->image_pitch};
    const std::int32_t checked = checkTarget(*context, *header, target);
    if (checked != 0) return checked;
    return writeImage(*header, jpeg, target, imageInfo);
}

int32_t APS5_VABI sceJpegDecDelete(void* handle) {
    Context* context = toContext(handle);
    if (!context) return SCE_JPEG_DEC_ERROR_INVALID_HANDLE;
    forgetStream(context);
    context->self = nullptr;
    return 0;
}

int32_t APS5_VABI sceJpegDecParseHeader(const JpegDecParseParam* param, JpegDecImageInfo* imageInfo) {
    if (!param) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (!param->jpeg_mem_addr) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    if (param->jpeg_mem_size == 0) return SCE_JPEG_DEC_ERROR_INVALID_SIZE;
    if (!validDecodeOptions(param->decode_mode, param->down_scale)) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    if (!imageInfo) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    requireDecodeMode(__func__, param->decode_mode);
    const std::uint32_t downScale = param->down_scale;
    const std::optional<Decoder::Jpeg::Header> header = Decoder::Jpeg::ParseHeader(toBytes(param->jpeg_mem_addr, param->jpeg_mem_size));
    if (!header) return SCE_JPEG_DEC_ERROR_INVALID_DATA;
    fillImageInfo(*header, downScale, imageInfo);
    return 0;
}

int32_t APS5_VABI sceJpegDecQueryMemorySize(const JpegDecCreateParam* param) {
    const std::int32_t result = validateCreateParam(param);
    if (result != 0) return result;
    return static_cast<std::int32_t>(workMemorySize(*param));
}

int32_t APS5_VABI sceJpegDecDecodeWithInputControl(void* handle, const JpegDecInputControlParam* param, JpegDecImageInfo* imageInfo) {
    const Context* context = toContext(handle);
    if (!context) return SCE_JPEG_DEC_ERROR_INVALID_HANDLE;
    if (!param || !param->input) return SCE_JPEG_DEC_ERROR_INVALID_ADDR;
    const std::uint16_t operation = param->operation & ~std::uint16_t{1};
    if ((operation != OPERATION_PARSE && operation != OPERATION_DECODE) || param->reserved != 0) return SCE_JPEG_DEC_ERROR_INVALID_PARAM;
    return operation == OPERATION_PARSE ? parseStream(context, param, imageInfo) : decodeStream(context, param);
}

}
