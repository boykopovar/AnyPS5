#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <cstddef>
#include <csetjmp>
#include <cstdint>
#include <stdexcept>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

namespace {

void AssertTrue(bool condition, const char* message) {
    if (!condition) {
        std::fprintf(stderr, "Assertion failed: %s\n", message);
        std::abort();
    }
}

void GenerateJpeg(std::vector<uint8_t>& outJpeg, int width, int height, int components, J_COLOR_SPACE colorSpace, bool progressive, int restartInterval, const uint8_t* markerData, size_t markerLen) {
    jpeg_compress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);

    unsigned char* memBuf = nullptr;
    unsigned long memSize = 0;
    jpeg_mem_dest(&cinfo, &memBuf, &memSize);

    cinfo.image_width = width;
    cinfo.image_height = height;
    cinfo.input_components = components;
    cinfo.in_color_space = colorSpace;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 85, TRUE);
    if (progressive) {
        jpeg_simple_progression(&cinfo);
    }
    if (restartInterval > 0) {
        cinfo.restart_interval = restartInterval;
    }

    jpeg_start_compress(&cinfo, TRUE);

    if (markerData && markerLen > 0) {
        jpeg_write_marker(&cinfo, JPEG_APP0 + 1, markerData, static_cast<unsigned int>(markerLen));
    }

    std::vector<uint8_t> row(width * components);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width * components; ++x) {
            row[x] = static_cast<uint8_t>((x * 19 + y * 23) & 0xFF);
        }
        JSAMPROW rowPtr = row.data();
        jpeg_write_scanlines(&cinfo, &rowPtr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    outJpeg.assign(memBuf, memBuf + memSize);
    std::free(memBuf);
}

std::vector<uint8_t> DecompressHost(const std::vector<uint8_t>& jpegData, int& outWidth, int& outHeight, int& outComps,
    int inputColorSpace = -1, int outputColorSpace = -1) {
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpegData.data(), jpegData.size());
    jpeg_read_header(&cinfo, TRUE);
    if (inputColorSpace >= 0) cinfo.jpeg_color_space = static_cast<J_COLOR_SPACE>(inputColorSpace);
    if (outputColorSpace >= 0) cinfo.out_color_space = static_cast<J_COLOR_SPACE>(outputColorSpace);
    jpeg_start_decompress(&cinfo);

    outWidth = cinfo.output_width;
    outHeight = cinfo.output_height;
    outComps = cinfo.output_components;

    std::vector<uint8_t> pixels(outWidth * outHeight * outComps);
    while (cinfo.output_scanline < cinfo.output_height) {
        JSAMPROW rowPtr = pixels.data() + cinfo.output_scanline * outWidth * outComps;
        jpeg_read_scanlines(&cinfo, &rowPtr, 1);
    }
    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
    return pixels;
}

static void APS5_VABI GuestInitSource(NKWebKit::GuestDecompressPtr cinfo) {
    (void)cinfo;
}

static int32_t APS5_VABI GuestFillInputBuffer(NKWebKit::GuestDecompressPtr cinfo) {
    (void)cinfo;
    return 0;
}

static void APS5_VABI GuestSkipInputData(NKWebKit::GuestDecompressPtr cinfo, int64_t num_bytes) {
    if (num_bytes <= 0) return;
    if (cinfo && cinfo->src) {
        if (static_cast<uint64_t>(num_bytes) > cinfo->src->bytes_in_buffer) {
            cinfo->src->next_input_byte += cinfo->src->bytes_in_buffer;
            cinfo->src->bytes_in_buffer = 0;
        } else {
            cinfo->src->next_input_byte += num_bytes;
            cinfo->src->bytes_in_buffer -= num_bytes;
        }
    }
}

static int32_t APS5_VABI GuestResyncToRestart(NKWebKit::GuestDecompressPtr cinfo, int32_t desired) {
    return jpeg_resync_to_restart_nid_postfix(cinfo, desired);
}

static void APS5_VABI GuestTermSource(NKWebKit::GuestDecompressPtr cinfo) {
    (void)cinfo;
}

struct Diagnostic {
    int code;
    int64_t warnings;
    bool operator==(const Diagnostic&) const = default;
};

void HostDiagnostic(j_common_ptr cinfo) {
    auto& messages = *static_cast<std::vector<Diagnostic>*>(cinfo->client_data);
    messages.push_back({cinfo->err->msg_code, cinfo->err->num_warnings});
}

