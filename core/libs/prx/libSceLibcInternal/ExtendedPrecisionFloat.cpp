/* Derived from Microsoft STL stl/src/xxxprec.h (219514876ea86491de191ceaa88632616bbc0f19) */
/* Copyright (c) Microsoft Corporation.
 * SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception */
#include "prx/libSceLibcInternal/include/MathInternals.hpp"
#include "prx/libc/include/Api.hpp"
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <string>

namespace {

constexpr short InfiniteCode = 1;
constexpr short NanCode = 2;
constexpr int InvalidException = 0x01;
constexpr int DelayWords = 4;

struct Format {
    static constexpr int MantissaBits = FLT_MANT_DIG;
    static constexpr std::int64_t LargeExponent = 2 * FLT_MAX_EXP;
    static short Test(const float* value) { return _FDtest_nid_postfix(value); }
    static short Unscale(short* exponent, float* value) { return _FDunscale_nid_postfix(exponent, value); }
    static short Truncate(float* value, short bits) { return _FDint_nid_postfix(value, bits); }
    static short Scale(float* value, std::int64_t exponent) { return _FDscale_nid_postfix(value, exponent); }
    static float LoadExponent(float value, int exponent) { return ldexpf_nid_postfix(value, exponent); }
};

constexpr int HalfBits = Format::MantissaBits / 2;

void CopyUp(float* parts, int from, int count) {
    int index = from;
    while (++index < count && (parts[index - 1] = parts[index]) != 0.0f) {
    }
    parts[count - 1] = 0.0f;
}

float* SetWhole(float* parts, int count, const float value) {
    float head = value;
    short code = 0;
    short exponent = 0;
    if (count <= 0) return parts;
    if (count == 1 || (code = Format::Unscale(&exponent, &head)) == 0) {
        parts[0] = head;
    } else if (code > 0) {
        parts[0] = head;
        parts[1] = 0.0f;
    } else {
        Format::Truncate(&head, static_cast<short>(HalfBits));
        Format::Scale(&head, exponent);
        parts[0] = head;
        parts[1] = value - head;
        if (count > 2) parts[2] = 0.0f;
    }
    return parts;
}

float* AddHalf(float* parts, int count, float addend) {
    float scaled = addend;
    short code = 0;
    short addendExponent = 0;
    if (count == 0) return parts;
    if ((code = Format::Unscale(&addendExponent, &scaled)) > 0) {
        if (code == NanCode || (code = Format::Test(&parts[0])) <= 0) {
            parts[0] = addend;
        } else if (code != NanCode && std::signbit(addend) != std::signbit(parts[0])) {
            _Feraise_nid_postfix(InvalidException);
            parts[0] = std::numeric_limits<float>::quiet_NaN();
            if (count > 1) parts[1] = 0.0f;
        }
        return parts;
    }
    if (code == 0) return parts;
    std::int64_t previousExponent = Format::LargeExponent;
    const int wordBits = HalfBits;
    int index = 0;
    while (index < count) {
        float partScaled = parts[index];
        short partExponent = 0;
        if ((code = Format::Unscale(&partExponent, &partScaled)) > 0) break;
        if (code == 0) {
            parts[index] = addend;
            if (index + 1 < count) parts[index + 1] = 0.0f;
            break;
        }
        const std::int64_t difference = static_cast<std::int64_t>(partExponent) - addendExponent;
        if (difference <= -wordBits && addend != 0.0f) {
            int last = index;
            do {
                ++last;
            } while (last < count && parts[last] != 0.0f);
            if (last < count - 1) ++last;
            else if (last == count) --last;
            for (; index < last; --last) parts[last] = parts[last - 1];
            parts[index] = addend;
            addend = 0.0f;
        } else if (wordBits <= difference && addend != 0.0f) {
            previousExponent = partExponent;
            ++index;
        } else {
            if ((parts[index] += addend) == 0.0f) {
                CopyUp(parts, index, count);
                if (parts[index] == 0.0f) break;
            }
            addend = parts[index];
            Format::Unscale(&addendExponent, &addend);
            if (previousExponent - wordBits < addendExponent) {
                Format::Truncate(&addend, static_cast<short>(addendExponent - (previousExponent - wordBits)));
                Format::Scale(&addend, addendExponent);
                if ((parts[index] -= addend) == 0.0f) CopyUp(parts, index, count);
                if (--index == 0) {
                    previousExponent = Format::LargeExponent;
                } else {
                    scaled = parts[index - 1];
                    Format::Unscale(&partExponent, &scaled);
                    previousExponent = partExponent;
                }
            } else if (index + 1 == count) {
                break;
            } else {
                addend = parts[index];
                Format::Unscale(&partExponent, &parts[index]);
                Format::Truncate(&parts[index], static_cast<short>(wordBits));
                Format::Scale(&parts[index], partExponent);
                addend -= parts[index];
                previousExponent = partExponent;
                scaled = addend != 0.0f ? addend : parts[index];
                Format::Unscale(&addendExponent, &scaled);
                ++index;
            }
        }
    }
    return parts;
}

float* MultiplyHalf(float* parts, int count, float factor) {
    float delay[DelayWords];
    if (count > 0) {
        delay[0] = parts[0] * factor;
        const short code = Format::Test(&delay[0]);
        if (code >= 0) {
            if (code == NanCode) _Feraise_nid_postfix(InvalidException);
            parts[0] = delay[0];
            if (code > 0 && count > 1) parts[1] = 0.0f;
            return parts;
        }
        parts[0] = 0.0f;
    }
    for (int filled = 1, index = 0; index < count; ++index, --filled) {
        for (; filled < DelayWords; ++filled) {
            if (index + filled < count && parts[index + filled] != 0.0f) {
                delay[filled] = parts[index + filled] * factor;
                parts[index + filled] = 0.0f;
            } else {
                delay[filled] = 0.0f;
                filled = 2 * DelayWords;
                break;
            }
        }
        if (delay[0] == 0.0f) break;
        int position = 0;
        float head = delay[0];
        short exponent = 0;
        Format::Unscale(&exponent, &head);
        Format::Truncate(&head, static_cast<short>(HalfBits));
        Format::Scale(&head, exponent);
        AddHalf(parts, count, head);
        AddHalf(parts, count, delay[0] - head);
        while (++position < filled) {
            if ((delay[position - 1] = delay[position]) == 0.0f) break;
        }
    }
    return parts;
}

float* SetInteger(float* parts, int count, std::int64_t value) {
    SetWhole(parts, count, static_cast<float>(value / 10000));
    MultiplyHalf(parts, count, 10000.0f);
    AddHalf(parts, count, static_cast<float>(value % 10000));
    return parts;
}

float* Move(float* parts, int count, const float* source) {
    std::memmove(parts, source, static_cast<std::size_t>(count) * sizeof(float));
    return parts;
}

float* AddExtended(float* parts, int count, const float* addend, int addendCount, bool negate) {
    for (int index = 0; index < addendCount && addend[index] != 0.0f; ++index) {
        AddHalf(parts, count, negate ? -addend[index] : addend[index]);
    }
    return parts;
}

float* LoadExponent(float* parts, int count, int exponent) {
    for (int index = 0; index < count; ++index) {
        parts[index] = Format::LoadExponent(parts[index], exponent);
        if (parts[index] == 0.0f) break;
    }
    return parts;
}

float* MultiplyExtended(float* parts, int count, const float* factor, int factorCount, float* scratch) {
    if (count == 0 || factorCount == 0) return parts;
    if (factor[0] == 0.0f || factor[1] == 0.0f) {
        MultiplyHalf(parts, count, factor[0]);
        return parts;
    }
    float* original = scratch;
    float* product = scratch + count;
    Move(original, count, parts);
    MultiplyHalf(parts, count, factor[0]);
    for (int index = 1; index < factorCount && factor[index] != 0.0f; ++index) {
        Move(product, count, original);
        MultiplyHalf(product, count, factor[index]);
        AddExtended(parts, count, product, count, false);
    }
    return parts;
}

float* Invert(float* parts, int count, float* scratch) {
    if (count == 0) return parts;
    const short code = Format::Test(&parts[0]);
    if (code >= 0) {
        if (code == InfiniteCode) parts[0] = 0.0f;
        else if (code == 0) parts[0] = std::numeric_limits<float>::infinity();
        return parts;
    }
    float* accumulator = scratch;
    float* negated = scratch + count;
    float* productScratch = negated + count;
    float estimate = parts[0];
    Move(negated, count, parts);
    MultiplyHalf(negated, count, -1.0f);
    if (count > 1) estimate += parts[1];
    SetWhole(parts, count, 1.0f / estimate);
    for (int precision = 1; precision < count; precision <<= 1) {
        Move(accumulator, count, parts);
        MultiplyExtended(accumulator, count, negated, count, productScratch);
        AddHalf(accumulator, count, 1.0f);
        MultiplyExtended(accumulator, count, parts, count, productScratch);
        AddExtended(parts, count, accumulator, count, false);
    }
    return parts;
}

void RequireCount(int count, const char* function) {
    if (count < 0) throw std::invalid_argument(std::string(function) + ": negative count");
}

void RequireWritable(const void* parts, int count, const char* function) {
    if (count > 0 && parts == nullptr) throw std::invalid_argument(std::string(function) + ": null parts");
}

void RequireParts(const void* parts, int count, const char* function) {
    RequireCount(count, function);
    RequireWritable(parts, count, function);
}

void RequireFactor(int count, const float* factor, int factorCount, const float* scratch, const char* function) {
    RequireCount(factorCount, function);
    if (count == 0 || factorCount == 0) return;
    if (factor == nullptr) throw std::invalid_argument(std::string(function) + ": null factor");
    if (factorCount == 1 && factor[0] != 0.0f) throw std::invalid_argument(std::string(function) + ": one nonzero factor part");
    if (factor[0] != 0.0f && factor[1] != 0.0f && scratch == nullptr) {
        throw std::invalid_argument(std::string(function) + ": null scratch");
    }
}

void RequireAddend(const float* addend, int addendCount, const char* function) {
    RequireCount(addendCount, function);
    if (addendCount != 0 && addend == nullptr) throw std::invalid_argument(std::string(function) + ": null addend");
}

void RequireScratch(const float* parts, int count, const float* scratch, const char* function) {
    if (count == 0 || scratch != nullptr) return;
    if (Format::Test(&parts[0]) < 0) throw std::invalid_argument(std::string(function) + ": null scratch");
}

}

