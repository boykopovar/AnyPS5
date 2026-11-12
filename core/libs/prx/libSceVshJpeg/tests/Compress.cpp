#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"
#include "prx/libSceVshJpeg/include/Compress.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <stdexcept>
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

struct Sink {
    std::vector<std::uint8_t> bytes;
    std::vector<std::uint8_t> chunk;
    int inits = 0;
    int empties = 0;
    int terms = 0;
};

Sink* g_sink = nullptr;

void ArmChunk(VshJpeg::GuestCompressPtr cinfo) {
    cinfo->dest->next_output_byte = g_sink->chunk.data();
    cinfo->dest->free_in_buffer = g_sink->chunk.size();
}

void APS5_VABI SinkInit(VshJpeg::GuestCompressPtr cinfo) {
    ++g_sink->inits;
    ArmChunk(cinfo);
}

std::int32_t APS5_VABI SinkEmpty(VshJpeg::GuestCompressPtr cinfo) {
    ++g_sink->empties;
    g_sink->bytes.insert(g_sink->bytes.end(), g_sink->chunk.begin(), g_sink->chunk.end());
    ArmChunk(cinfo);
    return 1;
}

void APS5_VABI SinkTerm(VshJpeg::GuestCompressPtr cinfo) {
    ++g_sink->terms;
    const std::size_t used = g_sink->chunk.size() - cinfo->dest->free_in_buffer;
    g_sink->bytes.insert(g_sink->bytes.end(), g_sink->chunk.begin(), g_sink->chunk.begin() + used);
}

struct Params {
    int width;
    int height;
    int components;
    J_COLOR_SPACE space;
    int quality;
    bool baseline;
    bool optimize;
    unsigned restart;
    std::size_t chunk;
    int tweak = 0;
};

JQUANT_TBL g_ownQuant;
jpeg_component_info g_ownComp[4];

template <typename Info>
void ApplyTweak(Info& c, int tweak, bool replace) {
    auto* comp = static_cast<jpeg_component_info*>(c.comp_info);
    auto* quant = static_cast<JQUANT_TBL*>(c.quant_tbl_ptrs[0]);
    switch (tweak) {
    case 1:
        comp[0].h_samp_factor = 1;
        comp[0].v_samp_factor = 1;
        break;
    case 2:
        c.arith_code = 1;
        c.arith_dc_L[0] = 1;
        c.arith_dc_U[0] = 4;
        c.arith_ac_K[0] = 8;
        break;
    case 3:
        quant->quantval[1] = 3;
        quant->quantval[5] = 40;
        break;
    case 4:
        if (replace) {
            g_ownQuant = *quant;
            g_ownQuant.quantval[2] = 7;
            c.quant_tbl_ptrs[0] = &g_ownQuant;
        } else {
            quant->quantval[2] = 7;
        }
        break;
    case 5:
        if (replace) {
            for (int i = 0; i < c.num_components; ++i) g_ownComp[i] = comp[i];
            g_ownComp[0].h_samp_factor = 1;
            g_ownComp[0].v_samp_factor = 1;
            c.comp_info = g_ownComp;
        } else {
            comp[0].h_samp_factor = 1;
            comp[0].v_samp_factor = 1;
        }
        break;
    default:
        break;
    }
}

std::vector<std::uint8_t> Pixels(const Params& p) {
    std::vector<std::uint8_t> v(static_cast<std::size_t>(p.width) * p.height * p.components);
    for (std::size_t i = 0; i < v.size(); ++i) v[i] = static_cast<std::uint8_t>((i * 37 + (i / 7) * 11) & 0xFF);
    return v;
}

