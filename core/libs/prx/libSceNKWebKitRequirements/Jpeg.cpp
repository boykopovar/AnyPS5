#include "prx/libSceNKWebKitRequirements/include/GuestJpeg.hpp"
#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"

#include <stdexcept>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

extern "C" {
#include <jpeglib.h>
#include <jerror.h>
}

namespace {

struct JpegBinding;

struct JpegBinding {
    NKWebKit::GuestJpegDecompress* guest_cinfo = nullptr;
    jpeg_decompress_struct host_cinfo{};
    jpeg_error_mgr host_err{};
    jpeg_source_mgr host_src{};
    std::vector<std::unique_ptr<NKWebKit::GuestJpegMarker>> guest_markers;
    void** jump = nullptr;
};

static std::mutex g_jpegMutex;
static std::unordered_map<NKWebKit::GuestJpegDecompress*, std::unique_ptr<JpegBinding>> g_jpegBindings;

[[noreturn]] static void HostErrorExit(j_common_ptr cinfo) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->jump) std::abort();
    __builtin_longjmp(b->jump, 1);
}

template <typename Call>
static bool RunGuarded(JpegBinding* b, Call call) {
    void* buffer[5]{};
    void** const previous = b->jump;
    if (__builtin_setjmp(buffer) != 0) {
        b->jump = previous;
        return false;
    }
    b->jump = buffer;
    call();
    b->jump = previous;
    return true;
}

static void HostEmitMessage(j_common_ptr cinfo, int msg_level) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->err) return;
    auto* gerr = b->guest_cinfo->err;
    gerr->msg_code = b->host_err.msg_code;
    for (int i = 0; i < 8; ++i) {
        gerr->msg_parm.i[i] = b->host_err.msg_parm.i[i];
    }
    if (gerr->emit_message) {
        gerr->emit_message(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo), msg_level);
    }
}

static void HostOutputMessage(j_common_ptr cinfo) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->err) return;
    auto* gerr = b->guest_cinfo->err;
    gerr->msg_code = b->host_err.msg_code;
    for (int i = 0; i < 8; ++i) {
        gerr->msg_parm.i[i] = b->host_err.msg_parm.i[i];
    }
    if (gerr->output_message) {
        gerr->output_message(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo));
    }
}

static void HostFormatMessage(j_common_ptr cinfo, char* buffer) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->err) return;
    auto* gerr = b->guest_cinfo->err;
    gerr->msg_code = b->host_err.msg_code;
    for (int i = 0; i < 8; ++i) {
        gerr->msg_parm.i[i] = b->host_err.msg_parm.i[i];
    }
    if (gerr->format_message) {
        gerr->format_message(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo), buffer);
    }
}

static void HostResetErrorMgr(j_common_ptr cinfo) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->err) return;
    auto* gerr = b->guest_cinfo->err;
    if (gerr->reset_error_mgr) {
        gerr->reset_error_mgr(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo));
    }
}

static void HostInitSource(j_decompress_ptr cinfo) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->src) return;
    auto* gsrc = b->guest_cinfo->src;
    if (gsrc->init_source) {
        gsrc->init_source(b->guest_cinfo);
    }
    b->host_src.next_input_byte = gsrc->next_input_byte;
    b->host_src.bytes_in_buffer = static_cast<size_t>(gsrc->bytes_in_buffer);
}

static boolean HostFillInputBuffer(j_decompress_ptr cinfo) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->src) return FALSE;
    auto* gsrc = b->guest_cinfo->src;
    int32_t ret = 0;
    if (gsrc->fill_input_buffer) {
        ret = gsrc->fill_input_buffer(b->guest_cinfo);
    }
    b->host_src.next_input_byte = gsrc->next_input_byte;
    b->host_src.bytes_in_buffer = static_cast<size_t>(gsrc->bytes_in_buffer);
    return ret ? TRUE : FALSE;
}

static void HostSkipInputData(j_decompress_ptr cinfo, long num_bytes) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->src) return;
    auto* gsrc = b->guest_cinfo->src;
    if (gsrc->skip_input_data) {
        gsrc->skip_input_data(b->guest_cinfo, static_cast<int64_t>(num_bytes));
    }
    b->host_src.next_input_byte = gsrc->next_input_byte;
    b->host_src.bytes_in_buffer = static_cast<size_t>(gsrc->bytes_in_buffer);
}