extern "C" {

float* APS5_VABI _FXp_setw_nid_postfix(float* parts, int count, float value) {
    RequireWritable(parts, count, __func__);
    return SetWhole(parts, count, value);
}

float* APS5_VABI _FXp_addh_nid_postfix(float* parts, int count, float addend) {
    RequireParts(parts, count, __func__);
    return AddHalf(parts, count, addend);
}

float* APS5_VABI _FXp_mulh_nid_postfix(float* parts, int count, float factor) {
    RequireWritable(parts, count, __func__);
    return MultiplyHalf(parts, count, factor);
}

float* APS5_VABI _FXp_setn_nid_postfix(float* parts, int count, std::int64_t value) {
    RequireWritable(parts, count, __func__);
    return SetInteger(parts, count, value);
}

float* APS5_VABI _FXp_movx_nid_postfix(float* parts, int count, const float* source) {
    RequireParts(parts, count, __func__);
    if (count != 0 && source == nullptr) throw std::invalid_argument(std::string(__func__) + ": null source");
    return Move(parts, count, source);
}

float* APS5_VABI _FXp_addx_nid_postfix(float* parts, int count, const float* addend, int addendCount) {
    RequireParts(parts, count, __func__);
    RequireAddend(addend, addendCount, __func__);
    return AddExtended(parts, count, addend, addendCount, false);
}

float* APS5_VABI _FXp_subx_nid_postfix(float* parts, int count, const float* subtrahend, int subtrahendCount) {
    RequireParts(parts, count, __func__);
    RequireAddend(subtrahend, subtrahendCount, __func__);
    return AddExtended(parts, count, subtrahend, subtrahendCount, true);
}

float* APS5_VABI _FXp_ldexpx_nid_postfix(float* parts, int count, int exponent) {
    RequireWritable(parts, count, __func__);
    return LoadExponent(parts, count, exponent);
}

float* APS5_VABI _FXp_mulx_nid_postfix(float* parts, int count, const float* factor, int factorCount, float* scratch) {
    RequireParts(parts, count, __func__);
    RequireFactor(count, factor, factorCount, scratch, __func__);
    return MultiplyExtended(parts, count, factor, factorCount, scratch);
}

float* APS5_VABI _FXp_invx_nid_postfix(float* parts, int count, float* scratch) {
    RequireParts(parts, count, __func__);
    RequireScratch(parts, count, scratch, __func__);
    return Invert(parts, count, scratch);
}

}
