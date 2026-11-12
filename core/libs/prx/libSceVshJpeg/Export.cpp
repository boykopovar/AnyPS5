#include "prx/libSceNKWebKitRequirements/include/Identified.hpp"
#include "prx/libSceVshJpeg/include/Compress.hpp"
#include "prx/libc/include/General.hpp"

#include <cstdio>

extern "C" {

void APS5_VABI jpeg_abort_nid_postfix(NKWebKit::GuestCommonPtr cinfo) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI jpeg_stdio_dest_nid_postfix(VshJpeg::GuestCompressPtr cinfo, void* outfile) {
    NotImplemented_nid_no_patch(__func__);
}

void APS5_VABI jpeg_stdio_src_nid_postfix(NKWebKit::GuestDecompressPtr cinfo, void* infile) {
    NotImplemented_nid_no_patch(__func__);
}

}