void APS5_VABI GuestDiagnostic(NKWebKit::GuestCommonPtr cinfo) {
    auto& messages = *static_cast<std::vector<Diagnostic>*>(cinfo->client_data);
    messages.push_back({cinfo->err->msg_code, cinfo->err->num_warnings});
}

void HostPromotingDiagnostic(j_common_ptr cinfo) {
    HostDiagnostic(cinfo);
    if (cinfo->err->msg_code == JWRN_EXTRANEOUS_DATA) cinfo->err->trace_level = 3;
}

void APS5_VABI GuestPromotingDiagnostic(NKWebKit::GuestCommonPtr cinfo) {
    GuestDiagnostic(cinfo);
    if (cinfo->err->msg_code == JWRN_EXTRANEOUS_DATA) cinfo->err->trace_level = 3;
}

void TestDiagnosticDispatch() {
    bool matched = true;
    for (const int traceLevel : {-1, 0, 1, 2, 3, 6}) {
        std::vector<Diagnostic> hostMessages;
        std::vector<Diagnostic> guestMessages;
        jpeg_error_mgr hostError{};
        jpeg_std_error(&hostError);
        hostError.output_message = HostDiagnostic;
        hostError.trace_level = traceLevel;
        jpeg_common_struct host{};
        host.err = &hostError;
        host.client_data = &hostMessages;
        NKWebKit::GuestJpegErrorMgr guestError{};
        jpeg_std_error_nid_postfix(&guestError);
        guestError.output_message = GuestDiagnostic;
        guestError.trace_level = traceLevel;
        NKWebKit::GuestJpegCommon guest{};
        guest.err = &guestError;
        guest.client_data = &guestMessages;
        for (const int messageLevel : {-2, -1, 0, 1, 2, 3, 7, -1, -2, 0}) {
            hostError.msg_code = guestError.msg_code = messageLevel < 0 ? JWRN_EXTRANEOUS_DATA : JTRC_SOI;
            hostError.emit_message(&host, messageLevel);
            guestError.emit_message(&guest, messageLevel);
            AssertTrue(guestError.num_warnings == hostError.num_warnings, "diagnostic warning counts differ from libjpeg");
        }
        if (guestMessages != hostMessages) {
            std::fprintf(stderr, "JPEG trace level %d: expected %zu messages, observed %zu\n", traceLevel, hostMessages.size(), guestMessages.size());
            matched = false;
        }
        hostError.reset_error_mgr(&host);
        guestError.reset_error_mgr(&guest);
        AssertTrue(guestError.num_warnings == hostError.num_warnings && guestError.msg_code == hostError.msg_code,
            "diagnostic reset differs from libjpeg");
        AssertTrue(guestError.trace_level == hostError.trace_level, "diagnostic reset changed the trace level");
    }
    AssertTrue(matched, "JPEG warning and trace dispatch differs from libjpeg");
}

