#include "prx/libSceVshJpeg/include/GuestJpegCompress.hpp"
#include "prx/libSceVshJpeg/include/Compress.hpp"
#include "prx/libc/include/General.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <unordered_map>

extern "C" {
#include <stdio.h>
#include <jpeglib.h>
#include <jerror.h>
}

namespace {

struct CompressBinding {
    VshJpeg::GuestJpegCompress* guest_cinfo = nullptr;
    jpeg_compress_struct host_cinfo{};
    jpeg_error_mgr host_err{};
    jpeg_destination_mgr host_dest{};
    VshJpeg::GuestJpegMemoryMgr guest_mem{};
    void** jump = nullptr;
};

std::mutex g_compressMutex;
std::unordered_map<VshJpeg::GuestJpegCompress*, std::unique_ptr<CompressBinding>> g_compressBindings;

[[noreturn]] void HostErrorExit(j_common_ptr cinfo) {
    auto* b = static_cast<CompressBinding*>(cinfo->client_data);
    if (!b || !b->jump) std::abort();
    __builtin_longjmp(b->jump, 1);
}

template <typename Call>
bool RunGuarded(CompressBinding* b, Call call) {
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

void CopyMessageToGuest(CompressBinding* b, NKWebKit::GuestJpegErrorMgr* gerr) {
    gerr->msg_code = b->host_err.msg_code;
    for (int i = 0; i < 8; ++i) gerr->msg_parm.i[i] = b->host_err.msg_parm.i[i];
}

NKWebKit::GuestJpegErrorMgr* GuestErrorOf(j_common_ptr cinfo, CompressBinding** binding) {
    auto* b = static_cast<CompressBinding*>(cinfo->client_data);
    *binding = b;
    if (!b || !b->guest_cinfo) return nullptr;
    return b->guest_cinfo->err;
}

void HostEmitMessage(j_common_ptr cinfo, int msg_level) {
    CompressBinding* b = nullptr;
    auto* gerr = GuestErrorOf(cinfo, &b);
    if (!gerr) return;
    CopyMessageToGuest(b, gerr);
    if (gerr->emit_message) gerr->emit_message(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo), msg_level);
    b->host_err.trace_level = gerr->trace_level;
}

void HostOutputMessage(j_common_ptr cinfo) {
    CompressBinding* b = nullptr;
    auto* gerr = GuestErrorOf(cinfo, &b);
    if (!gerr) return;
    CopyMessageToGuest(b, gerr);
    if (gerr->output_message) gerr->output_message(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo));
}

void HostFormatMessage(j_common_ptr cinfo, char* buffer) {
    CompressBinding* b = nullptr;
    auto* gerr = GuestErrorOf(cinfo, &b);
    if (!gerr) return;
    CopyMessageToGuest(b, gerr);
    if (gerr->format_message) gerr->format_message(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo), buffer);
}

void HostResetErrorMgr(j_common_ptr cinfo) {
    CompressBinding* b = nullptr;
    auto* gerr = GuestErrorOf(cinfo, &b);
    if (!gerr) return;
    if (gerr->reset_error_mgr) gerr->reset_error_mgr(reinterpret_cast<NKWebKit::GuestCommonPtr>(b->guest_cinfo));
}

void SyncDestinationToGuest(CompressBinding* b, VshJpeg::GuestJpegDestinationMgr* gdest) {
    gdest->next_output_byte = b->host_dest.next_output_byte;
    gdest->free_in_buffer = static_cast<std::uint64_t>(b->host_dest.free_in_buffer);
}

void SyncDestinationFromGuest(CompressBinding* b, const VshJpeg::GuestJpegDestinationMgr* gdest) {
    b->host_dest.next_output_byte = gdest->next_output_byte;
    b->host_dest.free_in_buffer = static_cast<std::size_t>(gdest->free_in_buffer);
}

void HostInitDestination(j_compress_ptr cinfo) {
    auto* b = static_cast<CompressBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->dest) return;
    auto* gdest = b->guest_cinfo->dest;
    SyncDestinationToGuest(b, gdest);
    if (gdest->init_destination) gdest->init_destination(b->guest_cinfo);
    SyncDestinationFromGuest(b, gdest);
}

boolean HostEmptyOutputBuffer(j_compress_ptr cinfo) {
    auto* b = static_cast<CompressBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->dest) return FALSE;
    auto* gdest = b->guest_cinfo->dest;
    std::int32_t ret = 0;
    SyncDestinationToGuest(b, gdest);
    if (gdest->empty_output_buffer) ret = gdest->empty_output_buffer(b->guest_cinfo);
    SyncDestinationFromGuest(b, gdest);
    return ret ? TRUE : FALSE;
}