std::vector<std::uint8_t> HostEncode(const Params& p, const std::vector<std::uint8_t>& pixels) {
    jpeg_compress_struct cinfo{};
    jpeg_error_mgr jerr{};
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    unsigned char* buffer = nullptr;
    unsigned long size = 0;
    jpeg_mem_dest(&cinfo, &buffer, &size);
    cinfo.image_width = p.width;
    cinfo.image_height = p.height;
    cinfo.input_components = p.components;
    cinfo.in_color_space = p.space;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, p.quality, p.baseline ? TRUE : FALSE);
    ApplyTweak(cinfo, p.tweak, false);
    cinfo.optimize_coding = p.optimize ? TRUE : FALSE;
    cinfo.restart_interval = p.restart;
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = const_cast<JSAMPROW>(pixels.data() + static_cast<std::size_t>(cinfo.next_scanline) * p.width * p.components);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    std::vector<std::uint8_t> out(buffer, buffer + size);
    std::free(buffer);
    return out;
}

std::vector<std::uint8_t> GuestEncode(const Params& p, const std::vector<std::uint8_t>& pixels, Sink& sink) {
    g_sink = &sink;
    sink.chunk.assign(p.chunk, 0);
    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegDestinationMgr dest{};
    dest.init_destination = SinkInit;
    dest.empty_output_buffer = SinkEmpty;
    dest.term_destination = SinkTerm;
    VshJpeg::GuestJpegCompress cinfo{};
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    AssertTrue(cinfo.is_decompressor == 0, "guest object is a compressor");
    AssertTrue(cinfo.global_state == 100, "CSTATE_START after create");
    cinfo.dest = &dest;
    cinfo.image_width = p.width;
    cinfo.image_height = p.height;
    cinfo.input_components = p.components;
    cinfo.in_color_space = p.space;
    jpeg_set_defaults_nid_postfix(&cinfo);
    AssertTrue(cinfo.num_components == (p.components == 1 ? 1 : p.components == 4 ? 4 : 3), "defaults set the component count");
    AssertTrue(cinfo.data_precision == 8, "defaults set 8-bit precision");
    AssertTrue(cinfo.comp_info != nullptr, "defaults allocate component info");
    jpeg_set_quality_nid_postfix(&cinfo, p.quality, p.baseline ? 1 : 0);
    AssertTrue(cinfo.quant_tbl_ptrs[0] != nullptr, "quality builds the first quantization table");
    ApplyTweak(cinfo, p.tweak, true);
    cinfo.optimize_coding = p.optimize ? 1 : 0;
    cinfo.restart_interval = p.restart;
    jpeg_start_compress_nid_postfix(&cinfo, 1);
    AssertTrue(sink.inits == 1, "init_destination ran once");
    AssertTrue(cinfo.global_state == 101, "CSTATE_SCANNING after start");
    while (cinfo.next_scanline < cinfo.image_height) {
        std::uint8_t* row = const_cast<std::uint8_t*>(pixels.data() + static_cast<std::size_t>(cinfo.next_scanline) * p.width * p.components);
        AssertTrue(jpeg_write_scanlines_nid_postfix(&cinfo, &row, 1) == 1, "one scanline written");
    }
    jpeg_finish_compress_nid_postfix(&cinfo);
    AssertTrue(sink.terms == 1, "term_destination ran once");
    AssertTrue(cinfo.global_state == 100, "state is back to start after finish");
    jpeg_destroy_compress_nid_postfix(&cinfo);
    return sink.bytes;
}

void CompareWithHost(const Params& p, const char* name) {
    const auto pixels = Pixels(p);
    const auto expected = HostEncode(p, pixels);
    Sink sink;
    const auto actual = GuestEncode(p, pixels, sink);
    if (expected != actual) {
        std::fprintf(stderr, "%s: guest %zu bytes, host %zu bytes\n", name, actual.size(), expected.size());
        AssertTrue(false, "guest stream equals the host libjpeg stream");
    }
    if (p.chunk < expected.size()) AssertTrue(sink.empties > 0, "a small chunk forces empty_output_buffer");
    jpeg_decompress_struct d{};
    jpeg_error_mgr jerr{};
    d.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&d);
    jpeg_mem_src(&d, actual.data(), actual.size());
    AssertTrue(jpeg_read_header(&d, TRUE) == JPEG_HEADER_OK, "host reads the guest stream");
    AssertTrue(static_cast<int>(d.image_width) == p.width && static_cast<int>(d.image_height) == p.height, "dimensions round trip");
    jpeg_destroy_decompress(&d);
    std::printf("compress %s: %zu bytes, %d empty_output_buffer calls\n", name, actual.size(), sink.empties);
}