void TestHeaderDiagnostics(int traceLevel, bool defaultOutput, bool promote = false) {
    std::vector<uint8_t> jpeg;
    GenerateJpeg(jpeg, 16, 16, 3, JCS_RGB, false, 0, nullptr, 0);
    jpeg.insert(jpeg.begin() + 2, 0x42);
    std::vector<Diagnostic> hostMessages;
    jpeg_error_mgr hostError{};
    jpeg_decompress_struct host{};
    host.err = jpeg_std_error(&hostError);
    jpeg_create_decompress(&host);
    host.client_data = &hostMessages;
    hostError.output_message = promote ? HostPromotingDiagnostic : HostDiagnostic;
    hostError.trace_level = traceLevel;
    jpeg_mem_src(&host, jpeg.data(), jpeg.size());
    AssertTrue(jpeg_read_header(&host, TRUE) == JPEG_HEADER_OK, "host warning fixture header failed");

    std::vector<Diagnostic> guestMessages;
    NKWebKit::GuestJpegErrorMgr guestError{};
    jpeg_std_error_nid_postfix(&guestError);
    NKWebKit::GuestJpegDecompress guest{};
    guest.err = &guestError;
    jpeg_CreateDecompress_nid_postfix(&guest, 62, sizeof(guest));
    guest.client_data = &guestMessages;
    guestError.trace_level = traceLevel;
    if (!defaultOutput) guestError.output_message = promote ? GuestPromotingDiagnostic : GuestDiagnostic;
    NKWebKit::GuestJpegSourceMgr source{};
    source.next_input_byte = jpeg.data();
    source.bytes_in_buffer = jpeg.size();
    source.init_source = GuestInitSource;
    source.fill_input_buffer = GuestFillInputBuffer;
    source.skip_input_data = GuestSkipInputData;
    source.resync_to_restart = GuestResyncToRestart;
    source.term_source = GuestTermSource;
    guest.src = &source;
    AssertTrue(jpeg_read_header_nid_postfix(&guest, 1) == JPEG_HEADER_OK, "guest warning fixture header failed");
    AssertTrue(guest.image_width == host.image_width && guest.image_height == host.image_height,
        "warning fixture dimensions differ from libjpeg");
    AssertTrue(hostError.num_warnings == 1 && guestError.num_warnings == hostError.num_warnings,
        "warning fixture did not count its extraneous header byte");
    AssertTrue(guestError.trace_level == hostError.trace_level, "header diagnostic callback trace levels differ");
    if (!defaultOutput && guestMessages != hostMessages) {
        std::fprintf(stderr, "JPEG header trace level %d: expected %zu messages, observed %zu\n", traceLevel, hostMessages.size(), guestMessages.size());
        AssertTrue(false, "header diagnostics differ from libjpeg");
    }
    jpeg_destroy_decompress_nid_postfix(&guest);
    jpeg_destroy_decompress(&host);
}

void TestDecode(const std::vector<uint8_t>& jpegData, const uint8_t* expectedMarkerData, size_t expectedMarkerLen,
    int inputColorSpace = -1, int outputColorSpace = -1) {
    int hostWidth = 0;
    int hostHeight = 0;
    int hostComps = 0;
    std::vector<uint8_t> hostPixels = DecompressHost(jpegData, hostWidth, hostHeight, hostComps, inputColorSpace, outputColorSpace);

    NKWebKit::GuestJpegErrorMgr jerr{};
    auto* errRet = jpeg_std_error_nid_postfix(&jerr);
    AssertTrue(errRet == &jerr, "jpeg_std_error returned incorrect pointer");

    NKWebKit::GuestJpegDecompress guest_cinfo{};
    guest_cinfo.err = &jerr;
    jpeg_CreateDecompress_nid_postfix(&guest_cinfo, 62, sizeof(NKWebKit::GuestJpegDecompress));

    NKWebKit::GuestJpegSourceMgr srcMgr{};
    srcMgr.next_input_byte = jpegData.data();
    srcMgr.bytes_in_buffer = jpegData.size();
    srcMgr.init_source = GuestInitSource;
    srcMgr.fill_input_buffer = GuestFillInputBuffer;
    srcMgr.skip_input_data = GuestSkipInputData;
    srcMgr.resync_to_restart = GuestResyncToRestart;
    srcMgr.term_source = GuestTermSource;
    guest_cinfo.src = &srcMgr;

    if (expectedMarkerData && expectedMarkerLen > 0) {
        jpeg_save_markers_nid_postfix(&guest_cinfo, 0xE1, 1000);
    }

    int headerRet = jpeg_read_header_nid_postfix(&guest_cinfo, 1);
    AssertTrue(headerRet == 1, "jpeg_read_header failed");
    if (inputColorSpace >= 0) guest_cinfo.jpeg_color_space = inputColorSpace;
    if (outputColorSpace >= 0) guest_cinfo.out_color_space = outputColorSpace;

    jpeg_calc_output_dimensions_nid_postfix(&guest_cinfo);
    AssertTrue(guest_cinfo.output_width == static_cast<uint32_t>(hostWidth), "width mismatch");
    AssertTrue(guest_cinfo.output_height == static_cast<uint32_t>(hostHeight), "height mismatch");
    AssertTrue(guest_cinfo.output_components == hostComps, "components mismatch");

    if (expectedMarkerData && expectedMarkerLen > 0) {
        AssertTrue(guest_cinfo.marker_list != nullptr, "marker_list is null");
        AssertTrue(guest_cinfo.marker_list->marker == 0xE1, "marker code mismatch");
        AssertTrue(guest_cinfo.marker_list->data_length == expectedMarkerLen, "marker data_length mismatch");
        AssertTrue(std::memcmp(guest_cinfo.marker_list->data, expectedMarkerData, expectedMarkerLen) == 0, "marker content mismatch");
    }

    int startRet = jpeg_start_decompress_nid_postfix(&guest_cinfo);
    AssertTrue(startRet == 1, "jpeg_start_decompress failed");

    std::vector<uint8_t> guestPixels(hostWidth * hostHeight * hostComps);
    while (guest_cinfo.output_scanline < guest_cinfo.output_height) {
        uint8_t* rowPtr = guestPixels.data() + guest_cinfo.output_scanline * hostWidth * hostComps;
        uint32_t lines = jpeg_read_scanlines_nid_postfix(&guest_cinfo, &rowPtr, 1);
        AssertTrue(lines == 1, "jpeg_read_scanlines returned 0");
    }

    int finishRet = jpeg_finish_decompress_nid_postfix(&guest_cinfo);
    AssertTrue(finishRet == 1, "jpeg_finish_decompress failed");

    jpeg_destroy_decompress_nid_postfix(&guest_cinfo);

    AssertTrue(jerr.num_warnings == 0, "the decode should not warn about corrupt data");
    AssertTrue(guestPixels == hostPixels, "decoded pixels differ from host libjpeg output");
    if (inputColorSpace >= 0) std::printf("JPEG input/output color spaces %d/%d match native pixels.\n", inputColorSpace, outputColorSpace);
}

