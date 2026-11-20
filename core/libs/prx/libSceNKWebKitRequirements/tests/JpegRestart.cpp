#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <cstdio>
#include <cstdlib>
#include <stdexcept>
#include <vector>

extern "C" {
#include <jpeglib.h>
}

namespace {

enum class Recovery { Delegate, Discard, Suspend, Replace };

struct Event {
    int before;
    int desired;
    int after;
    int result;
    bool operator==(const Event&) const = default;
};

struct Result {
    Recovery recovery;
    std::vector<Event> events;
    std::vector<unsigned char> pixels;
    int suspensions = 0;
};

void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

std::vector<unsigned char> Image(bool progressive, bool color) {
    jpeg_compress_struct info{};
    jpeg_error_mgr error{};
    info.err = jpeg_std_error(&error);
    jpeg_create_compress(&info);
    unsigned char* data = nullptr;
    unsigned long size = 0;
    jpeg_mem_dest(&info, &data, &size);
    info.image_width = 40;
    info.image_height = 24;
    info.input_components = color ? 3 : 1;
    info.in_color_space = color ? JCS_RGB : JCS_GRAYSCALE;
    jpeg_set_defaults(&info);
    info.restart_interval = 1;
    if (progressive) jpeg_simple_progression(&info);
    jpeg_start_compress(&info, TRUE);
    std::vector<unsigned char> row(info.image_width * info.input_components);
    while (info.next_scanline < info.image_height) {
        for (size_t x = 0; x < row.size(); ++x) row[x] = (x * 29 + info.next_scanline * 17) & 255;
        auto* samples = row.data();
        Require(jpeg_write_scanlines(&info, &samples, 1) == 1, "encode scanline");
    }
    jpeg_finish_compress(&info);
    jpeg_destroy_compress(&info);
    std::vector<unsigned char> encoded(data, data + size);
    std::free(data);
    return encoded;
}

template<typename TInfo, typename TCall>
int Recover(TInfo* info, int desired, TCall delegate) {
    auto& result = *static_cast<Result*>(info->client_data);
    const int before = info->unread_marker;
    int status = 1;
    if (result.recovery == Recovery::Suspend && result.events.empty()) status = 0;
    else if (result.recovery == Recovery::Discard || result.recovery == Recovery::Suspend) info->unread_marker = 0;
    else {
        if (result.recovery == Recovery::Replace) info->unread_marker = JPEG_RST0 + desired;
        status = delegate(info, desired);
    }
    result.events.push_back({before, desired, info->unread_marker, status});
    Require(result.events.size() < 200, "restart recovery did not converge");
    return status;
}

boolean NativeRecover(j_decompress_ptr info, int desired) {
    return Recover(info, desired, jpeg_resync_to_restart);
}

int32_t APS5_VABI GuestRecover(NKWebKit::GuestDecompressPtr info, int32_t desired) {
    return Recover(info, desired, jpeg_resync_to_restart_nid_postfix);
}

void NativeOutput(j_common_ptr) {}
void APS5_VABI GuestRestartOutput(NKWebKit::GuestCommonPtr) {}
void APS5_VABI GuestSource(NKWebKit::GuestDecompressPtr) {}
int32_t APS5_VABI GuestFill(NKWebKit::GuestDecompressPtr) { return 0; }

void APS5_VABI GuestSkip(NKWebKit::GuestDecompressPtr info, int64_t count) {
    if (count <= 0) return;
    Require(static_cast<uint64_t>(count) <= info->src->bytes_in_buffer, "unexpected source skip");
    info->src->next_input_byte += count;
    info->src->bytes_in_buffer -= count;
}

Result Native(const std::vector<unsigned char>& data, Recovery recovery) {
    Result result{recovery};
    jpeg_decompress_struct info{};
    jpeg_error_mgr error{};
    info.err = jpeg_std_error(&error);
    error.output_message = NativeOutput;
    jpeg_create_decompress(&info);
    info.client_data = &result;
    jpeg_mem_src(&info, data.data(), data.size());
    info.src->resync_to_restart = NativeRecover;
    Require(jpeg_read_header(&info, TRUE) == JPEG_HEADER_OK, "native header");
    while (!jpeg_start_decompress(&info)) Require(++result.suspensions < 4, "native start suspension");
    const size_t stride = info.output_width * info.output_components;
    result.pixels.resize(stride * info.output_height);
    while (info.output_scanline < info.output_height) {
        auto* row = result.pixels.data() + info.output_scanline * stride;
        if (!jpeg_read_scanlines(&info, &row, 1)) Require(++result.suspensions < 4, "native scanline suspension");
    }
    while (!jpeg_finish_decompress(&info)) Require(++result.suspensions < 4, "native finish suspension");
    jpeg_destroy_decompress(&info);
    return result;
}

Result Guest(const std::vector<unsigned char>& data, Recovery recovery) {
    Result result{recovery};
    NKWebKit::GuestJpegDecompress info{};
    NKWebKit::GuestJpegErrorMgr error{};
    info.err = jpeg_std_error_nid_postfix(&error);
    error.output_message = GuestRestartOutput;
    jpeg_CreateDecompress_nid_postfix(&info, 62, sizeof(info));
    info.client_data = &result;
    NKWebKit::GuestJpegSourceMgr source{};
    source.next_input_byte = data.data();
    source.bytes_in_buffer = data.size();
    source.init_source = GuestSource;
    source.fill_input_buffer = GuestFill;
    source.skip_input_data = GuestSkip;
    source.resync_to_restart = GuestRecover;
    source.term_source = GuestSource;
    info.src = &source;
    Require(jpeg_read_header_nid_postfix(&info, 1) == JPEG_HEADER_OK, "guest header");
    while (!jpeg_start_decompress_nid_postfix(&info)) Require(++result.suspensions < 4, "guest start suspension");
    const size_t stride = info.output_width * info.output_components;
    result.pixels.resize(stride * info.output_height);
    while (info.output_scanline < info.output_height) {
        auto* row = result.pixels.data() + info.output_scanline * stride;
        if (!jpeg_read_scanlines_nid_postfix(&info, &row, 1)) Require(++result.suspensions < 4, "guest scanline suspension");
    }
    while (!jpeg_finish_decompress_nid_postfix(&info)) Require(++result.suspensions < 4, "guest finish suspension");
    jpeg_destroy_decompress_nid_postfix(&info);
    return result;
}

} // namespace

