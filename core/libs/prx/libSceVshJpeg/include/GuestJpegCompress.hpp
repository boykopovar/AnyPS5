#ifndef CORE_LIBS_PRX_LIBSCE_VSH_JPEG_INCLUDE_GUEST_JPEG_COMPRESS_HPP
#define CORE_LIBS_PRX_LIBSCE_VSH_JPEG_INCLUDE_GUEST_JPEG_COMPRESS_HPP

#include "prx/libSceNKWebKitRequirements/include/GuestJpeg.hpp"

#include <cstddef>
#include <cstdint>

namespace VshJpeg {

struct GuestJpegCompress;
using GuestCompressPtr = GuestJpegCompress*;

struct GuestJpegDestinationMgr {
    uint8_t* next_output_byte;
    uint64_t free_in_buffer;

    void (APS5_VABI *init_destination)(GuestCompressPtr cinfo);
    int32_t (APS5_VABI *empty_output_buffer)(GuestCompressPtr cinfo);
    void (APS5_VABI *term_destination)(GuestCompressPtr cinfo);
};

static_assert(sizeof(GuestJpegDestinationMgr) == 40);
static_assert(offsetof(GuestJpegDestinationMgr, next_output_byte) == 0);
static_assert(offsetof(GuestJpegDestinationMgr, free_in_buffer) == 8);
static_assert(offsetof(GuestJpegDestinationMgr, init_destination) == 16);
static_assert(offsetof(GuestJpegDestinationMgr, empty_output_buffer) == 24);
static_assert(offsetof(GuestJpegDestinationMgr, term_destination) == 32);

struct GuestJpegMemoryMgr {
    void* (APS5_VABI *alloc_small)(GuestCompressPtr cinfo, int32_t pool_id, uint64_t size);
    void* (APS5_VABI *alloc_large)(GuestCompressPtr cinfo, int32_t pool_id, uint64_t size);
    uint8_t** (APS5_VABI *alloc_sarray)(GuestCompressPtr cinfo, int32_t pool_id, uint32_t samples_per_row, uint32_t num_rows);
    void* (APS5_VABI *alloc_barray)(GuestCompressPtr cinfo, int32_t pool_id, uint32_t blocks_per_row, uint32_t num_rows);
    void* (APS5_VABI *request_virt_sarray)(GuestCompressPtr cinfo, int32_t pool_id, int32_t pre_zero, uint32_t samples_per_row, uint32_t num_rows, uint32_t max_access);
    void* (APS5_VABI *request_virt_barray)(GuestCompressPtr cinfo, int32_t pool_id, int32_t pre_zero, uint32_t blocks_per_row, uint32_t num_rows, uint32_t max_access);
    void (APS5_VABI *realize_virt_arrays)(GuestCompressPtr cinfo);
    uint8_t** (APS5_VABI *access_virt_sarray)(GuestCompressPtr cinfo, void* array, uint32_t start_row, uint32_t num_rows, int32_t writable);
    void* (APS5_VABI *access_virt_barray)(GuestCompressPtr cinfo, void* array, uint32_t start_row, uint32_t num_rows, int32_t writable);
    void (APS5_VABI *free_pool)(GuestCompressPtr cinfo, int32_t pool_id);
    void (APS5_VABI *self_destruct)(GuestCompressPtr cinfo);
    int64_t max_memory_to_use;
    int64_t max_alloc_chunk;
};

static_assert(sizeof(GuestJpegMemoryMgr) == 104);
static_assert(offsetof(GuestJpegMemoryMgr, free_pool) == 72);
static_assert(offsetof(GuestJpegMemoryMgr, max_memory_to_use) == 88);

struct GuestJpegCompress {
    NKWebKit::GuestJpegErrorMgr* err;
    GuestJpegMemoryMgr* mem;
    void* progress;
    void* client_data;
    int32_t is_decompressor;
    int32_t global_state;

    GuestJpegDestinationMgr* dest;

    uint32_t image_width;
    uint32_t image_height;
    int32_t input_components;
    int32_t in_color_space;

    double input_gamma;

    int32_t data_precision;
    int32_t num_components;
    int32_t jpeg_color_space;
    uint32_t pad1;

    void* comp_info;

    void* quant_tbl_ptrs[4];
    void* dc_huff_tbl_ptrs[4];
    void* ac_huff_tbl_ptrs[4];

    uint8_t arith_dc_L[16];
    uint8_t arith_dc_U[16];
    uint8_t arith_ac_K[16];