static boolean HostResyncToRestart(j_decompress_ptr cinfo, int desired) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->src) {
        return jpeg_resync_to_restart(cinfo, desired);
    }
    auto* gsrc = b->guest_cinfo->src;
    if (gsrc->resync_to_restart) {
        int32_t ret = gsrc->resync_to_restart(b->guest_cinfo, desired);
        b->host_src.next_input_byte = gsrc->next_input_byte;
        b->host_src.bytes_in_buffer = static_cast<size_t>(gsrc->bytes_in_buffer);
        return ret ? TRUE : FALSE;
    }
    return jpeg_resync_to_restart(cinfo, desired);
}

static void HostTermSource(j_decompress_ptr cinfo) {
    auto* b = static_cast<JpegBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->src) return;
    auto* gsrc = b->guest_cinfo->src;
    if (gsrc->term_source) {
        gsrc->term_source(b->guest_cinfo);
    }
    b->host_src.next_input_byte = gsrc->next_input_byte;
    b->host_src.bytes_in_buffer = static_cast<size_t>(gsrc->bytes_in_buffer);
}

static void APS5_VABI DefaultGuestErrorExit_nid_no_patch(NKWebKit::GuestCommonPtr cinfo) {
    char buffer[JMSG_LENGTH_MAX]{};
    if (cinfo && cinfo->err) {
        if (cinfo->err->output_message) cinfo->err->output_message(cinfo);
        if (cinfo->err->format_message) cinfo->err->format_message(cinfo, buffer);
    }
    throw std::runtime_error(std::string("libjpeg: ") + buffer);
}

static void APS5_VABI DefaultGuestEmitMessage_nid_no_patch(NKWebKit::GuestCommonPtr cinfo, int32_t msg_level) {
    if (cinfo && cinfo->err && msg_level < 0) {
        cinfo->err->num_warnings++;
    }
}

static void APS5_VABI DefaultGuestOutputMessage_nid_no_patch(NKWebKit::GuestCommonPtr cinfo) {
    if (!cinfo || !cinfo->err) return;
    char buffer[200];
    if (cinfo->err->format_message) {
        cinfo->err->format_message(cinfo, buffer);
    }
}

static void APS5_VABI DefaultGuestFormatMessage_nid_no_patch(NKWebKit::GuestCommonPtr cinfo, char* buffer) {
    if (!cinfo || !cinfo->err || !buffer) return;
    jpeg_decompress_struct dummy{};
    jpeg_error_mgr dummy_err{};
    jpeg_std_error(&dummy_err);
    dummy.err = &dummy_err;
    dummy_err.msg_code = cinfo->err->msg_code;
    for (int i = 0; i < 8; ++i) {
        dummy_err.msg_parm.i[i] = cinfo->err->msg_parm.i[i];
    }
    dummy_err.format_message(reinterpret_cast<j_common_ptr>(&dummy), buffer);
}

static void APS5_VABI DefaultGuestResetErrorMgr_nid_no_patch(NKWebKit::GuestCommonPtr cinfo) {
    if (cinfo && cinfo->err) {
        cinfo->err->num_warnings = 0;
        cinfo->err->msg_code = 0;
    }
}

static JpegBinding* FindBinding(NKWebKit::GuestJpegDecompress* cinfo) {
    if (!cinfo) return nullptr;
    std::lock_guard<std::mutex> lock(g_jpegMutex);
    auto it = g_jpegBindings.find(cinfo);
    if (it != g_jpegBindings.end()) {
        return it->second.get();
    }
    return nullptr;
}

