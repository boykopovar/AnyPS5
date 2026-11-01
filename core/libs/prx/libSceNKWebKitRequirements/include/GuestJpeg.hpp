#ifndef CORE_LIBS_PRX_LIBSCE_NKWEBKIT_REQUIREMENTS_INCLUDE_GUEST_JPEG_HPP
#define CORE_LIBS_PRX_LIBSCE_NKWEBKIT_REQUIREMENTS_INCLUDE_GUEST_JPEG_HPP

#include "prx/libc/include/general/VabiMacros.hpp"

#include <cstddef>
#include <cstdint>

namespace NKWebKit {

struct GuestJpegDecompress;
struct GuestJpegCommon;
using GuestCommonPtr = GuestJpegCommon*;
using GuestDecompressPtr = GuestJpegDecompress*;

struct GuestJpegMarker {
    GuestJpegMarker* next;
    uint8_t marker;
    uint8_t pad1[3];
    uint32_t original_length;
    uint32_t data_length;
    uint32_t pad2;
    uint8_t* data;
};

static_assert(sizeof(GuestJpegMarker) == 32);
static_assert(offsetof(GuestJpegMarker, next) == 0);
static_assert(offsetof(GuestJpegMarker, marker) == 8);
static_assert(offsetof(GuestJpegMarker, original_length) == 12);
static_assert(offsetof(GuestJpegMarker, data_length) == 16);
static_assert(offsetof(GuestJpegMarker, data) == 24);

struct GuestJpegSourceMgr {
    const uint8_t* next_input_byte;
    uint64_t bytes_in_buffer;

    void (APS5_VABI *init_source)(GuestDecompressPtr cinfo);
    int32_t (APS5_VABI *fill_input_buffer)(GuestDecompressPtr cinfo);
    void (APS5_VABI *skip_input_data)(GuestDecompressPtr cinfo, int64_t num_bytes);
    int32_t (APS5_VABI *resync_to_restart)(GuestDecompressPtr cinfo, int32_t desired);
    void (APS5_VABI *term_source)(GuestDecompressPtr cinfo);
};

static_assert(sizeof(GuestJpegSourceMgr) == 56);
static_assert(offsetof(GuestJpegSourceMgr, next_input_byte) == 0);
static_assert(offsetof(GuestJpegSourceMgr, bytes_in_buffer) == 8);
static_assert(offsetof(GuestJpegSourceMgr, init_source) == 16);
static_assert(offsetof(GuestJpegSourceMgr, fill_input_buffer) == 24);
static_assert(offsetof(GuestJpegSourceMgr, skip_input_data) == 32);
static_assert(offsetof(GuestJpegSourceMgr, resync_to_restart) == 40);
static_assert(offsetof(GuestJpegSourceMgr, term_source) == 48);

struct GuestJpegErrorMgr {
    void (APS5_VABI *error_exit)(GuestCommonPtr cinfo);
    void (APS5_VABI *emit_message)(GuestCommonPtr cinfo, int32_t msg_level);
    void (APS5_VABI *output_message)(GuestCommonPtr cinfo);
    void (APS5_VABI *format_message)(GuestCommonPtr cinfo, char* buffer);
    void (APS5_VABI *reset_error_mgr)(GuestCommonPtr cinfo);

    int32_t msg_code;
    union {
        int32_t i[8];
        char s[80];
    } msg_parm;

    int32_t trace_level;
    uint32_t pad1;
    int64_t num_warnings;

    const char* const* jpeg_message_table;
    int32_t last_jpeg_message;
    uint32_t pad2;