    int32_t num_scans;
    uint32_t pad2;
    const void* scan_info;

    int32_t raw_data_in;
    int32_t arith_code;
    int32_t optimize_coding;
    int32_t CCIR601_sampling;
    int32_t smoothing_factor;
    int32_t dct_method;
    uint32_t restart_interval;
    int32_t restart_in_rows;

    int32_t write_JFIF_header;
    uint8_t JFIF_major_version;
    uint8_t JFIF_minor_version;
    uint8_t density_unit;
    uint8_t pad3;
    uint16_t X_density;
    uint16_t Y_density;
    int32_t write_Adobe_marker;

    uint32_t next_scanline;

    int32_t progressive_mode;
    int32_t max_h_samp_factor;
    int32_t max_v_samp_factor;
    uint32_t total_iMCU_rows;

    int32_t comps_in_scan;
    void* cur_comp_info[4];

    uint32_t MCUs_per_row;
    uint32_t MCU_rows_in_scan;
    int32_t blocks_in_MCU;
    int32_t MCU_membership[10];

    int32_t Ss;
    int32_t Se;
    int32_t Ah;
    int32_t Al;

    void* master;
    void* main;
    void* prep;
    void* coef;
    void* marker;
    void* cconvert;
    void* downsample;
    void* fdct;
    void* entropy;
    void* script_space;
    int32_t script_space_size;
    uint32_t pad5;
};

static_assert(sizeof(GuestJpegCompress) == 520);
static_assert(offsetof(GuestJpegCompress, dest) == 40);
static_assert(offsetof(GuestJpegCompress, image_width) == 48);
static_assert(offsetof(GuestJpegCompress, input_components) == 56);
static_assert(offsetof(GuestJpegCompress, in_color_space) == 60);
static_assert(offsetof(GuestJpegCompress, input_gamma) == 64);
static_assert(offsetof(GuestJpegCompress, data_precision) == 72);
static_assert(offsetof(GuestJpegCompress, num_components) == 76);
static_assert(offsetof(GuestJpegCompress, jpeg_color_space) == 80);
static_assert(offsetof(GuestJpegCompress, comp_info) == 88);
static_assert(offsetof(GuestJpegCompress, quant_tbl_ptrs) == 96);
static_assert(offsetof(GuestJpegCompress, dc_huff_tbl_ptrs) == 128);
static_assert(offsetof(GuestJpegCompress, ac_huff_tbl_ptrs) == 160);
static_assert(offsetof(GuestJpegCompress, arith_dc_L) == 192);
static_assert(offsetof(GuestJpegCompress, num_scans) == 240);
static_assert(offsetof(GuestJpegCompress, scan_info) == 248);
static_assert(offsetof(GuestJpegCompress, raw_data_in) == 256);
static_assert(offsetof(GuestJpegCompress, smoothing_factor) == 272);
static_assert(offsetof(GuestJpegCompress, dct_method) == 276);
static_assert(offsetof(GuestJpegCompress, restart_interval) == 280);
static_assert(offsetof(GuestJpegCompress, restart_in_rows) == 284);
static_assert(offsetof(GuestJpegCompress, write_JFIF_header) == 288);
static_assert(offsetof(GuestJpegCompress, JFIF_major_version) == 292);
static_assert(offsetof(GuestJpegCompress, X_density) == 296);
static_assert(offsetof(GuestJpegCompress, write_Adobe_marker) == 300);
static_assert(offsetof(GuestJpegCompress, next_scanline) == 304);
static_assert(offsetof(GuestJpegCompress, progressive_mode) == 308);
static_assert(offsetof(GuestJpegCompress, max_h_samp_factor) == 312);
static_assert(offsetof(GuestJpegCompress, total_iMCU_rows) == 320);
static_assert(offsetof(GuestJpegCompress, comps_in_scan) == 324);
static_assert(offsetof(GuestJpegCompress, cur_comp_info) == 328);
static_assert(offsetof(GuestJpegCompress, MCUs_per_row) == 360);
static_assert(offsetof(GuestJpegCompress, blocks_in_MCU) == 368);
static_assert(offsetof(GuestJpegCompress, MCU_membership) == 372);
static_assert(offsetof(GuestJpegCompress, Ss) == 412);
static_assert(offsetof(GuestJpegCompress, master) == 432);
static_assert(offsetof(GuestJpegCompress, entropy) == 496);
static_assert(offsetof(GuestJpegCompress, script_space) == 504);
static_assert(offsetof(GuestJpegCompress, script_space_size) == 512);

}

#endif
