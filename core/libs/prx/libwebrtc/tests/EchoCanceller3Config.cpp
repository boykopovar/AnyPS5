#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <source_location>
#include <stdexcept>

namespace {

void Require(bool condition, std::source_location location = std::source_location::current()) {
    if (condition) return;
    std::fprintf(stderr, "libwebrtc EchoCanceller3Config check failed at line %u\n", location.line());
    std::abort();
}

template<typename TAction> void RequireInvalidArgument(TAction action, std::source_location location = std::source_location::current()) {
    try {
        action();
    } catch (const std::invalid_argument&) {
        return;
    }
    Require(false, location);
}

struct ByteRange {
    std::size_t first;
    std::size_t last;
};

constexpr ByteRange FirmwareUnwrittenPadding[] {{0x71, 0x77}, {0x14e, 0x14f}, {0x179, 0x17f}, {0x184, 0x187}, {0x249, 0x24f}};

bool MembersEqual(const WebRtcEchoCanceller3Config& left, const WebRtcEchoCanceller3Config& right) {
    for (std::size_t index = 0; index < sizeof(left.bytes); ++index) {
        bool padding = false;
        for (const auto& range : FirmwareUnwrittenPadding) padding = padding || (index >= range.first && index <= range.last);
        if (!padding && left.bytes[index] != right.bytes[index]) return false;
    }
    return true;
}

WebRtcEchoCanceller3Config Pattern(unsigned seed) {
    WebRtcEchoCanceller3Config config;
    for (std::size_t index = 0; index < sizeof(config.bytes); ++index)
        config.bytes[index] = static_cast<unsigned char>(index * 31 + seed);
    return config;
}

template<typename TValue> void RequireDefault(const WebRtcEchoCanceller3Config& config, unsigned offset, TValue expected, const char* member) {
    TValue actual;
    std::memcpy(&actual, config.bytes + offset, sizeof(actual));
    if (actual == expected) return;
    std::fprintf(stderr, "EchoCanceller3Config default mismatch: %s at offset %u\n", member, offset);
    std::abort();
}

void TestDefaultConstructor() {
    struct GuardedConfig {
        WebRtcEchoCanceller3Config config;
        unsigned char guard[16];
    } storage;
    std::memset(&storage, 0xA5, sizeof(storage));
    _ZN6webrtc20EchoCanceller3ConfigC1Ev_nid_postfix(&storage.config);
    RequireDefault<std::size_t>(storage.config, 0, 250, "buffering.excess_render_detection_interval_blocks");
    RequireDefault<std::size_t>(storage.config, 8, 8, "buffering.max_allowed_excess_render_blocks");
    RequireDefault<std::size_t>(storage.config, 16, 5, "delay.default_delay");
    RequireDefault<std::size_t>(storage.config, 24, 4, "delay.down_sampling_factor");
    RequireDefault<std::size_t>(storage.config, 32, 5, "delay.num_filters");
    RequireDefault<std::size_t>(storage.config, 40, 32, "delay.delay_headroom_samples");
    RequireDefault<std::size_t>(storage.config, 48, 1, "delay.hysteresis_limit_blocks");
    RequireDefault<std::size_t>(storage.config, 56, 0, "delay.fixed_capture_delay_samples");
    RequireDefault<float>(storage.config, 64, 0.7f, "delay.delay_estimate_smoothing");
    RequireDefault<float>(storage.config, 68, 0.7f, "delay.delay_estimate_smoothing_delay_found");
    RequireDefault<float>(storage.config, 72, 0.2f, "delay.delay_candidate_detection_threshold");
    RequireDefault<int>(storage.config, 76, 5, "delay.delay_selection_thresholds.initial");
    RequireDefault<int>(storage.config, 80, 20, "delay.delay_selection_thresholds.converged");
    RequireDefault<bool>(storage.config, 84, false, "delay.use_external_delay_estimator");
    RequireDefault<bool>(storage.config, 85, false, "delay.log_warning_on_delay_changes");
    RequireDefault<bool>(storage.config, 88, false, "delay.render_alignment_mixing.downmix");
    RequireDefault<bool>(storage.config, 89, true, "delay.render_alignment_mixing.adaptive_selection");
    RequireDefault<float>(storage.config, 92, 10000.f, "delay.render_alignment_mixing.activity_power_threshold");
    RequireDefault<bool>(storage.config, 96, true, "delay.render_alignment_mixing.prefer_first_two_channels");
    RequireDefault<bool>(storage.config, 100, false, "delay.capture_alignment_mixing.downmix");
    RequireDefault<bool>(storage.config, 101, true, "delay.capture_alignment_mixing.adaptive_selection");
    RequireDefault<float>(storage.config, 104, 10000.f, "delay.capture_alignment_mixing.activity_power_threshold");
    RequireDefault<bool>(storage.config, 108, false, "delay.capture_alignment_mixing.prefer_first_two_channels");
    RequireDefault<bool>(storage.config, 112, true, "delay.detect_pre_echo");
    RequireDefault<std::size_t>(storage.config, 120, 13, "filter.refined.length_blocks");
    RequireDefault<float>(storage.config, 128, 0.00005f, "filter.refined.leakage_converged");
    RequireDefault<float>(storage.config, 132, 0.05f, "filter.refined.leakage_diverged");
    RequireDefault<float>(storage.config, 136, 0.001f, "filter.refined.error_floor");
    RequireDefault<float>(storage.config, 140, 2.f, "filter.refined.error_ceil");
    RequireDefault<float>(storage.config, 144, 20075344.f, "filter.refined.noise_gate");
    RequireDefault<std::size_t>(storage.config, 152, 13, "filter.coarse.length_blocks");
    RequireDefault<float>(storage.config, 160, 0.7f, "filter.coarse.rate");
    RequireDefault<float>(storage.config, 164, 20075344.f, "filter.coarse.noise_gate");
    RequireDefault<std::size_t>(storage.config, 168, 12, "filter.refined_initial.length_blocks");
    RequireDefault<float>(storage.config, 176, 0.005f, "filter.refined_initial.leakage_converged");
    RequireDefault<float>(storage.config, 180, 0.5f, "filter.refined_initial.leakage_diverged");
    RequireDefault<float>(storage.config, 184, 0.001f, "filter.refined_initial.error_floor");
    RequireDefault<float>(storage.config, 188, 2.f, "filter.refined_initial.error_ceil");
    RequireDefault<float>(storage.config, 192, 20075344.f, "filter.refined_initial.noise_gate");
    RequireDefault<std::size_t>(storage.config, 200, 12, "filter.coarse_initial.length_blocks");
    RequireDefault<float>(storage.config, 208, 0.9f, "filter.coarse_initial.rate");
    RequireDefault<float>(storage.config, 212, 20075344.f, "filter.coarse_initial.noise_gate");
    RequireDefault<std::size_t>(storage.config, 216, 250, "filter.config_change_duration_blocks");
    RequireDefault<float>(storage.config, 224, 2.5f, "filter.initial_state_seconds");
    RequireDefault<int>(storage.config, 228, 25, "filter.coarse_reset_hangover_blocks");
    RequireDefault<bool>(storage.config, 232, false, "filter.conservative_initial_phase");
    RequireDefault<bool>(storage.config, 233, true, "filter.enable_coarse_filter_output_usage");
    RequireDefault<bool>(storage.config, 234, true, "filter.use_linear_filter");
    RequireDefault<bool>(storage.config, 235, false, "filter.high_pass_filter_echo_reference");
    RequireDefault<bool>(storage.config, 236, false, "filter.export_linear_aec_output");
    RequireDefault<float>(storage.config, 240, 1.f, "erle.min");
    RequireDefault<float>(storage.config, 244, 4.f, "erle.max_l");
    RequireDefault<float>(storage.config, 248, 1.5f, "erle.max_h");
    RequireDefault<bool>(storage.config, 252, true, "erle.onset_detection");
    RequireDefault<std::size_t>(storage.config, 256, 1, "erle.num_sections");
    RequireDefault<bool>(storage.config, 264, true, "erle.clamp_quality_estimate_to_zero");
    RequireDefault<bool>(storage.config, 265, true, "erle.clamp_quality_estimate_to_one");
    RequireDefault<float>(storage.config, 272, 1.f, "ep_strength.default_gain");
    RequireDefault<float>(storage.config, 276, 0.83f, "ep_strength.default_len");
    RequireDefault<float>(storage.config, 280, 0.83f, "ep_strength.nearend_len");
    RequireDefault<bool>(storage.config, 284, true, "ep_strength.echo_can_saturate");
    RequireDefault<bool>(storage.config, 285, false, "ep_strength.bounded_erl");
    RequireDefault<bool>(storage.config, 286, false, "ep_strength.erle_onset_compensation_in_dominant_nearend");
    RequireDefault<bool>(storage.config, 287, true, "ep_strength.use_conservative_tail_frequency_response");
    RequireDefault<float>(storage.config, 288, 4 * 64.f, "echo_audibility.low_render_limit");
    RequireDefault<float>(storage.config, 292, 64.f, "echo_audibility.normal_render_limit");
    RequireDefault<float>(storage.config, 296, 2 * 64.f, "echo_audibility.floor_power");
    RequireDefault<float>(storage.config, 300, 10, "echo_audibility.audibility_threshold_lf");
    RequireDefault<float>(storage.config, 304, 10, "echo_audibility.audibility_threshold_mf");
    RequireDefault<float>(storage.config, 308, 10, "echo_audibility.audibility_threshold_hf");
    RequireDefault<bool>(storage.config, 312, false, "echo_audibility.use_stationarity_properties");
    RequireDefault<bool>(storage.config, 313, false, "echo_audibility.use_stationarity_properties_at_init");
    RequireDefault<float>(storage.config, 316, 100.f, "render_levels.active_render_limit");
    RequireDefault<float>(storage.config, 320, 150.f, "render_levels.poor_excitation_render_limit");
    RequireDefault<float>(storage.config, 324, 20.f, "render_levels.poor_excitation_render_limit_ds8");
    RequireDefault<float>(storage.config, 328, 0.f, "render_levels.render_power_gain_db");
    RequireDefault<bool>(storage.config, 332, false, "echo_removal_control.has_clock_drift");
    RequireDefault<bool>(storage.config, 333, false, "echo_removal_control.linear_and_stable_echo_path");
    RequireDefault<std::size_t>(storage.config, 336, 50, "echo_model.noise_floor_hold");
    RequireDefault<float>(storage.config, 344, 1638400.f, "echo_model.min_noise_floor_power");
    RequireDefault<float>(storage.config, 348, 10.f, "echo_model.stationary_gate_slope");
    RequireDefault<float>(storage.config, 352, 27509.42f, "echo_model.noise_gate_power");
    RequireDefault<float>(storage.config, 356, 0.3f, "echo_model.noise_gate_slope");
    RequireDefault<std::size_t>(storage.config, 360, 1, "echo_model.render_pre_window_size");
    RequireDefault<std::size_t>(storage.config, 368, 1, "echo_model.render_post_window_size");
    RequireDefault<bool>(storage.config, 376, true, "echo_model.model_reverb_in_nonlinear_mode");
    RequireDefault<float>(storage.config, 384, -96.03406f, "comfort_noise.noise_floor_dbfs");
    RequireDefault<std::size_t>(storage.config, 392, 4, "suppressor.nearend_average_blocks");
    RequireDefault<float>(storage.config, 400, .3f, "suppressor.normal_tuning.mask_lf.enr_transparent");
    RequireDefault<float>(storage.config, 404, .4f, "suppressor.normal_tuning.mask_lf.enr_suppress");
    RequireDefault<float>(storage.config, 408, .3f, "suppressor.normal_tuning.mask_lf.emr_transparent");
    RequireDefault<float>(storage.config, 412, .07f, "suppressor.normal_tuning.mask_hf.enr_transparent");
    RequireDefault<float>(storage.config, 416, .1f, "suppressor.normal_tuning.mask_hf.enr_suppress");
    RequireDefault<float>(storage.config, 420, .3f, "suppressor.normal_tuning.mask_hf.emr_transparent");
    RequireDefault<float>(storage.config, 424, 2.0f, "suppressor.normal_tuning.max_inc_factor");
    RequireDefault<float>(storage.config, 428, 0.25f, "suppressor.normal_tuning.max_dec_factor_lf");
    RequireDefault<float>(storage.config, 432, 1.09f, "suppressor.nearend_tuning.mask_lf.enr_transparent");
    RequireDefault<float>(storage.config, 436, 1.1f, "suppressor.nearend_tuning.mask_lf.enr_suppress");
    RequireDefault<float>(storage.config, 440, .3f, "suppressor.nearend_tuning.mask_lf.emr_transparent");
    RequireDefault<float>(storage.config, 444, .1f, "suppressor.nearend_tuning.mask_hf.enr_transparent");
    RequireDefault<float>(storage.config, 448, .3f, "suppressor.nearend_tuning.mask_hf.enr_suppress");
    RequireDefault<float>(storage.config, 452, .3f, "suppressor.nearend_tuning.mask_hf.emr_transparent");
    RequireDefault<float>(storage.config, 456, 2.0f, "suppressor.nearend_tuning.max_inc_factor");
    RequireDefault<float>(storage.config, 460, 0.25f, "suppressor.nearend_tuning.max_dec_factor_lf");
    RequireDefault<bool>(storage.config, 464, true, "suppressor.lf_smoothing_during_initial_phase");
    RequireDefault<int>(storage.config, 468, 0, "suppressor.last_permanent_lf_smoothing_band");
    RequireDefault<int>(storage.config, 472, 5, "suppressor.last_lf_smoothing_band");
    RequireDefault<int>(storage.config, 476, 5, "suppressor.last_lf_band");
    RequireDefault<int>(storage.config, 480, 8, "suppressor.first_hf_band");
    RequireDefault<float>(storage.config, 484, .25f, "suppressor.dominant_nearend_detection.enr_threshold");
    RequireDefault<float>(storage.config, 488, 10.f, "suppressor.dominant_nearend_detection.enr_exit_threshold");
    RequireDefault<float>(storage.config, 492, 30.f, "suppressor.dominant_nearend_detection.snr_threshold");
    RequireDefault<int>(storage.config, 496, 50, "suppressor.dominant_nearend_detection.hold_duration");
    RequireDefault<int>(storage.config, 500, 12, "suppressor.dominant_nearend_detection.trigger_threshold");
    RequireDefault<bool>(storage.config, 504, true, "suppressor.dominant_nearend_detection.use_during_initial_phase");
    RequireDefault<bool>(storage.config, 505, true, "suppressor.dominant_nearend_detection.use_unbounded_echo_spectrum");
    RequireDefault<std::size_t>(storage.config, 512, 1, "suppressor.subband_nearend_detection.nearend_average_blocks");
    RequireDefault<std::size_t>(storage.config, 520, 1, "suppressor.subband_nearend_detection.subband1.low");
    RequireDefault<std::size_t>(storage.config, 528, 1, "suppressor.subband_nearend_detection.subband1.high");
    RequireDefault<std::size_t>(storage.config, 536, 1, "suppressor.subband_nearend_detection.subband2.low");
    RequireDefault<std::size_t>(storage.config, 544, 1, "suppressor.subband_nearend_detection.subband2.high");
    RequireDefault<float>(storage.config, 552, 1.f, "suppressor.subband_nearend_detection.nearend_threshold");
    RequireDefault<float>(storage.config, 556, 1.f, "suppressor.subband_nearend_detection.snr_threshold");
    RequireDefault<bool>(storage.config, 560, false, "suppressor.use_subband_nearend_detection");
    RequireDefault<float>(storage.config, 564, 1.f, "suppressor.high_bands_suppression.enr_threshold");
    RequireDefault<float>(storage.config, 568, 1.f, "suppressor.high_bands_suppression.max_gain_during_echo");
    RequireDefault<float>(storage.config, 572, 400.f, "suppressor.high_bands_suppression.anti_howling_activation_threshold");
    RequireDefault<float>(storage.config, 576, 1.f, "suppressor.high_bands_suppression.anti_howling_gain");
    RequireDefault<float>(storage.config, 580, 0.00001f, "suppressor.floor_first_increase");
    RequireDefault<bool>(storage.config, 584, false, "suppressor.conservative_hf_suppression");
    RequireDefault<bool>(storage.config, 592, true, "multi_channel.detect_stereo_content");
    RequireDefault<float>(storage.config, 596, 0.0f, "multi_channel.stereo_detection_threshold");
    RequireDefault<int>(storage.config, 600, 300, "multi_channel.stereo_detection_timeout_threshold_seconds");
    RequireDefault<float>(storage.config, 604, 2.0f, "multi_channel.stereo_detection_hysteresis_seconds");
    for (unsigned char byte : storage.guard) Require(byte == 0xA5);
    constexpr ByteRange defaultPadding[] {{86, 87}, {90, 91}, {97, 99}, {102, 103}, {109, 111}, {113, 119}, {148, 151}, {196, 199}, {237, 239}, {253, 255}, {266, 271}, {314, 315}, {334, 335}, {377, 383}, {388, 391}, {465, 467}, {506, 511}, {561, 563}, {585, 591}, {593, 595}};
    for (const auto& range : defaultPadding)
        for (std::size_t index = range.first; index <= range.last; ++index)
            Require(storage.config.bytes[index] == 0xA5);
    for (const auto& range : FirmwareUnwrittenPadding)
        for (std::size_t index = range.first; index <= range.last; ++index)
            Require(storage.config.bytes[index] == 0xA5);
    WebRtcEchoCanceller3Config copy;
    std::memset(&copy, 0x5A, sizeof(copy));
    _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(&copy, &storage.config);
    Require(MembersEqual(copy, storage.config));
    RequireInvalidArgument([] { _ZN6webrtc20EchoCanceller3ConfigC1Ev_nid_postfix(nullptr); });
}

void TestCopyConstructor() {
    WebRtcEchoCanceller3Config source = Pattern(7);
    WebRtcEchoCanceller3Config copy;
    std::memset(&copy, 0xA5, sizeof(copy));
    _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(&copy, &source);
    Require(MembersEqual(copy, source));
}

void TestAssignment() {
    WebRtcEchoCanceller3Config source = Pattern(11);
    WebRtcEchoCanceller3Config target = Pattern(3);
    Require(_ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(&target, &source) == &target);
    Require(MembersEqual(target, source));
    const WebRtcEchoCanceller3Config assigned = target;
    Require(_ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(&target, &target) == &target);
    Require(std::memcmp(&target, &assigned, sizeof(target)) == 0);
}

void TestNullArgumentsThrow() {
    WebRtcEchoCanceller3Config config = Pattern(5);
    WebRtcEchoCanceller3Config expected = config;
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(nullptr, &config); });
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(&config, nullptr); });
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(nullptr, &config); });
    RequireInvalidArgument([&] { _ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(&config, nullptr); });
    Require(std::memcmp(&config, &expected, sizeof(config)) == 0);
}

}

int main() {
    TestDefaultConstructor();
    TestCopyConstructor();
    TestAssignment();
    TestNullArgumentsThrow();
    return 0;
}