void TestProgressiveBuffered(const std::vector<uint8_t>& jpegData) {
    NKWebKit::GuestJpegErrorMgr jerr{};
    jpeg_std_error_nid_postfix(&jerr);

    NKWebKit::GuestJpegDecompress cinfo{};
    cinfo.err = &jerr;
    jpeg_CreateDecompress_nid_postfix(&cinfo, 62, sizeof(NKWebKit::GuestJpegDecompress));

    NKWebKit::GuestJpegSourceMgr srcMgr{};
    srcMgr.next_input_byte = jpegData.data();
    srcMgr.bytes_in_buffer = jpegData.size();
    srcMgr.init_source = GuestInitSource;
    srcMgr.fill_input_buffer = GuestFillInputBuffer;
    srcMgr.skip_input_data = GuestSkipInputData;
    srcMgr.resync_to_restart = GuestResyncToRestart;
    srcMgr.term_source = GuestTermSource;
    cinfo.src = &srcMgr;

    int ret = jpeg_read_header_nid_postfix(&cinfo, 1);
    AssertTrue(ret == 1, "read header failed");

    cinfo.buffered_image = 1;
    ret = jpeg_start_decompress_nid_postfix(&cinfo);
    AssertTrue(ret == 1, "start decompress failed");

    AssertTrue(jpeg_has_multiple_scans_nid_postfix(&cinfo) == 1, "expected progressive multiple scans");

    while (jpeg_input_complete_nid_postfix(&cinfo) == 0) {
        jpeg_consume_input_nid_postfix(&cinfo);
    }

    std::vector<uint8_t> buf(cinfo.output_width * cinfo.output_components);
    uint8_t* rowPtr = buf.data();

    ret = jpeg_start_output_nid_postfix(&cinfo, cinfo.input_scan_number);
    AssertTrue(ret == 1, "start output failed");

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines_nid_postfix(&cinfo, &rowPtr, 1);
    }

    ret = jpeg_finish_output_nid_postfix(&cinfo);
    AssertTrue(ret == 1, "finish output failed");

    jpeg_finish_decompress_nid_postfix(&cinfo);
    jpeg_destroy_decompress_nid_postfix(&cinfo);
}