void HostTermDestination(j_compress_ptr cinfo) {
    auto* b = static_cast<CompressBinding*>(cinfo->client_data);
    if (!b || !b->guest_cinfo || !b->guest_cinfo->dest) return;
    auto* gdest = b->guest_cinfo->dest;
    SyncDestinationToGuest(b, gdest);
    if (gdest->term_destination) gdest->term_destination(b->guest_cinfo);
    SyncDestinationFromGuest(b, gdest);
}

CompressBinding* FindBinding(VshJpeg::GuestJpegCompress* cinfo) {
    if (!cinfo) return nullptr;
    std::lock_guard<std::mutex> lock(g_compressMutex);
    const auto it = g_compressBindings.find(cinfo);
    return it == g_compressBindings.end() ? nullptr : it->second.get();
}

CompressBinding* RequireBinding(VshJpeg::GuestJpegCompress* cinfo) {
    if (!cinfo) throw std::invalid_argument("libjpeg: null compression object");
    auto* b = FindBinding(cinfo);
    if (!b) throw std::invalid_argument("libjpeg: the compression object was not created by jpeg_CreateCompress");
    return b;
}

void SyncGuestToHost(CompressBinding* b) {
    const auto* g = b->guest_cinfo;
    auto* h = &b->host_cinfo;
    if (g->err) b->host_err.trace_level = g->err->trace_level;
    if (g->dest) SyncDestinationFromGuest(b, g->dest);
    h->image_width = g->image_width;
    h->image_height = g->image_height;
    h->input_components = g->input_components;
    h->in_color_space = static_cast<J_COLOR_SPACE>(g->in_color_space);
    h->input_gamma = g->input_gamma;
    h->data_precision = g->data_precision;
    h->num_components = g->num_components;
    h->jpeg_color_space = static_cast<J_COLOR_SPACE>(g->jpeg_color_space);
    h->comp_info = static_cast<jpeg_component_info*>(g->comp_info);
    for (int i = 0; i < 4; ++i) {
        h->quant_tbl_ptrs[i] = static_cast<JQUANT_TBL*>(g->quant_tbl_ptrs[i]);
        h->dc_huff_tbl_ptrs[i] = static_cast<JHUFF_TBL*>(g->dc_huff_tbl_ptrs[i]);
        h->ac_huff_tbl_ptrs[i] = static_cast<JHUFF_TBL*>(g->ac_huff_tbl_ptrs[i]);
    }
    for (int i = 0; i < 16; ++i) {
        h->arith_dc_L[i] = g->arith_dc_L[i];
        h->arith_dc_U[i] = g->arith_dc_U[i];
        h->arith_ac_K[i] = g->arith_ac_K[i];
    }
    h->num_scans = g->num_scans;
    h->scan_info = static_cast<const jpeg_scan_info*>(g->scan_info);
    h->raw_data_in = g->raw_data_in ? TRUE : FALSE;
    h->arith_code = g->arith_code ? TRUE : FALSE;
    h->optimize_coding = g->optimize_coding ? TRUE : FALSE;
    h->CCIR601_sampling = g->CCIR601_sampling ? TRUE : FALSE;
    h->smoothing_factor = g->smoothing_factor;
    h->dct_method = static_cast<J_DCT_METHOD>(g->dct_method);
    h->restart_interval = g->restart_interval;
    h->restart_in_rows = g->restart_in_rows;
    h->write_JFIF_header = g->write_JFIF_header ? TRUE : FALSE;
    h->JFIF_major_version = g->JFIF_major_version;
    h->JFIF_minor_version = g->JFIF_minor_version;
    h->density_unit = g->density_unit;
    h->X_density = g->X_density;
    h->Y_density = g->Y_density;
    h->write_Adobe_marker = g->write_Adobe_marker ? TRUE : FALSE;
}