    const char* const* addon_message_table;
    int32_t first_addon_message;
    int32_t last_addon_message;
};

static_assert(sizeof(GuestJpegErrorMgr) == 176);
static_assert(offsetof(GuestJpegErrorMgr, error_exit) == 0);
static_assert(offsetof(GuestJpegErrorMgr, emit_message) == 8);
static_assert(offsetof(GuestJpegErrorMgr, output_message) == 16);
static_assert(offsetof(GuestJpegErrorMgr, format_message) == 24);
static_assert(offsetof(GuestJpegErrorMgr, reset_error_mgr) == 32);
static_assert(offsetof(GuestJpegErrorMgr, msg_code) == 40);
static_assert(offsetof(GuestJpegErrorMgr, msg_parm) == 44);
static_assert(offsetof(GuestJpegErrorMgr, trace_level) == 124);
static_assert(offsetof(GuestJpegErrorMgr, num_warnings) == 136);
static_assert(offsetof(GuestJpegErrorMgr, jpeg_message_table) == 144);
static_assert(offsetof(GuestJpegErrorMgr, last_jpeg_message) == 152);
static_assert(offsetof(GuestJpegErrorMgr, addon_message_table) == 160);
static_assert(offsetof(GuestJpegErrorMgr, first_addon_message) == 168);
static_assert(offsetof(GuestJpegErrorMgr, last_addon_message) == 172);

struct GuestJpegCommon {
    GuestJpegErrorMgr* err;
    void* mem;
    void* progress;
    void* client_data;
    int32_t is_decompressor;
    int32_t global_state;
};

static_assert(sizeof(GuestJpegCommon) == 40);
static_assert(offsetof(GuestJpegCommon, err) == 0);
static_assert(offsetof(GuestJpegCommon, mem) == 8);
static_assert(offsetof(GuestJpegCommon, progress) == 16);
static_assert(offsetof(GuestJpegCommon, client_data) == 24);
static_assert(offsetof(GuestJpegCommon, is_decompressor) == 32);
static_assert(offsetof(GuestJpegCommon, global_state) == 36);

struct GuestJpegDecompress {
    GuestJpegErrorMgr* err;
    void* mem;
    void* progress;
    void* client_data;
    int32_t is_decompressor;
    int32_t global_state;

    GuestJpegSourceMgr* src;

    uint32_t image_width;
    uint32_t image_height;
    int32_t num_components;
    int32_t jpeg_color_space;

    int32_t out_color_space;
    uint32_t scale_num;
    uint32_t scale_denom;
    uint32_t pad1;

    double output_gamma;

    int32_t buffered_image;
    int32_t raw_data_out;

    int32_t dct_method;
    int32_t do_fancy_upsampling;
    int32_t do_block_smoothing;
    int32_t quantize_colors;

    int32_t dither_mode;
    int32_t two_pass_quantize;
    int32_t desired_number_of_colors;
    int32_t enable_1pass_quant;
    int32_t enable_external_quant;
    int32_t enable_2pass_quant;

    uint32_t output_width;
    uint32_t output_height;
    int32_t out_color_components;
    int32_t output_components;
    int32_t rec_outbuf_height;
    int32_t actual_number_of_colors;

    uint8_t** colormap;

    uint32_t output_scanline;
    int32_t input_scan_number;
    uint32_t input_iMCU_row;
    int32_t output_scan_number;
    uint32_t output_iMCU_row;
    uint32_t pad2;

    int32_t (*coef_bits)[64];

    void* quant_tbl_ptrs[4];
    void* dc_huff_tbl_ptrs[4];
    void* ac_huff_tbl_ptrs[4];

    int32_t data_precision;
    uint32_t pad3;
    void* comp_info;

    int32_t progressive_mode;
    int32_t arith_code;

    uint8_t arith_dc_L[16];
    uint8_t arith_dc_U[16];
    uint8_t arith_ac_K[16];

    uint32_t restart_interval;

    int32_t saw_JFIF_marker;
    uint8_t JFIF_major_version;
    uint8_t JFIF_minor_version;
    uint8_t density_unit;
    uint8_t pad4;
    uint16_t X_density;
    uint16_t Y_density;
    int32_t saw_Adobe_marker;
    uint8_t Adobe_transform;
    uint8_t pad5[3];

    int32_t CCIR601_sampling;
    uint32_t pad6;

    GuestJpegMarker* marker_list;

    int32_t max_h_samp_factor;
    int32_t max_v_samp_factor;
    int32_t min_DCT_scaled_size;
    uint32_t total_iMCU_rows;