bool TestColormap(const std::vector<uint8_t>& jpegData, int colors) {
    uint8_t palette[3][256]{};
    uint8_t* rows[] = {palette[0], palette[1], palette[2]};
    for (int index = 0; index < colors; ++index) {
        palette[0][index] = static_cast<uint8_t>(index * 47 + 17);
        palette[1][index] = static_cast<uint8_t>(index * 89 + 37);
        palette[2][index] = static_cast<uint8_t>(index * 137 + 71);
    }
    jpeg_decompress_struct host{};
    jpeg_error_mgr hostError{};
    host.err = jpeg_std_error(&hostError);
    jpeg_create_decompress(&host);
    jpeg_mem_src(&host, jpegData.data(), jpegData.size());
    AssertTrue(jpeg_read_header(&host, TRUE) == JPEG_HEADER_OK, "native colormap header");
    host.quantize_colors = TRUE;
    host.desired_number_of_colors = 16;
    host.dither_mode = JDITHER_NONE;
    host.colormap = colors ? rows : nullptr;
    host.actual_number_of_colors = colors;
    jpeg_calc_output_dimensions(&host);
    AssertTrue(jpeg_start_decompress(&host) == TRUE && host.output_components == 1, "native colormap start");
    const auto width = host.output_width;
    const auto height = host.output_height;
    const int actualColors = host.actual_number_of_colors;
    std::vector<uint8_t> expected(static_cast<size_t>(width) * height);
    while (host.output_scanline < height) {
        JSAMPROW row = expected.data() + host.output_scanline * width;
        AssertTrue(jpeg_read_scanlines(&host, &row, 1) == 1, "native colormap scanline");
    }
    AssertTrue(jpeg_finish_decompress(&host) == TRUE, "native colormap finish");
    jpeg_destroy_decompress(&host);

    NKWebKit::GuestJpegErrorMgr guestError{};
    NKWebKit::GuestJpegDecompress guest{};
    guest.err = jpeg_std_error_nid_postfix(&guestError);
    jpeg_CreateDecompress_nid_postfix(&guest, 62, sizeof(guest));
    NKWebKit::GuestJpegSourceMgr source{};
    source.next_input_byte = jpegData.data();
    source.bytes_in_buffer = jpegData.size();
    source.init_source = GuestInitSource;
    source.fill_input_buffer = GuestFillInputBuffer;
    source.skip_input_data = GuestSkipInputData;
    source.resync_to_restart = GuestResyncToRestart;
    source.term_source = GuestTermSource;
    guest.src = &source;
    bool matches = false;
    try {
        AssertTrue(jpeg_read_header_nid_postfix(&guest, 1) == JPEG_HEADER_OK, "guest colormap header");
        guest.quantize_colors = 1;
        guest.desired_number_of_colors = 16;
        guest.dither_mode = JDITHER_NONE;
        guest.colormap = colors ? rows : nullptr;
        guest.actual_number_of_colors = colors;
        jpeg_calc_output_dimensions_nid_postfix(&guest);
        AssertTrue(jpeg_start_decompress_nid_postfix(&guest) == 1, "guest colormap start");
        AssertTrue(guest.output_width == width && guest.output_height == height && guest.output_components == 1,
                   "guest colormap dimensions or components differ");
        std::vector<uint8_t> actual(expected.size());
        while (guest.output_scanline < height) {
            uint8_t* row = actual.data() + guest.output_scanline * width;
            AssertTrue(jpeg_read_scanlines_nid_postfix(&guest, &row, 1) == 1, "guest colormap scanline");
        }
        matches = actual == expected && guest.actual_number_of_colors == actualColors;
        AssertTrue(jpeg_finish_decompress_nid_postfix(&guest) == 1, "guest colormap finish");
    } catch (const std::runtime_error& error) {
        std::fprintf(stderr, "JPEG colormap %d: %s\n", colors, error.what());
    }
    jpeg_destroy_decompress_nid_postfix(&guest);
    std::printf("JPEG colormap requested=%d native=%d match=%d\n", colors, actualColors, matches);
    return matches;
}

bool TestColormaps() {
    int passed = 0;
    for (const bool progressive : {false, true}) {
        std::vector<uint8_t> jpeg;
        GenerateJpeg(jpeg, 17, 13, 3, JCS_RGB, progressive, 0, nullptr, 0);
        for (const int colors : {0, 1, 2, 4, 7, 16, 256}) {
            if (TestColormap(jpeg, colors)) ++passed;
        }
    }
    std::printf("JPEG colormap native-reference cases: %d/14\n", passed);
    return passed == 14;
}

int g_errorExitCalls = 0;
int g_errorExitCode = -1;
int g_errorExitParms[2] = {-1, -1};

void APS5_VABI RecordingErrorExit(NKWebKit::GuestCommonPtr cinfo) {
    ++g_errorExitCalls;
    g_errorExitCode = cinfo->err->msg_code;
    g_errorExitParms[0] = cinfo->err->msg_parm.i[0];
    g_errorExitParms[1] = cinfo->err->msg_parm.i[1];
}

