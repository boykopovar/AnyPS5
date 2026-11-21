#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI png_create_info_struct();
int APS5_VABI png_create_read_struct();
int APS5_VABI png_create_write_struct();
int APS5_VABI png_destroy_read_struct();
int APS5_VABI png_destroy_write_struct();
int APS5_VABI png_error();
int APS5_VABI png_get_IHDR();
int APS5_VABI png_get_PLTE();
int APS5_VABI png_get_bit_depth();
int APS5_VABI png_get_channels();
int APS5_VABI png_get_color_type();
int APS5_VABI png_get_error_ptr();
int APS5_VABI png_get_gAMA();
int APS5_VABI png_get_iCCP();
int APS5_VABI png_get_image_height();
int APS5_VABI png_get_image_width();
int APS5_VABI png_get_interlace_type();
int APS5_VABI png_get_io_ptr();
int APS5_VABI png_get_progressive_ptr();
int APS5_VABI png_get_tRNS();
int APS5_VABI png_get_user_chunk_ptr();
int APS5_VABI png_get_valid();
int APS5_VABI png_init_io();
int APS5_VABI png_process_data();
int APS5_VABI png_process_data_pause();
int APS5_VABI png_progressive_combine_row();
int APS5_VABI png_read_end();
int APS5_VABI png_read_image();
int APS5_VABI png_read_info();
int APS5_VABI png_read_row();
int APS5_VABI png_read_update_info();
int APS5_VABI png_save_uint_16();
int APS5_VABI png_save_uint_32();
int APS5_VABI png_set_IHDR();
int APS5_VABI png_set_bKGD();
int APS5_VABI png_set_chunk_malloc_max();
int APS5_VABI png_set_crc_action();
int APS5_VABI png_set_expand();
int APS5_VABI png_set_expand_gray_1_2_4_to_8();
int APS5_VABI png_set_filler();
int APS5_VABI png_set_filter();
int APS5_VABI png_set_gAMA();
int APS5_VABI png_set_gamma();
int APS5_VABI png_set_gray_to_rgb();
int APS5_VABI png_set_interlace_handling();
int APS5_VABI png_set_keep_unknown_chunks();
int APS5_VABI png_set_longjmp_fn();
int APS5_VABI png_set_packing();
int APS5_VABI png_set_packswap();
int APS5_VABI png_set_palette_to_rgb();
int APS5_VABI png_set_progressive_read_fn();
int APS5_VABI png_set_read_fn();
int APS5_VABI png_set_read_user_chunk_fn();
int APS5_VABI png_set_read_user_transform_fn();
int APS5_VABI png_set_strip_16();
int APS5_VABI png_set_swap();
int APS5_VABI png_set_tRNS_to_alpha();
int APS5_VABI png_set_write_fn();
int APS5_VABI png_set_write_user_transform_fn();
int APS5_VABI png_write_end();
int APS5_VABI png_write_image();
int APS5_VABI png_write_info();

int APS5_VABI libpng16Unknown_KfijsxTYXjA_nid_no_patch();
int APS5_VABI libpng16Unknown_aW9EOzLYcpE_nid_no_patch();
int APS5_VABI libpng16Unknown_dE_plus__plus_k4zc9go_nid_no_patch();
int APS5_VABI libpng16Unknown_npOsN_plus__minus_i1Vg_nid_no_patch();
int APS5_VABI libpng16Unknown_tdUy1yBytBk_nid_no_patch();

}