    uint8_t* sample_range_limit;

    int32_t comps_in_scan;
    uint32_t pad7;
    void* cur_comp_info[4];

    uint32_t MCUs_per_row;
    uint32_t MCU_rows_in_scan;
    int32_t blocks_in_MCU;
    int32_t MCU_membership[10];

    int32_t Ss;
    int32_t Se;
    int32_t Ah;
    int32_t Al;

    int32_t unread_marker;

    void* master;
    void* main;
    void* coef;
    void* post;
    void* inputctl;
    void* marker;
    void* entropy;
    void* idct;
    void* upsample;
    void* cconvert;
    void* cquantize;
};

static_assert(sizeof(GuestJpegDecompress) == 632);
static_assert(offsetof(GuestJpegDecompress, err) == 0);
static_assert(offsetof(GuestJpegDecompress, mem) == 8);
static_assert(offsetof(GuestJpegDecompress, progress) == 16);
static_assert(offsetof(GuestJpegDecompress, client_data) == 24);
static_assert(offsetof(GuestJpegDecompress, is_decompressor) == 32);
static_assert(offsetof(GuestJpegDecompress, global_state) == 36);
static_assert(offsetof(GuestJpegDecompress, src) == 40);
static_assert(offsetof(GuestJpegDecompress, image_width) == 48);
static_assert(offsetof(GuestJpegDecompress, image_height) == 52);
static_assert(offsetof(GuestJpegDecompress, num_components) == 56);
static_assert(offsetof(GuestJpegDecompress, jpeg_color_space) == 60);
static_assert(offsetof(GuestJpegDecompress, out_color_space) == 64);
static_assert(offsetof(GuestJpegDecompress, scale_num) == 68);
static_assert(offsetof(GuestJpegDecompress, scale_denom) == 72);
static_assert(offsetof(GuestJpegDecompress, output_gamma) == 80);
static_assert(offsetof(GuestJpegDecompress, buffered_image) == 88);
static_assert(offsetof(GuestJpegDecompress, raw_data_out) == 92);
static_assert(offsetof(GuestJpegDecompress, dct_method) == 96);
static_assert(offsetof(GuestJpegDecompress, do_fancy_upsampling) == 100);
static_assert(offsetof(GuestJpegDecompress, do_block_smoothing) == 104);
static_assert(offsetof(GuestJpegDecompress, quantize_colors) == 108);
static_assert(offsetof(GuestJpegDecompress, dither_mode) == 112);
static_assert(offsetof(GuestJpegDecompress, two_pass_quantize) == 116);
static_assert(offsetof(GuestJpegDecompress, desired_number_of_colors) == 120);
static_assert(offsetof(GuestJpegDecompress, enable_1pass_quant) == 124);
static_assert(offsetof(GuestJpegDecompress, enable_external_quant) == 128);
static_assert(offsetof(GuestJpegDecompress, enable_2pass_quant) == 132);
static_assert(offsetof(GuestJpegDecompress, output_width) == 136);
static_assert(offsetof(GuestJpegDecompress, output_height) == 140);
static_assert(offsetof(GuestJpegDecompress, out_color_components) == 144);
static_assert(offsetof(GuestJpegDecompress, output_components) == 148);
static_assert(offsetof(GuestJpegDecompress, rec_outbuf_height) == 152);
static_assert(offsetof(GuestJpegDecompress, actual_number_of_colors) == 156);
static_assert(offsetof(GuestJpegDecompress, colormap) == 160);
static_assert(offsetof(GuestJpegDecompress, output_scanline) == 168);
static_assert(offsetof(GuestJpegDecompress, input_scan_number) == 172);
static_assert(offsetof(GuestJpegDecompress, input_iMCU_row) == 176);
static_assert(offsetof(GuestJpegDecompress, output_scan_number) == 180);
static_assert(offsetof(GuestJpegDecompress, output_iMCU_row) == 184);
static_assert(offsetof(GuestJpegDecompress, coef_bits) == 192);
static_assert(offsetof(GuestJpegDecompress, quant_tbl_ptrs) == 200);
static_assert(offsetof(GuestJpegDecompress, dc_huff_tbl_ptrs) == 232);
static_assert(offsetof(GuestJpegDecompress, ac_huff_tbl_ptrs) == 264);
static_assert(offsetof(GuestJpegDecompress, data_precision) == 296);
static_assert(offsetof(GuestJpegDecompress, comp_info) == 304);
static_assert(offsetof(GuestJpegDecompress, progressive_mode) == 312);
static_assert(offsetof(GuestJpegDecompress, arith_code) == 316);
static_assert(offsetof(GuestJpegDecompress, arith_dc_L) == 320);
static_assert(offsetof(GuestJpegDecompress, arith_dc_U) == 336);
static_assert(offsetof(GuestJpegDecompress, arith_ac_K) == 352);
static_assert(offsetof(GuestJpegDecompress, restart_interval) == 368);
static_assert(offsetof(GuestJpegDecompress, saw_JFIF_marker) == 372);
static_assert(offsetof(GuestJpegDecompress, JFIF_major_version) == 376);
static_assert(offsetof(GuestJpegDecompress, JFIF_minor_version) == 377);
static_assert(offsetof(GuestJpegDecompress, density_unit) == 378);
static_assert(offsetof(GuestJpegDecompress, X_density) == 380);
static_assert(offsetof(GuestJpegDecompress, Y_density) == 382);
static_assert(offsetof(GuestJpegDecompress, saw_Adobe_marker) == 384);
static_assert(offsetof(GuestJpegDecompress, Adobe_transform) == 388);
static_assert(offsetof(GuestJpegDecompress, CCIR601_sampling) == 392);
static_assert(offsetof(GuestJpegDecompress, marker_list) == 400);
static_assert(offsetof(GuestJpegDecompress, max_h_samp_factor) == 408);
static_assert(offsetof(GuestJpegDecompress, max_v_samp_factor) == 412);
static_assert(offsetof(GuestJpegDecompress, min_DCT_scaled_size) == 416);
static_assert(offsetof(GuestJpegDecompress, total_iMCU_rows) == 420);
static_assert(offsetof(GuestJpegDecompress, sample_range_limit) == 424);
static_assert(offsetof(GuestJpegDecompress, comps_in_scan) == 432);
static_assert(offsetof(GuestJpegDecompress, cur_comp_info) == 440);
static_assert(offsetof(GuestJpegDecompress, MCUs_per_row) == 472);
static_assert(offsetof(GuestJpegDecompress, MCU_rows_in_scan) == 476);
static_assert(offsetof(GuestJpegDecompress, blocks_in_MCU) == 480);
static_assert(offsetof(GuestJpegDecompress, MCU_membership) == 484);
static_assert(offsetof(GuestJpegDecompress, Ss) == 524);
static_assert(offsetof(GuestJpegDecompress, Se) == 528);
static_assert(offsetof(GuestJpegDecompress, Ah) == 532);
static_assert(offsetof(GuestJpegDecompress, Al) == 536);
static_assert(offsetof(GuestJpegDecompress, unread_marker) == 540);
static_assert(offsetof(GuestJpegDecompress, master) == 544);
static_assert(offsetof(GuestJpegDecompress, main) == 552);
static_assert(offsetof(GuestJpegDecompress, coef) == 560);
static_assert(offsetof(GuestJpegDecompress, post) == 568);
static_assert(offsetof(GuestJpegDecompress, inputctl) == 576);
static_assert(offsetof(GuestJpegDecompress, marker) == 584);
static_assert(offsetof(GuestJpegDecompress, entropy) == 592);
static_assert(offsetof(GuestJpegDecompress, idct) == 600);
static_assert(offsetof(GuestJpegDecompress, upsample) == 608);
static_assert(offsetof(GuestJpegDecompress, cconvert) == 616);
static_assert(offsetof(GuestJpegDecompress, cquantize) == 624);

} // namespace NKWebKit

#endif