struct SamplingResult {
    bool aligned;
    int rows;
    int started;
    int errors;
    int code;
    bool operator==(const SamplingResult&) const = default;
};

struct SamplingJump {
    std::jmp_buf jump;
    SamplingResult* result;
};

void NativeSamplingError(j_common_ptr info) {
    auto* context = static_cast<SamplingJump*>(info->client_data);
    ++context->result->errors;
    context->result->code = info->err->msg_code;
    std::longjmp(context->jump, 1);
}

int StartNativeSampling(j_decompress_ptr info, SamplingResult* result) {
    SamplingJump context{};
    context.result = result;
    info->client_data = &context;
    info->err->error_exit = NativeSamplingError;
    if (setjmp(context.jump)) {
        info->client_data = nullptr;
        return 0;
    }
    const int started = jpeg_start_decompress(info);
    info->client_data = nullptr;
    return started;
}

SamplingResult NativeSampling(const std::vector<uint8_t>& image, int aligned, bool raw, bool fancy) {
    jpeg_decompress_struct info{};
    jpeg_error_mgr error{};
    info.err = jpeg_std_error(&error);
    jpeg_create_decompress(&info);
    jpeg_mem_src(&info, image.data(), image.size());
    AssertTrue(jpeg_read_header(&info, TRUE) == JPEG_HEADER_OK, "native sampling header failed");
    info.CCIR601_sampling = aligned;
    info.raw_data_out = raw;
    info.do_fancy_upsampling = fancy;
    jpeg_calc_output_dimensions(&info);
    SamplingResult result{info.CCIR601_sampling != 0, info.rec_outbuf_height, 0, 0, 0};
    result.started = StartNativeSampling(&info, &result);
    jpeg_destroy_decompress(&info);
    return result;
}

SamplingResult GuestSampling(const std::vector<uint8_t>& image, int aligned, bool raw, bool fancy) {
    NKWebKit::GuestJpegDecompress info{};
    NKWebKit::GuestJpegErrorMgr error{};
    info.err = jpeg_std_error_nid_postfix(&error);
    error.error_exit = RecordingErrorExit;
    jpeg_CreateDecompress_nid_postfix(&info, 62, sizeof(info));
    NKWebKit::GuestJpegSourceMgr source{};
    source.next_input_byte = image.data();
    source.bytes_in_buffer = image.size();
    source.init_source = GuestInitSource;
    source.fill_input_buffer = GuestFillInputBuffer;
    source.skip_input_data = GuestSkipInputData;
    source.resync_to_restart = GuestResyncToRestart;
    source.term_source = GuestTermSource;
    info.src = &source;
    AssertTrue(jpeg_read_header_nid_postfix(&info, 1) == JPEG_HEADER_OK, "guest sampling header failed");
    info.CCIR601_sampling = aligned;
    info.raw_data_out = raw;
    info.do_fancy_upsampling = fancy;
    jpeg_calc_output_dimensions_nid_postfix(&info);
    SamplingResult result{info.CCIR601_sampling != 0, info.rec_outbuf_height, 0, 0, 0};
    g_errorExitCalls = 0;
    g_errorExitCode = 0;
    result.started = jpeg_start_decompress_nid_postfix(&info);
    result.errors = g_errorExitCalls;
    result.code = g_errorExitCode;
    jpeg_destroy_decompress_nid_postfix(&info);
    return result;
}

void TestSamplingAlignment() {
    int passed = 0;
    for (const bool progressive : {false, true}) {
        for (const bool color : {false, true}) {
            std::vector<uint8_t> image;
            GenerateJpeg(image, 32, 32, color ? 3 : 1, color ? JCS_RGB : JCS_GRAYSCALE, progressive, 0, nullptr, 0);
            for (const bool raw : {false, true}) {
                for (const bool fancy : {false, true}) {
                    for (const int aligned : {0, 1, 2, -1}) {
                        const auto native = NativeSampling(image, aligned, raw, fancy);
                        const auto guest = GuestSampling(image, aligned, raw, fancy);
                        AssertTrue(native.errors == (aligned != 0 && !raw), "native sampling refusal control failed");
                        AssertTrue(!native.errors || native.code == JERR_CCIR601_NOTIMPL, "native sampling error differs");
                        if (native == guest) ++passed;
                        else std::fprintf(stderr, "Sampling mismatch: progressive=%d color=%d raw=%d fancy=%d aligned=%d native_rows=%d guest_rows=%d native_error=%d guest_error=%d\n", progressive, color, raw, fancy, aligned, native.rows, guest.rows, native.code, guest.code);
                    }
                }
            }
        }
    }
    std::fprintf(stderr, "JPEG sampling alignment: %d/64 native comparisons passed\n", passed);
    AssertTrue(passed == 64, "sampling alignment differs from native libjpeg");
}

