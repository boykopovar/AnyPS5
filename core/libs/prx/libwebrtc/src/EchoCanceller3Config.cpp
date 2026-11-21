/*
 *  Copyright (c) 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

/*
 * Copyright (c) 2011, The WebRTC project authors. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *
 *   * Neither the name of Google nor the names of its contributors may
 *     be used to endorse or promote products derived from this software
 *     without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "prx/libwebrtc/libwebrtc.hpp"

#include <cstring>
#include <stdexcept>

extern "C" {

void APS5_VABI _ZN6webrtc20EchoCanceller3ConfigC1Ev_nid_postfix(WebRtcEchoCanceller3Config* config) {
    if (!config)
        throw std::invalid_argument("EchoCanceller3Config constructor requires a config");
    const auto store = [config]<typename TValue>(unsigned offset, TValue value) {
        std::memcpy(config->bytes + offset, &value, sizeof(value));
    };
    using namespace WebRtcEchoCanceller3ConfigOffset;
    store(BufferingExcessRenderDetectionIntervalBlocks, static_cast<std::size_t>(250));
    store(BufferingMaxAllowedExcessRenderBlocks, static_cast<std::size_t>(8));
    store(DelayDefaultDelay, static_cast<std::size_t>(5));
    store(DelayDownSamplingFactor, static_cast<std::size_t>(4));
    store(DelayNumFilters, static_cast<std::size_t>(5));
    store(DelayDelayHeadroomSamples, static_cast<std::size_t>(32));
    store(DelayHysteresisLimitBlocks, static_cast<std::size_t>(1));
    store(DelayFixedCaptureDelaySamples, static_cast<std::size_t>(0));
    store(DelayDelayEstimateSmoothing, static_cast<float>(0.7f));
    store(DelayDelayEstimateSmoothingDelayFound, static_cast<float>(0.7f));
    store(DelayDelayCandidateDetectionThreshold, static_cast<float>(0.2f));
    store(DelayDelaySelectionThresholdsInitial, static_cast<int>(5));
    store(DelayDelaySelectionThresholdsConverged, static_cast<int>(20));
    store(DelayUseExternalDelayEstimator, static_cast<bool>(false));
    store(DelayLogWarningOnDelayChanges, static_cast<bool>(false));
    store(DelayRenderAlignmentMixingDownmix, static_cast<bool>(false));
    store(DelayRenderAlignmentMixingAdaptiveSelection, static_cast<bool>(true));
    store(DelayRenderAlignmentMixingActivityPowerThreshold, static_cast<float>(10000.f));
    store(DelayRenderAlignmentMixingPreferFirstTwoChannels, static_cast<bool>(true));
    store(DelayCaptureAlignmentMixingDownmix, static_cast<bool>(false));
    store(DelayCaptureAlignmentMixingAdaptiveSelection, static_cast<bool>(true));
    store(DelayCaptureAlignmentMixingActivityPowerThreshold, static_cast<float>(10000.f));
    store(DelayCaptureAlignmentMixingPreferFirstTwoChannels, static_cast<bool>(false));
    store(DelayDetectPreEcho, static_cast<bool>(true));
    store(FilterRefinedLengthBlocks, static_cast<std::size_t>(13));
    store(FilterRefinedLeakageConverged, static_cast<float>(0.00005f));
    store(FilterRefinedLeakageDiverged, static_cast<float>(0.05f));
    store(FilterRefinedErrorFloor, static_cast<float>(0.001f));
    store(FilterRefinedErrorCeil, static_cast<float>(2.f));
    store(FilterRefinedNoiseGate, static_cast<float>(20075344.f));
    store(FilterCoarseLengthBlocks, static_cast<std::size_t>(13));
    store(FilterCoarseRate, static_cast<float>(0.7f));
    store(FilterCoarseNoiseGate, static_cast<float>(20075344.f));
    store(FilterRefinedInitialLengthBlocks, static_cast<std::size_t>(12));
    store(FilterRefinedInitialLeakageConverged, static_cast<float>(0.005f));
    store(FilterRefinedInitialLeakageDiverged, static_cast<float>(0.5f));
    store(FilterRefinedInitialErrorFloor, static_cast<float>(0.001f));
    store(FilterRefinedInitialErrorCeil, static_cast<float>(2.f));
    store(FilterRefinedInitialNoiseGate, static_cast<float>(20075344.f));
    store(FilterCoarseInitialLengthBlocks, static_cast<std::size_t>(12));
    store(FilterCoarseInitialRate, static_cast<float>(0.9f));
    store(FilterCoarseInitialNoiseGate, static_cast<float>(20075344.f));
    store(FilterConfigChangeDurationBlocks, static_cast<std::size_t>(250));
    store(FilterInitialStateSeconds, static_cast<float>(2.5f));
    store(FilterCoarseResetHangoverBlocks, static_cast<int>(25));
    store(FilterConservativeInitialPhase, static_cast<bool>(false));
    store(FilterEnableCoarseFilterOutputUsage, static_cast<bool>(true));
    store(FilterUseLinearFilter, static_cast<bool>(true));
    store(FilterHighPassFilterEchoReference, static_cast<bool>(false));
    store(FilterExportLinearAecOutput, static_cast<bool>(false));
    store(ErleMin, static_cast<float>(1.f));
    store(ErleMaxL, static_cast<float>(4.f));
    store(ErleMaxH, static_cast<float>(1.5f));
    store(ErleOnsetDetection, static_cast<bool>(true));
    store(ErleNumSections, static_cast<std::size_t>(1));
    store(ErleClampQualityEstimateToZero, static_cast<bool>(true));
    store(ErleClampQualityEstimateToOne, static_cast<bool>(true));
    store(EpStrengthDefaultGain, static_cast<float>(1.f));
    store(EpStrengthDefaultLen, static_cast<float>(0.83f));
    store(EpStrengthNearendLen, static_cast<float>(0.83f));
    store(EpStrengthEchoCanSaturate, static_cast<bool>(true));
    store(EpStrengthBoundedErl, static_cast<bool>(false));
    store(EpStrengthErleOnsetCompensationInDominantNearend, static_cast<bool>(false));
    store(EpStrengthUseConservativeTailFrequencyResponse, static_cast<bool>(true));
    store(EchoAudibilityLowRenderLimit, static_cast<float>(4 * 64.f));
    store(EchoAudibilityNormalRenderLimit, static_cast<float>(64.f));
    store(EchoAudibilityFloorPower, static_cast<float>(2 * 64.f));
    store(EchoAudibilityAudibilityThresholdLf, static_cast<float>(10));
    store(EchoAudibilityAudibilityThresholdMf, static_cast<float>(10));
    store(EchoAudibilityAudibilityThresholdHf, static_cast<float>(10));
    store(EchoAudibilityUseStationarityProperties, static_cast<bool>(false));
    store(EchoAudibilityUseStationarityPropertiesAtInit, static_cast<bool>(false));
    store(RenderLevelsActiveRenderLimit, static_cast<float>(100.f));
    store(RenderLevelsPoorExcitationRenderLimit, static_cast<float>(150.f));
    store(RenderLevelsPoorExcitationRenderLimitDs8, static_cast<float>(20.f));
    store(RenderLevelsRenderPowerGainDb, static_cast<float>(0.f));
    store(EchoRemovalControlHasClockDrift, static_cast<bool>(false));
    store(EchoRemovalControlLinearAndStableEchoPath, static_cast<bool>(false));
    store(EchoModelNoiseFloorHold, static_cast<std::size_t>(50));
    store(EchoModelMinNoiseFloorPower, static_cast<float>(1638400.f));
    store(EchoModelStationaryGateSlope, static_cast<float>(10.f));
    store(EchoModelNoiseGatePower, static_cast<float>(27509.42f));
    store(EchoModelNoiseGateSlope, static_cast<float>(0.3f));
    store(EchoModelRenderPreWindowSize, static_cast<std::size_t>(1));
    store(EchoModelRenderPostWindowSize, static_cast<std::size_t>(1));
    store(EchoModelModelReverbInNonlinearMode, static_cast<bool>(true));
    store(ComfortNoiseNoiseFloorDbfs, static_cast<float>(-96.03406f));
    store(SuppressorNearendAverageBlocks, static_cast<std::size_t>(4));
    store(SuppressorNormalTuningMaskLfEnrTransparent, static_cast<float>(.3f));
    store(SuppressorNormalTuningMaskLfEnrSuppress, static_cast<float>(.4f));
    store(SuppressorNormalTuningMaskLfEmrTransparent, static_cast<float>(.3f));
    store(SuppressorNormalTuningMaskHfEnrTransparent, static_cast<float>(.07f));
    store(SuppressorNormalTuningMaskHfEnrSuppress, static_cast<float>(.1f));
    store(SuppressorNormalTuningMaskHfEmrTransparent, static_cast<float>(.3f));
    store(SuppressorNormalTuningMaxIncFactor, static_cast<float>(2.0f));
    store(SuppressorNormalTuningMaxDecFactorLf, static_cast<float>(0.25f));
    store(SuppressorNearendTuningMaskLfEnrTransparent, static_cast<float>(1.09f));
    store(SuppressorNearendTuningMaskLfEnrSuppress, static_cast<float>(1.1f));
    store(SuppressorNearendTuningMaskLfEmrTransparent, static_cast<float>(.3f));
    store(SuppressorNearendTuningMaskHfEnrTransparent, static_cast<float>(.1f));
    store(SuppressorNearendTuningMaskHfEnrSuppress, static_cast<float>(.3f));
    store(SuppressorNearendTuningMaskHfEmrTransparent, static_cast<float>(.3f));
    store(SuppressorNearendTuningMaxIncFactor, static_cast<float>(2.0f));
    store(SuppressorNearendTuningMaxDecFactorLf, static_cast<float>(0.25f));
    store(SuppressorLfSmoothingDuringInitialPhase, static_cast<bool>(true));
    store(SuppressorLastPermanentLfSmoothingBand, static_cast<int>(0));
    store(SuppressorLastLfSmoothingBand, static_cast<int>(5));
    store(SuppressorLastLfBand, static_cast<int>(5));
    store(SuppressorFirstHfBand, static_cast<int>(8));
    store(SuppressorDominantNearendDetectionEnrThreshold, static_cast<float>(.25f));
    store(SuppressorDominantNearendDetectionEnrExitThreshold, static_cast<float>(10.f));
    store(SuppressorDominantNearendDetectionSnrThreshold, static_cast<float>(30.f));
    store(SuppressorDominantNearendDetectionHoldDuration, static_cast<int>(50));
    store(SuppressorDominantNearendDetectionTriggerThreshold, static_cast<int>(12));
    store(SuppressorDominantNearendDetectionUseDuringInitialPhase, static_cast<bool>(true));
    store(SuppressorDominantNearendDetectionUseUnboundedEchoSpectrum, static_cast<bool>(true));
    store(SuppressorSubbandNearendDetectionNearendAverageBlocks, static_cast<std::size_t>(1));
    store(SuppressorSubbandNearendDetectionSubband1Low, static_cast<std::size_t>(1));
    store(SuppressorSubbandNearendDetectionSubband1High, static_cast<std::size_t>(1));
    store(SuppressorSubbandNearendDetectionSubband2Low, static_cast<std::size_t>(1));
    store(SuppressorSubbandNearendDetectionSubband2High, static_cast<std::size_t>(1));
    store(SuppressorSubbandNearendDetectionNearendThreshold, static_cast<float>(1.f));
    store(SuppressorSubbandNearendDetectionSnrThreshold, static_cast<float>(1.f));
    store(SuppressorUseSubbandNearendDetection, static_cast<bool>(false));
    store(SuppressorHighBandsSuppressionEnrThreshold, static_cast<float>(1.f));
    store(SuppressorHighBandsSuppressionMaxGainDuringEcho, static_cast<float>(1.f));
    store(SuppressorHighBandsSuppressionAntiHowlingActivationThreshold, static_cast<float>(400.f));
    store(SuppressorHighBandsSuppressionAntiHowlingGain, static_cast<float>(1.f));
    store(SuppressorFloorFirstIncrease, static_cast<float>(0.00001f));
    store(SuppressorConservativeHfSuppression, static_cast<bool>(false));
    store(MultiChannelDetectStereoContent, static_cast<bool>(true));
    store(MultiChannelStereoDetectionThreshold, static_cast<float>(0.0f));
    store(MultiChannelStereoDetectionTimeoutThresholdSeconds, static_cast<int>(300));
    store(MultiChannelStereoDetectionHysteresisSeconds, static_cast<float>(2.0f));
}

void APS5_VABI _ZN6webrtc20EchoCanceller3ConfigC1ERKS0__nid_postfix(WebRtcEchoCanceller3Config* config, const WebRtcEchoCanceller3Config* other) {
    if (!config)
        throw std::invalid_argument("EchoCanceller3Config copy constructor requires a config");
    if (!other)
        throw std::invalid_argument("EchoCanceller3Config copy constructor requires a source");
    std::memmove(config, other, sizeof(WebRtcEchoCanceller3Config));
}

WebRtcEchoCanceller3Config* APS5_VABI _ZN6webrtc20EchoCanceller3ConfigaSERKS0__nid_postfix(WebRtcEchoCanceller3Config* config, const WebRtcEchoCanceller3Config* other) {
    if (!config)
        throw std::invalid_argument("EchoCanceller3Config::operator= requires a config");
    if (!other)
        throw std::invalid_argument("EchoCanceller3Config::operator= requires a source");
    std::memmove(config, other, sizeof(WebRtcEchoCanceller3Config));
    return config;
}

}
