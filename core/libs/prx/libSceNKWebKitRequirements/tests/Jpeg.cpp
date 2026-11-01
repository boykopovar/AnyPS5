#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

extern "C" {
#include <jpeglib.h>
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

std::vector<uint8_t> DecompressHost(const std::vector<uint8_t>& jpegData, int& outWidth, int& outHeight, int& outComps) {
    jpeg_decompress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, jpegData.data(), jpegData.size());
    jpeg_read_header(&cinfo, TRUE);
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

void TestDecode(const std::vector<uint8_t>& jpegData, const uint8_t* expectedMarkerData, size_t expectedMarkerLen) {
    int hostWidth = 0;
    int hostHeight = 0;
    int hostComps = 0;
    std::vector<uint8_t> hostPixels = DecompressHost(jpegData, hostWidth, hostHeight, hostComps);

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

    AssertTrue(guestPixels == hostPixels, "decoded pixels differ from host libjpeg output");
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

} // namespace

int main() {
    std::vector<uint8_t> baselineJpeg;
    const uint8_t app1Data[] = "EXIF_TEST_MARKER_PAYLOAD";
    GenerateJpeg(baselineJpeg, 16, 16, 3, JCS_RGB, false, 0, app1Data, sizeof(app1Data));
    TestDecode(baselineJpeg, app1Data, sizeof(app1Data));

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

    std::printf("Jpeg tests passed.\n");
    return 0;
}