void TestBadStructSize() {
    NKWebKit::GuestJpegErrorMgr jerr{};
    jpeg_std_error_nid_postfix(&jerr);
    jerr.error_exit = RecordingErrorExit;
    NKWebKit::GuestJpegDecompress guest_cinfo{};
    guest_cinfo.err = &jerr;
    g_errorExitCalls = 0;
    const std::size_t wrong = sizeof(NKWebKit::GuestJpegDecompress) - 8;
    jpeg_CreateDecompress_nid_postfix(&guest_cinfo, 62, wrong);
    AssertTrue(g_errorExitCalls == 1, "a wrong structsize should call the guest error_exit once");
    AssertTrue(g_errorExitCode == JERR_BAD_STRUCT_SIZE, "a wrong structsize should raise JERR_BAD_STRUCT_SIZE");
    AssertTrue(g_errorExitParms[0] == static_cast<int>(sizeof(NKWebKit::GuestJpegDecompress)), "JERR_BAD_STRUCT_SIZE should report the expected size first");
    AssertTrue(g_errorExitParms[1] == static_cast<int>(wrong), "JERR_BAD_STRUCT_SIZE should report the passed size second");
    bool rejected = false;
    try {
        jpeg_read_header_nid_postfix(&guest_cinfo, 1);
    } catch (const std::invalid_argument&) {
        rejected = true;
    }
    AssertTrue(rejected, "an object whose creation failed should not be usable");
    jpeg_destroy_decompress_nid_postfix(&guest_cinfo);
    g_errorExitCalls = 0;
}

void TestGuestErrorExit() {
    const uint8_t notJpeg[] = {'N', 'O', 'T', ' ', 'J', 'P', 'E', 'G', 0, 0, 0, 0};
    NKWebKit::GuestJpegErrorMgr jerr{};
    jpeg_std_error_nid_postfix(&jerr);
    jerr.error_exit = RecordingErrorExit;

    NKWebKit::GuestJpegDecompress guest_cinfo{};
    guest_cinfo.err = &jerr;
    jpeg_CreateDecompress_nid_postfix(&guest_cinfo, 62, sizeof(NKWebKit::GuestJpegDecompress));

    NKWebKit::GuestJpegSourceMgr srcMgr{};
    srcMgr.next_input_byte = notJpeg;
    srcMgr.bytes_in_buffer = sizeof(notJpeg);
    srcMgr.init_source = GuestInitSource;
    srcMgr.fill_input_buffer = GuestFillInputBuffer;
    srcMgr.skip_input_data = GuestSkipInputData;
    srcMgr.resync_to_restart = GuestResyncToRestart;
    srcMgr.term_source = GuestTermSource;
    guest_cinfo.src = &srcMgr;

    AssertTrue(jpeg_read_header_nid_postfix(&guest_cinfo, 1) == 0, "jpeg_read_header should end early after error_exit returns");
    AssertTrue(g_errorExitCalls == 1, "the guest error_exit should be called once");
    AssertTrue(g_errorExitCode == JERR_NO_SOI, "the guest error_exit should see JERR_NO_SOI");
    jpeg_destroy_decompress_nid_postfix(&guest_cinfo);
}

