#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <type_traits>

#include "Ngs2Internal.hpp"
#include "prx/libc/include/General.hpp"

namespace {

constexpr float InverseRootEight = 0.3535533905932738f;
constexpr std::array<double, 8> DelaySeconds{0.0127, 0.0179, 0.0211, 0.0253, 0.0299, 0.0337, 0.0391, 0.0439};

double Gain(std::int32_t millibels) {
    return std::pow(10.0, millibels / 2000.0);
}

bool IsLfe(std::uint32_t channel, std::uint32_t count) {
    return channel == 3 && (count == 6 || count == 8);
}

float Sign(std::uint32_t row, std::uint32_t column) {
    return (std::popcount(row & column) & 1) == 0 ? 1.0f : -1.0f;
}

void Diffuse(std::array<float, 8>& values, float sine, float cosine) {
    for (std::uint32_t step = 1; step < values.size(); step *= 2) {
        for (std::uint32_t i = 0; i < values.size(); ++i) {
            if ((i & step) != 0) continue;
            const auto j = i | step;
            const auto left = values[i];
            const auto right = values[j];
            values[i] = cosine * left - sine * right;
            values[j] = sine * left + cosine * right;
        }
    }
}

float DrySample(const std::array<float, 2>& input, std::uint32_t inputs, std::uint32_t channel, std::uint32_t outputs) {
    if (outputs == 1) return inputs == 1 ? input[0] : (input[0] + input[1]) * 0.5f;
    if (channel >= 2) return 0.0f;
    return input[inputs == 1 ? 0 : channel];
}

}

void Ngs2ReverbShelf::Configure(double lowGain, double highGain, double frequency, double sampleRate) {
    const double k = std::tan(std::numbers::pi * frequency / sampleRate);
    b0 = (lowGain * k + highGain) / (1.0 + k);
    b1 = (lowGain * k - highGain) / (1.0 + k);
    a1 = (k - 1.0) / (1.0 + k);
}

float Ngs2ReverbShelf::Process(float input) {
    double output = b0 * input + b1 * x1 - a1 * y1;
    if (std::abs(output) < 1e-20) output = 0.0;
    x1 = input;
    y1 = output;
    return static_cast<float>(output);
}

void Ngs2ReverbDelay::Resize(std::size_t size) {
    if (samples.size() == size) return;
    samples.assign(size, 0.0f);
    cursor = 0;
}

float Ngs2ReverbDelay::Read(std::size_t delay) const {
    return samples[(cursor + samples.size() - 1 - delay) % samples.size()];
}

void Ngs2ReverbDelay::Write(float input) {
    samples[cursor] = input;
    cursor = (cursor + 1) % samples.size();
}

void Ngs2ReverbDelay::Clear() {
    std::fill(samples.begin(), samples.end(), 0.0f);
    cursor = 0;
}

void Ngs2Reverb::Clear() {
    for (auto& delay : input) delay.Clear();
    for (auto& delay : lines) delay.Clear();
    for (auto& shelf : inputShelf) shelf.x1 = shelf.y1 = 0.0;
    for (auto& shelf : feedback) shelf.x1 = shelf.y1 = 0.0;
    remainingSamples = 0;
}