int g_errorCode = 0;
int g_errorCalls = 0;

void APS5_VABI RecordingErrorExit(NKWebKit::GuestCommonPtr cinfo) {
    ++g_errorCalls;
    g_errorCode = cinfo->err->msg_code;
}

void ErrorForwarding() {
    NKWebKit::GuestJpegErrorMgr err{};
    {
        VshJpeg::GuestJpegCompress cinfo{};
        cinfo.err = jpeg_std_error_nid_postfix(&err);
        err.error_exit = RecordingErrorExit;
        jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo) - 8);
        AssertTrue(g_errorCalls == 1 && g_errorCode == JERR_BAD_STRUCT_SIZE, "a wrong structure size reaches the guest error_exit");
        AssertTrue(err.msg_parm.i[0] == static_cast<int>(sizeof(cinfo)) && err.msg_parm.i[1] == static_cast<int>(sizeof(cinfo) - 8), "the structure sizes are reported");
    }
    {
        g_errorCalls = 0;
        VshJpeg::GuestJpegCompress cinfo{};
        cinfo.err = jpeg_std_error_nid_postfix(&err);
        err.error_exit = RecordingErrorExit;
        jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION + 1, sizeof(cinfo));
        AssertTrue(g_errorCalls == 1 && g_errorCode == JERR_BAD_LIB_VERSION, "a wrong library version reaches the guest error_exit");
    }
    {
        g_errorCalls = 0;
        Sink sink;
        sink.chunk.assign(64, 0);
        g_sink = &sink;
        VshJpeg::GuestJpegDestinationMgr dest{};
        dest.init_destination = SinkInit;
        dest.empty_output_buffer = SinkEmpty;
        dest.term_destination = SinkTerm;
        VshJpeg::GuestJpegCompress cinfo{};
        cinfo.err = jpeg_std_error_nid_postfix(&err);
        err.error_exit = RecordingErrorExit;
        jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
        cinfo.dest = &dest;
        cinfo.image_width = 0;
        cinfo.image_height = 0;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults_nid_postfix(&cinfo);
        jpeg_start_compress_nid_postfix(&cinfo, 1);
        AssertTrue(g_errorCalls == 1 && g_errorCode == JERR_EMPTY_IMAGE, "an empty image reaches the guest error_exit");
        jpeg_destroy_compress_nid_postfix(&cinfo);
    }
    std::puts("compress error forwarding passed");
}