void SyncHostToGuest(CompressBinding* b) {
    auto* g = b->guest_cinfo;
    const auto* h = &b->host_cinfo;
    if (g->dest) SyncDestinationToGuest(b, g->dest);
    g->image_width = h->image_width;
    g->image_height = h->image_height;
    g->input_components = h->input_components;
    g->in_color_space = static_cast<std::int32_t>(h->in_color_space);
    g->input_gamma = h->input_gamma;
    g->data_precision = h->data_precision;
    g->num_components = h->num_components;
    g->jpeg_color_space = static_cast<std::int32_t>(h->jpeg_color_space);
    g->comp_info = h->comp_info;
    for (int i = 0; i < 4; ++i) {
        g->quant_tbl_ptrs[i] = h->quant_tbl_ptrs[i];
        g->dc_huff_tbl_ptrs[i] = h->dc_huff_tbl_ptrs[i];
        g->ac_huff_tbl_ptrs[i] = h->ac_huff_tbl_ptrs[i];
        g->cur_comp_info[i] = h->cur_comp_info[i];
    }
    for (int i = 0; i < 16; ++i) {
        g->arith_dc_L[i] = h->arith_dc_L[i];
        g->arith_dc_U[i] = h->arith_dc_U[i];
        g->arith_ac_K[i] = h->arith_ac_K[i];
    }
    g->num_scans = h->num_scans;
    g->scan_info = h->scan_info;
    g->raw_data_in = h->raw_data_in ? 1 : 0;
    g->arith_code = h->arith_code ? 1 : 0;
    g->optimize_coding = h->optimize_coding ? 1 : 0;
    g->CCIR601_sampling = h->CCIR601_sampling ? 1 : 0;
    g->smoothing_factor = h->smoothing_factor;
    g->dct_method = static_cast<std::int32_t>(h->dct_method);
    g->restart_interval = h->restart_interval;
    g->restart_in_rows = h->restart_in_rows;
    g->write_JFIF_header = h->write_JFIF_header ? 1 : 0;
    g->JFIF_major_version = h->JFIF_major_version;
    g->JFIF_minor_version = h->JFIF_minor_version;
    g->density_unit = h->density_unit;
    g->X_density = h->X_density;
    g->Y_density = h->Y_density;
    g->write_Adobe_marker = h->write_Adobe_marker ? 1 : 0;
    g->next_scanline = h->next_scanline;
    g->progressive_mode = h->progressive_mode ? 1 : 0;
    g->max_h_samp_factor = h->max_h_samp_factor;
    g->max_v_samp_factor = h->max_v_samp_factor;
    g->total_iMCU_rows = h->total_iMCU_rows;
    g->comps_in_scan = h->comps_in_scan;
    g->MCUs_per_row = h->MCUs_per_row;
    g->MCU_rows_in_scan = h->MCU_rows_in_scan;
    g->blocks_in_MCU = h->blocks_in_MCU;
    for (int i = 0; i < 10; ++i) g->MCU_membership[i] = h->MCU_membership[i];
    g->Ss = h->Ss;
    g->Se = h->Se;
    g->Ah = h->Ah;
    g->Al = h->Al;
    g->global_state = h->global_state;
    g->is_decompressor = h->is_decompressor ? 1 : 0;
}

void ForwardError(CompressBinding* b) {
    auto* g = b->guest_cinfo;
    if (!g->err) return;
    CopyMessageToGuest(b, g->err);
    g->err->num_warnings = b->host_err.num_warnings;
    g->err->trace_level = b->host_err.trace_level;
    if (g->err->error_exit) g->err->error_exit(reinterpret_cast<NKWebKit::GuestCommonPtr>(g));
}

void HandleException(CompressBinding* b) {
    SyncHostToGuest(b);
    ForwardError(b);
}

template <typename Call>
bool Guarded(CompressBinding* b, Call call) {
    if (b->guest_cinfo->progress) NotImplemented_nid_no_patch("jpeg_compress_struct progress monitor");
    SyncGuestToHost(b);
    if (!RunGuarded(b, call)) {
        HandleException(b);
        return false;
    }
    SyncHostToGuest(b);
    return true;
}


template <typename R, typename Call>
R MemoryCall(VshJpeg::GuestJpegCompress* cinfo, Call call) {
    auto* b = RequireBinding(cinfo);
    R result{};
    if (!RunGuarded(b, [&] { result = call(b, reinterpret_cast<j_common_ptr>(&b->host_cinfo)); })) {
        HandleException(b);
        return R{};
    }
    return result;
}

void* APS5_VABI GuestAllocSmall(VshJpeg::GuestCompressPtr cinfo, std::int32_t pool, std::uint64_t size) {
    return MemoryCall<void*>(cinfo, [&](CompressBinding* b, j_common_ptr c) { return b->host_cinfo.mem->alloc_small(c, pool, static_cast<std::size_t>(size)); });
}