void Ngs2SetReverb(Ngs2Voice& voice, const Ngs2ReverbI3dl2Param& requested) {
    auto param = requested;
    if (voice.inputChannels == 0 || voice.channels == 0) throw std::invalid_argument("NGS2: reverb voice must be set up before I3DL2 control");
    if (voice.inputChannels > 2 || (voice.channels != 1 && voice.channels != 2 && voice.channels != 4 && voice.channels != 6 && voice.channels != 8))
        throw std::runtime_error("NGS2: reverb channel layout is not implemented");
    if (param.reflection_pattern != 0) throw std::runtime_error("NGS2: reverb reflection pattern " + std::to_string(param.reflection_pattern) + " is not implemented");
    const auto check = [](const char* name, auto& value, auto minimum, auto maximum) {
        if constexpr (std::is_floating_point_v<decltype(minimum)>) {
            const auto infinity = std::numeric_limits<decltype(minimum)>::infinity();
            if (value == std::nextafter(minimum, -infinity)) value = minimum;
            if (value == std::nextafter(maximum, infinity)) value = maximum;
        }
        if (std::isfinite(value) && value >= minimum && value <= maximum) return;
        std::ostringstream message;
        message.precision(9);
        message << "NGS2: reverb " << name << '=' << value << " is outside [" << minimum << ", " << maximum << ']';
        throw std::invalid_argument(message.str());
    };
    check("wet", param.wet, 0.0f, 1.0f);
    check("dry", param.dry, 0.0f, 1.0f);
    check("room", param.room, -10000, 0);
    check("room_hf", param.room_hf, -10000, 0);
    check("decay_time", param.decay_time, 0.1f, 20.0f);
    check("decay_hf_ratio", param.decay_hf_ratio, 0.1f, 2.0f);
    check("reflections", param.reflections, -10000, 1000);
    check("reverb", param.reverb, -10000, 2000);
    check("reflections_delay", param.reflections_delay, 0.0f, 0.3f);
    check("reverb_delay", param.reverb_delay, 0.0f, 0.1f);
    check("diffusion", param.diffusion, 0.0f, 100.0f);
    check("density", param.density, 0.0f, 100.0f);
    check("hf_reference", param.hf_reference, 20.0f, 20000.0f);
    const auto rate = voice.rack->system->option.sample_rate;
    if (param.hf_reference >= rate * 0.5 || rate > 192000)
        throw std::runtime_error("NGS2: reverb sample rate or HF reference is not supported");

    auto next = voice.reverb ? std::make_unique<Ngs2Reverb>(*voice.reverb) : std::make_unique<Ngs2Reverb>();
    next->param = param;
    next->earlyDelay = static_cast<std::size_t>(std::llround(param.reflections_delay * static_cast<double>(rate)));
    next->lateDelay = next->earlyDelay + static_cast<std::size_t>(std::llround(param.reverb_delay * static_cast<double>(rate)));
    next->earlyGain = static_cast<float>(Gain(param.room + param.reflections));
    next->lateGain = static_cast<float>(Gain(param.room + param.reverb));
    const double angle = param.diffusion * std::numbers::pi / 400.0;
    next->sine = static_cast<float>(std::sin(angle));
    next->cosine = static_cast<float>(std::cos(angle));
    next->tailSamples = static_cast<std::uint64_t>(std::ceil((3.0 * param.decay_time * std::max(1.0f, param.decay_hf_ratio) + 0.5) * rate));
    if (next->remainingSamples != 0) next->remainingSamples = next->tailSamples;
    for (std::size_t i = 0; i < next->input.size(); ++i) {
        next->input[i].Resize(static_cast<std::size_t>(std::ceil(0.4 * rate)) + 2);
        next->inputShelf[i].Configure(1.0, Gain(param.room_hf), param.hf_reference, rate);
    }
    for (std::size_t i = 0; i < next->lines.size(); ++i) {
        next->lines[i].Resize(static_cast<std::size_t>(std::ceil(DelaySeconds[i] * rate)) + 1);
        next->delays[i] = std::max<std::size_t>(1, static_cast<std::size_t>(std::llround(DelaySeconds[i] * rate * (0.25 + 0.0075 * param.density))));
        const double seconds = next->delays[i] / static_cast<double>(rate);
        next->feedback[i].Configure(std::pow(0.001, seconds / param.decay_time),
                                    std::pow(0.001, seconds / (param.decay_time * param.decay_hf_ratio)), param.hf_reference, rate);
    }
    voice.reverb = std::move(next);
}

void Ngs2ProcessReverb(Ngs2Voice& voice, std::uint32_t grain) {
    if (!voice.reverb) throw std::runtime_error("NGS2: rendering reverb without explicit I3DL2 parameters is not implemented");
    auto& reverb = *voice.reverb;
    bool audible = false;
    for (std::uint32_t sample = 0; sample < grain; ++sample) {
        std::array<float, 2> input{};
        for (std::uint32_t channel = 0; channel < voice.inputChannels; ++channel) {
            input[channel] = voice.samples[channel * grain + sample];
            if (input[channel] != 0.0f) reverb.remainingSamples = reverb.tailSamples;
        }
        std::array<float, 2> early{};
        std::array<float, 2> late{};
        std::array<float, 8> field{};
        if (reverb.remainingSamples != 0) {
            for (std::uint32_t channel = 0; channel < voice.inputChannels; ++channel) {
                reverb.input[channel].Write(reverb.inputShelf[channel].Process(input[channel]));
                early[channel] = reverb.input[channel].Read(reverb.earlyDelay);
                late[channel] = reverb.input[channel].Read(reverb.lateDelay);
            }
            for (std::uint32_t i = 0; i < field.size(); ++i)
                field[i] = reverb.feedback[i].Process(reverb.lines[i].Read(reverb.delays[i] - 1));
            Diffuse(field, reverb.sine, reverb.cosine);
            for (std::uint32_t i = 0; i < field.size(); ++i) {
                field[i] += (late[0] + Sign(1, i) * late[1]) * InverseRootEight;
                reverb.lines[i].Write(field[i]);
            }
            if (--reverb.remainingSamples == 0) reverb.Clear();
        }
        for (std::uint32_t channel = 0; channel < voice.channels; ++channel) {
            float wet = 0.0f;
            if (!IsLfe(channel, voice.channels)) {
                for (std::uint32_t i = 0; i < field.size(); ++i) {
                    const float projection = voice.channels == 1 && voice.inputChannels == 2 ? (1.0f + Sign(1, i)) * 0.5f : Sign(channel, i);
                    wet += projection * field[i];
                }
                wet *= InverseRootEight * reverb.lateGain;
                const float reflection = voice.inputChannels == 1 ? early[0]
                    : (voice.channels == 1 || channel == 2) ? (early[0] + early[1]) * 0.5f : early[channel % 2];
                wet += reflection * reverb.earlyGain;
            }
            const float output = reverb.param.dry * DrySample(input, voice.inputChannels, channel, voice.channels) + reverb.param.wet * wet;
            voice.samples[channel * grain + sample] = output;
            audible |= output != 0.0f;
        }
    }
    voice.hasSamples = audible;
}