static void SyncGuestToHost(JpegBinding* b) {
    auto* g = b->guest_cinfo;
    auto* h = &b->host_cinfo;
    if (g->src) {
        b->host_src.next_input_byte = g->src->next_input_byte;
        b->host_src.bytes_in_buffer = static_cast<size_t>(g->src->bytes_in_buffer);
    }
    h->out_color_space = static_cast<J_COLOR_SPACE>(g->out_color_space);
    h->scale_num = g->scale_num;
    h->scale_denom = g->scale_denom;
    h->output_gamma = g->output_gamma;
    h->buffered_image = g->buffered_image ? TRUE : FALSE;
    h->raw_data_out = g->raw_data_out ? TRUE : FALSE;
    h->dct_method = static_cast<J_DCT_METHOD>(g->dct_method);
    h->do_fancy_upsampling = g->do_fancy_upsampling ? TRUE : FALSE;
    h->do_block_smoothing = g->do_block_smoothing ? TRUE : FALSE;
    h->quantize_colors = g->quantize_colors ? TRUE : FALSE;
    h->dither_mode = static_cast<J_DITHER_MODE>(g->dither_mode);
    h->two_pass_quantize = g->two_pass_quantize ? TRUE : FALSE;
    h->desired_number_of_colors = g->desired_number_of_colors;
    h->enable_1pass_quant = g->enable_1pass_quant ? TRUE : FALSE;
    h->enable_external_quant = g->enable_external_quant ? TRUE : FALSE;
    h->enable_2pass_quant = g->enable_2pass_quant ? TRUE : FALSE;
    h->colormap = reinterpret_cast<JSAMPARRAY>(g->colormap);
}

static void SyncHostToGuest(JpegBinding* b) {
    auto* g = b->guest_cinfo;
    auto* h = &b->host_cinfo;
    if (g->src) {
        g->src->next_input_byte = b->host_src.next_input_byte;
        g->src->bytes_in_buffer = static_cast<uint64_t>(b->host_src.bytes_in_buffer);
    }
    g->image_width = h->image_width;
    g->image_height = h->image_height;
    g->num_components = h->num_components;
    g->jpeg_color_space = static_cast<int32_t>(h->jpeg_color_space);
    g->out_color_space = static_cast<int32_t>(h->out_color_space);
    g->scale_num = h->scale_num;
    g->scale_denom = h->scale_denom;
    g->output_gamma = h->output_gamma;
    g->buffered_image = h->buffered_image ? 1 : 0;
    g->raw_data_out = h->raw_data_out ? 1 : 0;
    g->dct_method = static_cast<int32_t>(h->dct_method);
    g->do_fancy_upsampling = h->do_fancy_upsampling ? 1 : 0;
    g->do_block_smoothing = h->do_block_smoothing ? 1 : 0;
    g->quantize_colors = h->quantize_colors ? 1 : 0;
    g->dither_mode = static_cast<int32_t>(h->dither_mode);
    g->two_pass_quantize = h->two_pass_quantize ? 1 : 0;
    g->desired_number_of_colors = h->desired_number_of_colors;
    g->enable_1pass_quant = h->enable_1pass_quant ? 1 : 0;
    g->enable_external_quant = h->enable_external_quant ? 1 : 0;
    g->enable_2pass_quant = h->enable_2pass_quant ? 1 : 0;
    g->output_width = h->output_width;
    g->output_height = h->output_height;
    g->out_color_components = h->out_color_components;
    g->output_components = h->output_components;
    g->rec_outbuf_height = h->rec_outbuf_height;
    g->actual_number_of_colors = h->actual_number_of_colors;
    g->colormap = reinterpret_cast<uint8_t**>(h->colormap);
    g->output_scanline = h->output_scanline;
    g->input_scan_number = h->input_scan_number;
    g->input_iMCU_row = h->input_iMCU_row;
    g->output_scan_number = h->output_scan_number;
    g->output_iMCU_row = h->output_iMCU_row;
    g->data_precision = h->data_precision;
    g->progressive_mode = h->progressive_mode ? 1 : 0;
    g->arith_code = h->arith_code ? 1 : 0;
    g->restart_interval = h->restart_interval;
    g->saw_JFIF_marker = h->saw_JFIF_marker ? 1 : 0;
    g->JFIF_major_version = h->JFIF_major_version;
    g->JFIF_minor_version = h->JFIF_minor_version;
    g->density_unit = h->density_unit;
    g->X_density = h->X_density;
    g->Y_density = h->Y_density;
    g->saw_Adobe_marker = h->saw_Adobe_marker ? 1 : 0;
    g->Adobe_transform = h->Adobe_transform;
    g->CCIR601_sampling = h->CCIR601_sampling ? 1 : 0;
    g->max_h_samp_factor = h->max_h_samp_factor;
    g->max_v_samp_factor = h->max_v_samp_factor;
    g->min_DCT_scaled_size = h->min_DCT_scaled_size;
    g->total_iMCU_rows = h->total_iMCU_rows;
    g->comps_in_scan = h->comps_in_scan;
    g->MCUs_per_row = h->MCUs_per_row;
    g->MCU_rows_in_scan = h->MCU_rows_in_scan;
    g->blocks_in_MCU = h->blocks_in_MCU;
    g->unread_marker = h->unread_marker;
    g->global_state = h->global_state;
    g->is_decompressor = h->is_decompressor ? 1 : 0;

    b->guest_markers.clear();
    NKWebKit::GuestJpegMarker* prev = nullptr;
    for (jpeg_saved_marker_ptr hm = h->marker_list; hm != nullptr; hm = hm->next) {
        auto gm = std::make_unique<NKWebKit::GuestJpegMarker>();
        gm->marker = hm->marker;
        gm->original_length = hm->original_length;
        gm->data_length = hm->data_length;
        gm->data = hm->data;
        gm->next = nullptr;
        if (prev) {
            prev->next = gm.get();
        } else {
            g->marker_list = gm.get();
        }
        prev = gm.get();
        b->guest_markers.push_back(std::move(gm));
    }
    if (!prev) {
        g->marker_list = nullptr;
    }
}

