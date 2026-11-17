#include "prx/libSceNKWebKitRequirements/libSceNKWebKitRequirements.hpp"

#include <csetjmp>
#include <cstddef>
#include <cstdint>
#include <cstdio>

extern "C" {
#include <jpeglib.h>
}

namespace {

struct Failure {
    int code = 0;
    int trace = 0;
    int64_t warnings = 0;
    int calls = 0;
    bool operator==(const Failure&) const = default;
};

struct HostContext {
    std::jmp_buf jump;
    Failure failure;
};

void HostOutput(j_common_ptr) {}

void HostError(j_common_ptr cinfo) {
    auto& context = *static_cast<HostContext*>(cinfo->client_data);
    context.failure = {cinfo->err->msg_code, cinfo->err->trace_level, cinfo->err->num_warnings, context.failure.calls + 1};
    std::longjmp(context.jump, 1);
}

bool ReadHost(j_decompress_ptr cinfo, HostContext* context, const uint8_t* input, std::size_t size, bool badVersion) {
    if (setjmp(context->jump) != 0) return false;
    if (badVersion) jpeg_CreateDecompress(cinfo, JPEG_LIB_VERSION - 1, sizeof(*cinfo));
    else {
        jpeg_mem_src(cinfo, input, size);
        jpeg_read_header(cinfo, TRUE);
    }
    return true;
}

void APS5_VABI GuestOutput(NKWebKit::GuestCommonPtr) {}

void APS5_VABI GuestError(NKWebKit::GuestCommonPtr cinfo) {
    auto& failure = *static_cast<Failure*>(cinfo->client_data);
    failure = {cinfo->err->msg_code, cinfo->err->trace_level, cinfo->err->num_warnings, failure.calls + 1};
}

void APS5_VABI InitSource(NKWebKit::GuestDecompressPtr) {}
int32_t APS5_VABI FillSource(NKWebKit::GuestDecompressPtr) { return 0; }
void APS5_VABI TermSource(NKWebKit::GuestDecompressPtr) {}

void APS5_VABI SkipSource(NKWebKit::GuestDecompressPtr cinfo, int64_t count) {
    if (count <= 0) return;
    const uint64_t take = static_cast<uint64_t>(count) < cinfo->src->bytes_in_buffer ? count : cinfo->src->bytes_in_buffer;
    cinfo->src->next_input_byte += take;
    cinfo->src->bytes_in_buffer -= take;
}

int32_t APS5_VABI ResyncSource(NKWebKit::GuestDecompressPtr cinfo, int32_t desired) {
    return jpeg_resync_to_restart_nid_postfix(cinfo, desired);
}

bool Check(const char* name, const uint8_t* input, std::size_t size, int traceLevel, int expectedWarnings, bool badVersion) {
    jpeg_error_mgr hostError{};
    jpeg_decompress_struct host{};
    host.err = jpeg_std_error(&hostError);
    if (!badVersion) jpeg_create_decompress(&host);
    HostContext hostContext{};
    host.client_data = &hostContext;
    hostError.error_exit = HostError;
    hostError.output_message = HostOutput;
    hostError.trace_level = traceLevel;
    hostError.num_warnings = 17;
    const bool hostReturned = ReadHost(&host, &hostContext, input, size, badVersion);

    NKWebKit::GuestJpegErrorMgr guestError{};
    NKWebKit::GuestJpegDecompress guest{};
    guest.err = jpeg_std_error_nid_postfix(&guestError);
    if (!badVersion) jpeg_CreateDecompress_nid_postfix(&guest, JPEG_LIB_VERSION, sizeof(guest));
    Failure observed;
    guest.client_data = &observed;
    guestError.error_exit = GuestError;
    guestError.output_message = GuestOutput;
    guestError.trace_level = traceLevel;
    guestError.num_warnings = 17;
    NKWebKit::GuestJpegSourceMgr source{};
    source.next_input_byte = input;
    source.bytes_in_buffer = size;
    source.init_source = InitSource;
    source.fill_input_buffer = FillSource;
    source.skip_input_data = SkipSource;
    source.resync_to_restart = ResyncSource;
    source.term_source = TermSource;
    guest.src = &source;
    int result = 0;
    if (badVersion) jpeg_CreateDecompress_nid_postfix(&guest, JPEG_LIB_VERSION - 1, sizeof(guest));
    else result = jpeg_read_header_nid_postfix(&guest, 1);
    const auto& expected = hostContext.failure;
    const bool passed = !hostReturned && result == 0 && expected.calls == 1 && expected.warnings == expectedWarnings
        && observed == expected && guestError.num_warnings == hostError.num_warnings && guestError.trace_level == hostError.trace_level;
    if (!passed) std::fprintf(stderr, "JPEG %s at trace %d: expected code=%d warnings=%lld trace=%d, observed code=%d warnings=%lld trace=%d\n",
        name, traceLevel, expected.code, static_cast<long long>(expected.warnings), expected.trace,
        observed.code, static_cast<long long>(observed.warnings), observed.trace);
    jpeg_destroy_decompress_nid_postfix(&guest);
    jpeg_destroy_decompress(&host);
    return passed;
}

} // namespace

int main() {
    constexpr uint8_t notJpeg[]{'N', 'O', 'T', ' ', 'J', 'P', 'E', 'G'};
    constexpr uint8_t warnedHeader[]{0xff, 0xd8, 0x42, 0xff, 0xc0, 0, 11, 0, 0, 1, 0, 1, 1, 1, 0x11, 0, 0xff, 0xd9};
    constexpr int traceLevels[]{0, 2, 3};
    int matched = 0;
    for (const int traceLevel : traceLevels) {
        if (Check("bad header", notJpeg, sizeof(notJpeg), traceLevel, 0, false)) ++matched;
        if (Check("warning before fatal", warnedHeader, sizeof(warnedHeader), traceLevel, 1, false)) ++matched;
        if (Check("bad create version", nullptr, 0, traceLevel, 17, true)) ++matched;
    }
    std::printf("JPEG fatal error state: %d/9 cases match bundled libjpeg\n", matched);
    return matched == 9 ? 0 : 1;
}