void* APS5_VABI GuestAllocLarge(VshJpeg::GuestCompressPtr cinfo, std::int32_t pool, std::uint64_t size) {
    return MemoryCall<void*>(cinfo, [&](CompressBinding* b, j_common_ptr c) { return b->host_cinfo.mem->alloc_large(c, pool, static_cast<std::size_t>(size)); });
}

std::uint8_t** APS5_VABI GuestAllocSarray(VshJpeg::GuestCompressPtr cinfo, std::int32_t pool, std::uint32_t samples, std::uint32_t rows) {
    return MemoryCall<std::uint8_t**>(cinfo, [&](CompressBinding* b, j_common_ptr c) { return reinterpret_cast<std::uint8_t**>(b->host_cinfo.mem->alloc_sarray(c, pool, samples, rows)); });
}

void* APS5_VABI GuestAllocBarray(VshJpeg::GuestCompressPtr cinfo, std::int32_t pool, std::uint32_t blocks, std::uint32_t rows) {
    return MemoryCall<void*>(cinfo, [&](CompressBinding* b, j_common_ptr c) { return static_cast<void*>(b->host_cinfo.mem->alloc_barray(c, pool, blocks, rows)); });
}

void APS5_VABI GuestFreePool(VshJpeg::GuestCompressPtr cinfo, std::int32_t pool) {
    MemoryCall<int>(cinfo, [&](CompressBinding* b, j_common_ptr c) { b->host_cinfo.mem->free_pool(c, pool); return 0; });
}

void* APS5_VABI GuestRequestVirtSarray(VshJpeg::GuestCompressPtr, std::int32_t, std::int32_t, std::uint32_t, std::uint32_t, std::uint32_t) {
    NotImplemented_nid_no_patch("jpeg_memory_mgr request_virt_sarray");
}

void* APS5_VABI GuestRequestVirtBarray(VshJpeg::GuestCompressPtr, std::int32_t, std::int32_t, std::uint32_t, std::uint32_t, std::uint32_t) {
    NotImplemented_nid_no_patch("jpeg_memory_mgr request_virt_barray");
}

void APS5_VABI GuestRealizeVirtArrays(VshJpeg::GuestCompressPtr) {
    NotImplemented_nid_no_patch("jpeg_memory_mgr realize_virt_arrays");
}

std::uint8_t** APS5_VABI GuestAccessVirtSarray(VshJpeg::GuestCompressPtr, void*, std::uint32_t, std::uint32_t, std::int32_t) {
    NotImplemented_nid_no_patch("jpeg_memory_mgr access_virt_sarray");
}

void* APS5_VABI GuestAccessVirtBarray(VshJpeg::GuestCompressPtr, void*, std::uint32_t, std::uint32_t, std::int32_t) {
    NotImplemented_nid_no_patch("jpeg_memory_mgr access_virt_barray");
}

void APS5_VABI GuestSelfDestruct(VshJpeg::GuestCompressPtr) {
    NotImplemented_nid_no_patch("jpeg_memory_mgr self_destruct");
}

void InstallGuestMemory(CompressBinding* b) {
    auto& m = b->guest_mem;
    m.alloc_small = GuestAllocSmall;
    m.alloc_large = GuestAllocLarge;
    m.alloc_sarray = GuestAllocSarray;
    m.alloc_barray = GuestAllocBarray;
    m.request_virt_sarray = GuestRequestVirtSarray;
    m.request_virt_barray = GuestRequestVirtBarray;
    m.realize_virt_arrays = GuestRealizeVirtArrays;
    m.access_virt_sarray = GuestAccessVirtSarray;
    m.access_virt_barray = GuestAccessVirtBarray;
    m.free_pool = GuestFreePool;
    m.self_destruct = GuestSelfDestruct;
    m.max_memory_to_use = b->host_cinfo.mem->max_memory_to_use;
    m.max_alloc_chunk = b->host_cinfo.mem->max_alloc_chunk;
    b->guest_cinfo->mem = &m;
}

}