static void ForwardError(JpegBinding* b) {
    auto* g = b->guest_cinfo;
    if (g->err) {
        g->err->msg_code = b->host_err.msg_code;
        for (int i = 0; i < 8; ++i) {
            g->err->msg_parm.i[i] = b->host_err.msg_parm.i[i];
        }
        g->err->num_warnings = b->host_err.num_warnings;
        g->err->trace_level = b->host_err.trace_level;
        if (g->err->error_exit) {
            g->err->error_exit(reinterpret_cast<NKWebKit::GuestCommonPtr>(g));
        }
    }
}

static void HandleException(JpegBinding* b) {
    if (!b || !b->guest_cinfo) return;
    SyncHostToGuest(b);
    ForwardError(b);
}

} // namespace

extern "C" {

NKWebKit::GuestJpegErrorMgr* APS5_VABI jpeg_std_error_nid_postfix(NKWebKit::GuestJpegErrorMgr* err) {
    if (!err) throw std::invalid_argument("libjpeg: null error manager");
    static jpeg_error_mgr s_std_err;
    static bool s_std_inited = false;
    if (!s_std_inited) {
        jpeg_std_error(&s_std_err);
        s_std_inited = true;
    }

    err->error_exit = DefaultGuestErrorExit_nid_no_patch;
    err->emit_message = DefaultGuestEmitMessage_nid_no_patch;
    err->output_message = DefaultGuestOutputMessage_nid_no_patch;
    err->format_message = DefaultGuestFormatMessage_nid_no_patch;
    err->reset_error_mgr = DefaultGuestResetErrorMgr_nid_no_patch;
    err->msg_code = 0;
    err->trace_level = 0;
    err->num_warnings = 0;
    err->jpeg_message_table = s_std_err.jpeg_message_table;
    err->last_jpeg_message = s_std_err.last_jpeg_message;
    err->addon_message_table = nullptr;
    err->first_addon_message = 0;
    err->last_addon_message = 0;
    return err;
}

void APS5_VABI jpeg_CreateDecompress_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo, int version, size_t structsize) {
    if (!cinfo) throw std::invalid_argument("libjpeg: null decompression object");
    auto b = std::make_unique<JpegBinding>();
    b->guest_cinfo = cinfo;
    b->host_cinfo.err = &b->host_err;
    jpeg_std_error(&b->host_err);
    b->host_err.error_exit = HostErrorExit;
    b->host_err.emit_message = HostEmitMessage;
    b->host_err.output_message = HostOutputMessage;
    b->host_err.format_message = HostFormatMessage;
    b->host_err.reset_error_mgr = HostResetErrorMgr;
    b->host_cinfo.client_data = b.get();

    if (!RunGuarded(b.get(), [&] { jpeg_CreateDecompress(&b->host_cinfo, version, sizeof(jpeg_decompress_struct)); })) {
        ForwardError(b.get());
        return;
    }

    b->host_cinfo.src = &b->host_src;
    b->host_src.init_source = HostInitSource;
    b->host_src.fill_input_buffer = HostFillInputBuffer;
    b->host_src.skip_input_data = HostSkipInputData;
    b->host_src.resync_to_restart = HostResyncToRestart;
    b->host_src.term_source = HostTermSource;

    SyncHostToGuest(b.get());

    std::lock_guard<std::mutex> lock(g_jpegMutex);
    g_jpegBindings[cinfo] = std::move(b);
}

