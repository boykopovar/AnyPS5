#pragma once

#include "prx/libSceVshJpeg/include/GuestJpegCompress.hpp"

extern "C" {

void APS5_VABI jpeg_CreateCompress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, int version, size_t structsize);
void APS5_VABI jpeg_destroy_compress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo);
void APS5_VABI jpeg_set_defaults_nid_postfix(VshJpeg::GuestJpegCompress* cinfo);
void APS5_VABI jpeg_set_quality_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, int quality, int force_baseline);
void APS5_VABI jpeg_start_compress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, int write_all_tables);
std::uint32_t APS5_VABI jpeg_write_scanlines_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, std::uint8_t** scanlines, std::uint32_t num_lines);
void APS5_VABI jpeg_finish_compress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo);
void APS5_VABI jpeg_abort_nid_postfix(NKWebKit::GuestCommonPtr cinfo);
void APS5_VABI jpeg_stdio_dest_nid_postfix(VshJpeg::GuestCompressPtr cinfo, void* outfile);
void APS5_VABI jpeg_stdio_src_nid_postfix(NKWebKit::GuestDecompressPtr cinfo, void* infile);

}