int main() {
    int passed = 0;
    int total = 0;
    for (const bool progressive : {false, true}) {
        for (const bool color : {false, true}) {
            const auto original = Image(progressive, color);
            for (const int offset : {0, 1, 3, 6}) {
                auto encoded = original;
                bool changed = false;
                for (size_t i = 1; i < encoded.size(); ++i) {
                    if (encoded[i - 1] == 0xff && encoded[i] == JPEG_RST0) {
                        encoded[i] += offset;
                        changed = true;
                        break;
                    }
                }
                Require(changed, "fixture has no restart marker");
                for (const auto recovery : {Recovery::Delegate, Recovery::Discard, Recovery::Suspend, Recovery::Replace}) {
                    ++total;
                    const auto native = Native(encoded, recovery);
                    const auto guest = Guest(encoded, recovery);
                    Require((offset == 0) == native.events.empty(), "native recovery control");
                    const bool same = native.events == guest.events && native.pixels == guest.pixels && native.suspensions == guest.suspensions;
                    if (same) ++passed;
                    else std::fprintf(stderr, "Restart mismatch: progressive=%d color=%d offset=%d recovery=%d native_events=%zu guest_events=%zu pixels=%d\n", progressive, color, offset, static_cast<int>(recovery), native.events.size(), guest.events.size(), native.pixels == guest.pixels);
                }
            }
        }
    }
    std::printf("JPEG restart recovery: %d/%d native comparisons passed\n", passed, total);
    return passed == total ? 0 : 1;
}