void APS5_VABI jpeg_destroy_decompress_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    if (!cinfo) throw std::invalid_argument("libjpeg: null decompression object");
    std::unique_ptr<JpegBinding> b;
    {
        std::lock_guard<std::mutex> lock(g_jpegMutex);
        auto it = g_jpegBindings.find(cinfo);
        if (it != g_jpegBindings.end()) {
            b = std::move(it->second);
            g_jpegBindings.erase(it);
        }
    }
    if (b) {
        RunGuarded(b.get(), [&] { jpeg_destroy_decompress(&b->host_cinfo); });
    }
}

int APS5_VABI jpeg_read_header_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo, int require_image) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    int ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_read_header(&b->host_cinfo, require_image ? TRUE : FALSE); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return ret;
}

int APS5_VABI jpeg_start_decompress_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    int ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_start_decompress(&b->host_cinfo); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return ret ? 1 : 0;
}

uint32_t APS5_VABI jpeg_read_scanlines_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo, uint8_t** scanlines, uint32_t max_lines) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    JDIMENSION ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_read_scanlines(&b->host_cinfo, reinterpret_cast<JSAMPARRAY>(scanlines), max_lines); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return static_cast<uint32_t>(ret);
}

int APS5_VABI jpeg_finish_decompress_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    int ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_finish_decompress(&b->host_cinfo); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return ret ? 1 : 0;
}

void APS5_VABI jpeg_calc_output_dimensions_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { jpeg_calc_output_dimensions(&b->host_cinfo); })) {
        HandleException(b);
        return;
    }
    SyncHostToGuest(b);
}

int APS5_VABI jpeg_consume_input_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    int ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_consume_input(&b->host_cinfo); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return ret;
}

int APS5_VABI jpeg_has_multiple_scans_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    boolean ret = FALSE;
    if (!RunGuarded(b, [&] { ret = jpeg_has_multiple_scans(&b->host_cinfo); })) {
        HandleException(b);
        return 0;
    }
    return ret ? 1 : 0;
}

int APS5_VABI jpeg_input_complete_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    boolean ret = FALSE;
    if (!RunGuarded(b, [&] { ret = jpeg_input_complete(&b->host_cinfo); })) {
        HandleException(b);
        return 0;
    }
    return ret ? 1 : 0;
}

int APS5_VABI jpeg_start_output_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo, int scan_number) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    int ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_start_output(&b->host_cinfo, scan_number); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return ret ? 1 : 0;
}

int APS5_VABI jpeg_finish_output_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    int ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_finish_output(&b->host_cinfo); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return ret ? 1 : 0;
}

int APS5_VABI jpeg_resync_to_restart_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo, int desired) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    int ret = 0;
    SyncGuestToHost(b);
    if (!RunGuarded(b, [&] { ret = jpeg_resync_to_restart(&b->host_cinfo, desired); })) {
        HandleException(b);
        return 0;
    }
    SyncHostToGuest(b);
    return ret ? 1 : 0;
}

void APS5_VABI jpeg_save_markers_nid_postfix(NKWebKit::GuestJpegDecompress* cinfo, int marker_code, unsigned int length_limit) {
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the decompression object was not created by jpeg_CreateDecompress");
    if (!RunGuarded(b, [&] { jpeg_save_markers(&b->host_cinfo, marker_code, length_limit); })) HandleException(b);
}

}