void TestDefaultErrorExitThrows() {
    const uint8_t notJpeg[] = {'N', 'O', 'T', ' ', 'J', 'P', 'E', 'G', 0, 0, 0, 0};
    NKWebKit::GuestJpegErrorMgr jerr{};
    jpeg_std_error_nid_postfix(&jerr);

    NKWebKit::GuestJpegDecompress guest_cinfo{};
    guest_cinfo.err = &jerr;
    jpeg_CreateDecompress_nid_postfix(&guest_cinfo, 62, sizeof(NKWebKit::GuestJpegDecompress));

    NKWebKit::GuestJpegSourceMgr srcMgr{};
    srcMgr.next_input_byte = notJpeg;
    srcMgr.bytes_in_buffer = sizeof(notJpeg);
    srcMgr.init_source = GuestInitSource;
    srcMgr.fill_input_buffer = GuestFillInputBuffer;
    srcMgr.skip_input_data = GuestSkipInputData;
    srcMgr.resync_to_restart = GuestResyncToRestart;
    srcMgr.term_source = GuestTermSource;
    guest_cinfo.src = &srcMgr;

    bool threw = false;
    try {
        jpeg_read_header_nid_postfix(&guest_cinfo, 1);
    } catch (const std::runtime_error& error) {
        threw = std::strstr(error.what(), "libjpeg: ") == error.what() && std::strlen(error.what()) > std::strlen("libjpeg: ");
    }
    AssertTrue(threw, "the default error_exit should throw the formatted libjpeg message");
    AssertTrue(jerr.msg_code == JERR_NO_SOI, "the default error_exit should see JERR_NO_SOI");
    jpeg_destroy_decompress_nid_postfix(&guest_cinfo);
}

} // namespace

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--diagnostic-output") == 0) {
        TestHeaderDiagnostics(0, true);
        std::puts("JPEG diagnostic output path passed");
        return 0;
    }
    TestDiagnosticDispatch();
    for (const int traceLevel : {0, 1, 3}) TestHeaderDiagnostics(traceLevel, false);
    TestHeaderDiagnostics(0, false, true);
    TestBadStructSize();
    TestSamplingAlignment();
    TestGuestErrorExit();
    TestDefaultErrorExitThrows();
    if (!TestColormaps()) return 1;
    std::vector<uint8_t> baselineJpeg;
    const uint8_t app1Data[] = "EXIF_TEST_MARKER_PAYLOAD";
    GenerateJpeg(baselineJpeg, 16, 16, 3, JCS_RGB, false, 0, app1Data, sizeof(app1Data));
    TestDecode(baselineJpeg, app1Data, sizeof(app1Data));

    std::vector<uint8_t> skippedPayload(96, 'A');
    skippedPayload[skippedPayload.size() - 12] = 0xFF;
    skippedPayload[skippedPayload.size() - 11] = 0xD9;
    std::vector<uint8_t> skippedJpeg;
    GenerateJpeg(skippedJpeg, 16, 16, 3, JCS_RGB, false, 0, skippedPayload.data(), skippedPayload.size());
    TestDecode(skippedJpeg, nullptr, 0);

    std::vector<uint8_t> grayJpeg;
    GenerateJpeg(grayJpeg, 16, 16, 1, JCS_GRAYSCALE, false, 0, nullptr, 0);
    TestDecode(grayJpeg, nullptr, 0);

    std::vector<uint8_t> restartJpeg;
    GenerateJpeg(restartJpeg, 32, 32, 3, JCS_RGB, false, 2, nullptr, 0);
    TestDecode(restartJpeg, nullptr, 0);

    std::vector<uint8_t> progJpeg;
    GenerateJpeg(progJpeg, 32, 32, 3, JCS_RGB, true, 0, nullptr, 0);
    TestDecode(progJpeg, nullptr, 0);
    TestProgressiveBuffered(progJpeg);

    TestDecode(baselineJpeg, nullptr, 0, JCS_RGB, JCS_RGB);
    TestDecode(baselineJpeg, nullptr, 0, JCS_YCbCr, JCS_RGB);
    TestDecode(baselineJpeg, nullptr, 0, JCS_UNKNOWN, JCS_UNKNOWN);
    TestDecode(grayJpeg, nullptr, 0, JCS_UNKNOWN, JCS_UNKNOWN);
    TestDecode(progJpeg, nullptr, 0, JCS_RGB, JCS_RGB);
    std::vector<uint8_t> cmykJpeg;
    GenerateJpeg(cmykJpeg, 17, 13, 4, JCS_CMYK, false, 0, nullptr, 0);
    TestDecode(cmykJpeg, nullptr, 0);
    TestDecode(cmykJpeg, nullptr, 0, JCS_YCCK, JCS_CMYK);
    TestDecode(cmykJpeg, nullptr, 0, JCS_UNKNOWN, JCS_UNKNOWN);

    std::printf("Jpeg tests passed.\n");
    return 0;
}
