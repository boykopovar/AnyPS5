#include "Mixer.hpp"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void Require(bool value, const char* message) {
    if (value) return;
    std::fprintf(stderr, "%s\n", message);
    std::abort();
}

int main() {
    std::int16_t pcm[4] = {16384, -16384, 8192, -8192};
    Audio3dInternal::Source s{pcm, 2, 2, false, 1.0f, 0.0f};
    std::vector<std::int16_t> out;
    Audio3dInternal::MixFrame(&s, 1, 2, out);
    Require(out.size() == 4, "frame is granularity*2 samples");
    Require(out[0] == 16384 && out[1] == -16384, "centered stereo passes through");
    Require(out[2] == 8192 && out[3] == -8192, "centered stereo passes through the second frame");

    std::int16_t panned[2] = {10000, 20000};
    Audio3dInternal::Source hard_right{panned, 1, 2, false, 1.0f, 1.0f};
    std::vector<std::int16_t> panned_out;
    Audio3dInternal::MixFrame(&hard_right, 1, 1, panned_out);
    Require(panned_out.size() == 2 && panned_out[0] == 0 && panned_out[1] == 19999, "hard right keeps only the right channel");

    float floats[2] = {0.5f, -0.5f};
    Audio3dInternal::Source float_source{floats, 1, 2, true, 1.0f, 0.0f};
    std::vector<std::int16_t> float_out;
    Audio3dInternal::MixFrame(&float_source, 1, 1, float_out);
    Require(float_out.size() == 2 && float_out[0] == 16384 && float_out[1] == -16384, "float samples are converted");

    std::int16_t mono[1] = {10000};
    Audio3dInternal::Source mono_source{mono, 1, 1, false, 1.0f, 0.0f};
    std::vector<std::int16_t> mono_out;
    Audio3dInternal::MixFrame(&mono_source, 1, 1, mono_out);
    Require(mono_out.size() == 2 && mono_out[0] == 10000 && mono_out[1] == 10000, "centered mono is duplicated");

    std::vector<std::int16_t> short_out;
    Audio3dInternal::MixFrame(&mono_source, 1, 4, short_out);
    Require(short_out.size() == 8, "short frame keeps the full frame size");
    Require(short_out[0] == 10000 && short_out[1] == 10000, "short frame mixes the first sample");
    Require(short_out[2] == 0 && short_out[3] == 0 && short_out[4] == 0 && short_out[5] == 0 && short_out[6] == 0 && short_out[7] == 0, "short frame zero pads the tail");

    std::int16_t loud[1] = {32767};
    Audio3dInternal::Source loud_source{loud, 1, 1, false, 1.0f, 0.0f};
    Audio3dInternal::Source loud_sources[2] = {loud_source, loud_source};
    std::vector<std::int16_t> loud_out;
    Audio3dInternal::MixFrame(loud_sources, 2, 1, loud_out);
    Require(loud_out.size() == 2 && loud_out[0] == 32767 && loud_out[1] == 32767, "sums above full scale clamp");

    Audio3dInternal::Source out_of_range{panned, 1, 2, false, 1.0f, 2.0f};
    std::vector<std::int16_t> out_of_range_out;
    Audio3dInternal::MixFrame(&out_of_range, 1, 1, out_of_range_out);
    Require(out_of_range_out.size() == 2 && out_of_range_out[0] == 0 && out_of_range_out[1] == 19999, "out of range pan clamps");

    std::vector<std::int16_t> empty_out;
    Audio3dInternal::MixFrame(nullptr, 0, 2, empty_out);
    Require(empty_out.size() == 4 && empty_out[0] == 0 && empty_out[1] == 0 && empty_out[2] == 0 && empty_out[3] == 0, "no sources produce silence");

    std::int16_t skipped[2] = {10000, 10000};
    Audio3dInternal::Source null_samples{nullptr, 1, 2, false, 1.0f, 0.0f};
    Audio3dInternal::Source empty_samples{skipped, 0, 2, false, 1.0f, 0.0f};
    Audio3dInternal::Source skipped_sources[2] = {null_samples, empty_samples};
    std::vector<std::int16_t> skipped_out;
    Audio3dInternal::MixFrame(skipped_sources, 2, 1, skipped_out);
    Require(skipped_out.size() == 2 && skipped_out[0] == 0 && skipped_out[1] == 0, "null and empty sources are skipped");

    std::int16_t surround6[6] = {1000, 2000, 3000, 0, 4000, 5000};
    Audio3dInternal::Source surround6_source{surround6, 1, 6, false, 1.0f, 0.0f};
    std::vector<std::int16_t> surround6_out;
    Audio3dInternal::MixFrame(&surround6_source, 1, 1, surround6_out);
    Require(surround6_out.size() == 2 && surround6_out[0] == 5950 && surround6_out[1] == 7657,
            "6 channel downmix folds center and surrounds, dropping LFE");

    std::int16_t surround8[8] = {1000, 2000, 3000, 0, 4000, 5000, 6000, 7000};
    Audio3dInternal::Source surround8_source{surround8, 1, 8, false, 1.0f, 0.0f};
    std::vector<std::int16_t> surround8_out;
    Audio3dInternal::MixFrame(&surround8_source, 1, 1, surround8_out);
    Require(surround8_out.size() == 2 && surround8_out[0] == 10192 && surround8_out[1] == 12606,
            "8 channel downmix folds rear surrounds into stereo");

    std::int16_t surround5[5] = {1000, 2000, 3000, 0, 4000};
    Audio3dInternal::Source surround5_source{surround5, 1, 5, false, 1.0f, 0.0f};
    std::vector<std::int16_t> surround5_out;
    Audio3dInternal::MixFrame(&surround5_source, 1, 1, surround5_out);
    Require(surround5_out.size() == 2 && surround5_out[0] == 5950 && surround5_out[1] == 4121,
            "5 channel downmix reads only the channels present");

    std::int16_t surround3[3] = {1000, 2000, 3000};
    Audio3dInternal::Source surround3_source{surround3, 1, 3, false, 1.0f, 0.0f};
    std::vector<std::int16_t> surround3_out;
    Audio3dInternal::MixFrame(&surround3_source, 1, 1, surround3_out);
    Require(surround3_out.size() == 2 && surround3_out[0] == 3121 && surround3_out[1] == 4121,
            "3 channel downmix folds center into both sides");

    std::int16_t surround4[4] = {1000, 2000, 3000, 0};
    Audio3dInternal::Source surround4_source{surround4, 1, 4, false, 1.0f, 0.0f};
    std::vector<std::int16_t> surround4_out;
    Audio3dInternal::MixFrame(&surround4_source, 1, 1, surround4_out);
    Require(surround4_out.size() == 2 && surround4_out[0] == 3121 && surround4_out[1] == 4121,
            "4 channel downmix drops LFE");

    std::int16_t surround6_panned[6] = {1000, 2000, 3000, 0, 4000, 5000};
    Audio3dInternal::Source surround6_panned_source{surround6_panned, 1, 6, false, 1.0f, 1.0f};
    std::vector<std::int16_t> surround6_panned_out;
    Audio3dInternal::MixFrame(&surround6_panned_source, 1, 1, surround6_panned_out);
    Require(surround6_panned_out.size() == 2 && surround6_panned_out[0] == 0 && surround6_panned_out[1] == 7657,
            "6 channel downmix is panned after folding");

    return 0;
}