void Misuse() {
    bool threw = false;
    try {
        jpeg_set_defaults_nid_postfix(nullptr);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    AssertTrue(threw, "a null object throws");
    VshJpeg::GuestJpegCompress stranger{};
    threw = false;
    try {
        jpeg_set_quality_nid_postfix(&stranger, 80, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    AssertTrue(threw, "an object not created by jpeg_CreateCompress throws");

    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegCompress cinfo{};
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    threw = false;
    try {
        jpeg_start_compress_nid_postfix(&cinfo, 1);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    AssertTrue(threw, "starting without a destination manager throws");
    jpeg_destroy_compress_nid_postfix(&cinfo);
    std::puts("compress misuse passed");
}

std::int32_t APS5_VABI RefusingEmpty(VshJpeg::GuestCompressPtr cinfo) {
    (void)cinfo;
    ++g_sink->empties;
    return 0;
}

void LifecycleAndDestinationRefusal() {
    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegCompress cinfo{};
    int marker = 0;
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    cinfo.client_data = &marker;
    cinfo.dest = reinterpret_cast<VshJpeg::GuestJpegDestinationMgr*>(&marker);
    cinfo.image_width = 99;
    cinfo.num_scans = 5;
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    AssertTrue(cinfo.err == &err && cinfo.client_data == &marker, "create keeps err and client_data");
    AssertTrue(cinfo.dest == nullptr && cinfo.image_width == 0 && cinfo.num_scans == 0, "create clears the rest of the structure");
    jpeg_destroy_compress_nid_postfix(&cinfo);
    AssertTrue(cinfo.global_state == 0, "destroy leaves the object in state 0");
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    AssertTrue(cinfo.global_state == 100, "a destroyed object can be created again");
    jpeg_destroy_compress_nid_postfix(&cinfo);

    Sink sink;
    sink.chunk.assign(16, 0);
    g_sink = &sink;
    VshJpeg::GuestJpegDestinationMgr dest{};
    dest.init_destination = SinkInit;
    dest.empty_output_buffer = RefusingEmpty;
    dest.term_destination = SinkTerm;
    g_errorCalls = 0;
    err.error_exit = RecordingErrorExit;
    VshJpeg::GuestJpegCompress refused{};
    refused.err = &err;
    jpeg_CreateCompress_nid_postfix(&refused, JPEG_LIB_VERSION, sizeof(refused));
    refused.dest = &dest;
    refused.image_width = 32;
    refused.image_height = 32;
    refused.input_components = 3;
    refused.in_color_space = JCS_RGB;
    jpeg_set_defaults_nid_postfix(&refused);
    jpeg_start_compress_nid_postfix(&refused, 1);
    AssertTrue(g_errorCalls == 1 && g_errorCode == JERR_CANT_SUSPEND, "a refusing empty_output_buffer ends in JERR_CANT_SUSPEND");
    AssertTrue(sink.empties >= 1, "empty_output_buffer was asked");
    jpeg_destroy_compress_nid_postfix(&refused);
    std::puts("compress lifecycle passed");
}

void Suspension() {
    const Params p{96, 96, 3, JCS_RGB, 90, true, false, 0, 1024};
    const auto pixels = Pixels(p);
    const auto expected = HostEncode(p, pixels);
    Sink sink;
    sink.chunk.assign(p.chunk, 0);
    g_sink = &sink;
    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegDestinationMgr dest{};
    dest.init_destination = SinkInit;
    dest.empty_output_buffer = RefusingEmpty;
    dest.term_destination = SinkTerm;
    VshJpeg::GuestJpegCompress cinfo{};
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    cinfo.dest = &dest;
    cinfo.image_width = p.width;
    cinfo.image_height = p.height;
    cinfo.input_components = p.components;
    cinfo.in_color_space = p.space;
    jpeg_set_defaults_nid_postfix(&cinfo);
    jpeg_set_quality_nid_postfix(&cinfo, p.quality, 1);
    jpeg_start_compress_nid_postfix(&cinfo, 1);
    int suspensions = 0;
    while (cinfo.next_scanline < cinfo.image_height) {
        std::uint8_t* row = const_cast<std::uint8_t*>(pixels.data() + static_cast<std::size_t>(cinfo.next_scanline) * p.width * p.components);
        if (jpeg_write_scanlines_nid_postfix(&cinfo, &row, 1) == 0) {
            ++suspensions;
            const std::size_t used = sink.chunk.size() - cinfo.dest->free_in_buffer;
            sink.bytes.insert(sink.bytes.end(), sink.chunk.begin(), sink.chunk.begin() + used);
            cinfo.dest->next_output_byte = sink.chunk.data();
            cinfo.dest->free_in_buffer = sink.chunk.size();
        }
    }
    jpeg_finish_compress_nid_postfix(&cinfo);
    AssertTrue(suspensions > 3, "the refusing destination suspends the compressor");
    AssertTrue(sink.bytes == expected, "a suspended and resumed stream equals the host libjpeg stream");
    jpeg_destroy_compress_nid_postfix(&cinfo);
    std::printf("compress suspension passed: %d suspensions\n", suspensions);
}

void Recreate() {
    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegDestinationMgr dest{};
    Sink sink;
    sink.chunk.assign(4096, 0);
    g_sink = &sink;
    dest.init_destination = SinkInit;
    dest.empty_output_buffer = SinkEmpty;
    dest.term_destination = SinkTerm;
    VshJpeg::GuestJpegCompress cinfo{};
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    for (int i = 0; i < 20; ++i) {
        jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
        cinfo.dest = &dest;
        cinfo.image_width = 8;
        cinfo.image_height = 8;
        cinfo.input_components = 3;
        cinfo.in_color_space = JCS_RGB;
        jpeg_set_defaults_nid_postfix(&cinfo);
        jpeg_set_quality_nid_postfix(&cinfo, 50, 1);
    }
    jpeg_destroy_compress_nid_postfix(&cinfo);
    VshJpeg::GuestJpegCompress wrongSize{};
    wrongSize.err = jpeg_std_error_nid_postfix(&err);
    for (int i = 0; i < 20; ++i) {
        try {
            jpeg_CreateCompress_nid_postfix(&wrongSize, JPEG_LIB_VERSION, sizeof(wrongSize) + 8);
        } catch (const std::runtime_error&) {
        }
    }
    std::puts("compress re-create passed");
}

void ProgressMonitorThrows() {
    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegCompress cinfo{};
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    int monitor = 0;
    cinfo.progress = &monitor;
    bool thrown = false;
    try {
        jpeg_set_defaults_nid_postfix(&cinfo);
    } catch (const std::exception&) {
        thrown = true;
    }
    AssertTrue(thrown, "a guest progress monitor throws instead of being ignored");
    cinfo.progress = nullptr;
    jpeg_destroy_compress_nid_postfix(&cinfo);
    std::puts("compress progress monitor passed");
}

template <typename Call>
bool Throws(Call call) {
    try {
        call();
    } catch (const std::exception&) {
        return true;
    }
    return false;
}

void GuestMemoryManager() {
    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegCompress failed{};
    failed.err = jpeg_std_error_nid_postfix(&err);
    AssertTrue(Throws([&] { jpeg_CreateCompress_nid_postfix(&failed, JPEG_LIB_VERSION, sizeof(failed) + 8); }), "a wrong structure size fails");
    AssertTrue(failed.mem == nullptr, "a failed create leaves no memory manager");

    VshJpeg::GuestJpegCompress cinfo{};
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    AssertTrue(cinfo.mem != nullptr, "create sets the memory manager");
    auto* mem = cinfo.mem;
    AssertTrue(mem->max_alloc_chunk > 0, "the allocation limit is copied");
    auto* small = static_cast<std::uint8_t*>(mem->alloc_small(&cinfo, JPOOL_IMAGE, 64));
    auto* large = static_cast<std::uint8_t*>(mem->alloc_large(&cinfo, JPOOL_IMAGE, 4096));
    std::memset(small, 1, 64);
    std::memset(large, 2, 4096);
    auto** samples = mem->alloc_sarray(&cinfo, JPOOL_IMAGE, 16, 4);
    for (int r = 0; r < 4; ++r) std::memset(samples[r], r, 16);
    auto* blocks = static_cast<JBLOCKROW*>(mem->alloc_barray(&cinfo, JPOOL_IMAGE, 2, 3));
    std::memset(blocks[2], 0, 2 * sizeof(JBLOCK));
    AssertTrue(samples[3][15] == 3, "sample rows are writable");
    mem->free_pool(&cinfo, JPOOL_IMAGE);
    AssertTrue(Throws([&] { mem->request_virt_sarray(&cinfo, JPOOL_IMAGE, 0, 8, 8, 8); }), "virtual sample arrays throw");
    AssertTrue(Throws([&] { mem->request_virt_barray(&cinfo, JPOOL_IMAGE, 0, 8, 8, 8); }), "virtual block arrays throw");
    AssertTrue(Throws([&] { mem->realize_virt_arrays(&cinfo); }), "realizing virtual arrays throws");
    AssertTrue(Throws([&] { mem->access_virt_sarray(&cinfo, nullptr, 0, 1, 0); }), "accessing virtual sample arrays throws");
    AssertTrue(Throws([&] { mem->access_virt_barray(&cinfo, nullptr, 0, 1, 0); }), "accessing virtual block arrays throws");
    AssertTrue(Throws([&] { mem->self_destruct(&cinfo); }), "self_destruct throws");
    AssertTrue(Throws([&] { mem->alloc_large(&cinfo, JPOOL_IMAGE, ~std::uint64_t{0} / 2); }), "an impossible allocation reaches the guest error_exit");
    jpeg_destroy_compress_nid_postfix(&cinfo);
    AssertTrue(cinfo.mem == nullptr, "destroy clears the memory manager");
    std::puts("compress guest memory manager passed");
}

void UnimplementedExportsThrow() {
    AssertTrue(Throws([&] { jpeg_abort_nid_postfix(nullptr); }), "jpeg_abort throws");
    AssertTrue(Throws([&] { jpeg_stdio_dest_nid_postfix(nullptr, nullptr); }), "jpeg_stdio_dest throws");
    AssertTrue(Throws([&] { jpeg_stdio_src_nid_postfix(nullptr, nullptr); }), "jpeg_stdio_src throws");
    std::puts("unimplemented exports passed");
}

void ThrowingDefaultErrorExit() {
    NKWebKit::GuestJpegErrorMgr err{};
    VshJpeg::GuestJpegCompress cinfo{};
    cinfo.err = jpeg_std_error_nid_postfix(&err);
    jpeg_CreateCompress_nid_postfix(&cinfo, JPEG_LIB_VERSION, sizeof(cinfo));
    Sink sink;
    sink.chunk.assign(64, 0);
    g_sink = &sink;
    VshJpeg::GuestJpegDestinationMgr dest{};
    dest.init_destination = SinkInit;
    dest.empty_output_buffer = SinkEmpty;
    dest.term_destination = SinkTerm;
    cinfo.dest = &dest;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_defaults_nid_postfix(&cinfo);
    bool threw = false;
    try {
        jpeg_start_compress_nid_postfix(&cinfo, 1);
    } catch (const std::runtime_error&) {
        threw = true;
    }
    AssertTrue(threw, "the default error_exit throws instead of exiting");
    jpeg_destroy_compress_nid_postfix(&cinfo);
    std::puts("compress default error_exit passed");
}

}

int main() {
    CompareWithHost({17, 13, 3, JCS_RGB, 85, true, false, 0, 4096}, "rgb q85");
    CompareWithHost({64, 48, 3, JCS_RGB, 10, false, false, 0, 64}, "rgb q10 64-byte chunks");
    CompareWithHost({33, 31, 1, JCS_GRAYSCALE, 95, true, true, 0, 128}, "gray q95 optimized");
    CompareWithHost({40, 40, 3, JCS_RGB, 60, true, false, 2, 4096}, "rgb restart interval 2");
    CompareWithHost({16, 16, 4, JCS_CMYK, 75, true, false, 0, 4096}, "cmyk q75");
    CompareWithHost({1, 1, 3, JCS_RGB, 100, true, false, 0, 1}, "1x1 single-byte chunks");
    CompareWithHost({32, 32, 3, JCS_RGB, 80, true, false, 0, 4096, 1}, "guest edits comp_info sampling in place");
    CompareWithHost({32, 32, 3, JCS_RGB, 80, true, false, 0, 4096, 2}, "guest sets arithmetic tables");
    CompareWithHost({32, 32, 3, JCS_RGB, 80, true, false, 0, 4096, 3}, "guest edits quantization table in place");
    CompareWithHost({32, 32, 3, JCS_RGB, 80, true, false, 0, 4096, 4}, "guest replaces quantization table pointer");
    CompareWithHost({32, 32, 3, JCS_RGB, 80, true, false, 0, 4096, 5}, "guest replaces comp_info pointer");
    Recreate();
    ErrorForwarding();
    Misuse();
    LifecycleAndDestinationRefusal();
    Suspension();
    UnimplementedExportsThrow();
    GuestMemoryManager();
    ProgressMonitorThrows();
    ThrowingDefaultErrorExit();
    std::puts("Vsh jpeg compress tests passed.");
    return 0;
}