extern "C" {

void APS5_VABI jpeg_CreateCompress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, int version, size_t structsize) {
    if (!cinfo) throw std::invalid_argument("libjpeg: null compression object");
    std::unique_ptr<CompressBinding> stale;
    {
        std::lock_guard<std::mutex> lock(g_compressMutex);
        const auto it = g_compressBindings.find(cinfo);
        if (it != g_compressBindings.end()) {
            stale = std::move(it->second);
            g_compressBindings.erase(it);
        }
    }
    if (stale) RunGuarded(stale.get(), [&] { jpeg_destroy_compress(&stale->host_cinfo); });
    stale.reset();
    auto b = std::make_unique<CompressBinding>();
    b->guest_cinfo = cinfo;
    b->host_cinfo.err = &b->host_err;
    jpeg_std_error(&b->host_err);
    b->host_err.error_exit = HostErrorExit;
    b->host_err.emit_message = HostEmitMessage;
    b->host_err.output_message = HostOutputMessage;
    b->host_err.format_message = HostFormatMessage;
    b->host_err.reset_error_mgr = HostResetErrorMgr;
    b->host_cinfo.client_data = b.get();

    if (!RunGuarded(b.get(), [&] { jpeg_CreateCompress(&b->host_cinfo, version, sizeof(jpeg_compress_struct)); })) {
        RunGuarded(b.get(), [&] { jpeg_destroy_compress(&b->host_cinfo); });
        ForwardError(b.get());
        return;
    }
    if (structsize != sizeof(VshJpeg::GuestJpegCompress)) {
        jpeg_destroy_compress(&b->host_cinfo);
        b->host_err.msg_code = JERR_BAD_STRUCT_SIZE;
        b->host_err.msg_parm.i[0] = static_cast<int>(sizeof(VshJpeg::GuestJpegCompress));
        b->host_err.msg_parm.i[1] = static_cast<int>(structsize);
        ForwardError(b.get());
        return;
    }

    b->host_cinfo.dest = &b->host_dest;
    b->host_dest.init_destination = HostInitDestination;
    b->host_dest.empty_output_buffer = HostEmptyOutputBuffer;
    b->host_dest.term_destination = HostTermDestination;

    NKWebKit::GuestJpegErrorMgr* const err = cinfo->err;
    void* const clientData = cinfo->client_data;
    std::memset(cinfo, 0, sizeof(*cinfo));
    cinfo->err = err;
    cinfo->client_data = clientData;
    SyncHostToGuest(b.get());
    InstallGuestMemory(b.get());

    std::lock_guard<std::mutex> lock(g_compressMutex);
    g_compressBindings[cinfo] = std::move(b);
}

void APS5_VABI jpeg_destroy_compress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo) {
    if (!cinfo) throw std::invalid_argument("libjpeg: null compression object");
    std::unique_ptr<CompressBinding> b;
    {
        std::lock_guard<std::mutex> lock(g_compressMutex);
        const auto it = g_compressBindings.find(cinfo);
        if (it != g_compressBindings.end()) {
            b = std::move(it->second);
            g_compressBindings.erase(it);
        }
    }
    if (!b) return;
    RunGuarded(b.get(), [&] { jpeg_destroy_compress(&b->host_cinfo); });
    cinfo->global_state = b->host_cinfo.global_state;
    cinfo->mem = nullptr;
}

void APS5_VABI jpeg_set_defaults_nid_postfix(VshJpeg::GuestJpegCompress* cinfo) {
    auto* b = RequireBinding(cinfo);
    Guarded(b, [&] { jpeg_set_defaults(&b->host_cinfo); });
}

void APS5_VABI jpeg_set_quality_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, int quality, int force_baseline) {
    auto* b = RequireBinding(cinfo);
    Guarded(b, [&] { jpeg_set_quality(&b->host_cinfo, quality, force_baseline ? TRUE : FALSE); });
}

void APS5_VABI jpeg_start_compress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, int write_all_tables) {
    auto* b = RequireBinding(cinfo);
    if (!cinfo->dest) throw std::invalid_argument("libjpeg: the compression object has no destination manager");
    Guarded(b, [&] { jpeg_start_compress(&b->host_cinfo, write_all_tables ? TRUE : FALSE); });
}

std::uint32_t APS5_VABI jpeg_write_scanlines_nid_postfix(VshJpeg::GuestJpegCompress* cinfo, std::uint8_t** scanlines, std::uint32_t num_lines) {
    auto* b = RequireBinding(cinfo);
    JDIMENSION written = 0;
    Guarded(b, [&] { written = jpeg_write_scanlines(&b->host_cinfo, reinterpret_cast<JSAMPARRAY>(scanlines), num_lines); });
    return written;
}

void APS5_VABI jpeg_finish_compress_nid_postfix(VshJpeg::GuestJpegCompress* cinfo) {
    auto* b = RequireBinding(cinfo);
    Guarded(b, [&] { jpeg_finish_compress(&b->host_cinfo); });
}

}
