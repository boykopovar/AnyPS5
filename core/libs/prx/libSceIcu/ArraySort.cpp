// Adapted from uprv_sortArray in ICU 68.2 common/uarrsort.cpp:
// https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/source/common/uarrsort.cpp
//
// ICU's notice, kept as the upstream file has it:
//   Copyright (C) 2003-2013, International Business Machines
//   Corporation and others.  All Rights Reserved.
// ICU and its data are distributed under the Unicode licence:
//   https://github.com/unicode-org/icu/blob/84e1f26ea77152936e70d53178a816dbfbf69989/icu4c/LICENSE
//
// Error codes are ICU's UErrorCode values: U_ZERO_ERROR is 0 and
// U_ILLEGAL_ARGUMENT_ERROR is 1. A status that already holds a failure is
// left unchanged, as in ICU.

#include "prx/libSceIcu/include/LegacyExports.hpp"

#include <cstring>
#include <vector>

namespace {

constexpr std::int32_t ZeroError = 0;
constexpr std::int32_t IllegalArgumentError = 1;
// ICU's MIN_QSORT: below this many elements the sort is an insertion sort.
constexpr std::int32_t MinQsort = 9;

void InsertionSort(char* array, std::int32_t length, std::int32_t itemSize, std::int32_t (APS5_VABI* cmp)(const void*, const void*, const void*), const void* context) {
    std::vector<char> pivot(static_cast<std::size_t>(itemSize));
    for (std::int32_t j = 1; j < length; ++j) {
        char* item = array + j * itemSize;
        std::int32_t insertionPoint = uprv_stableBinarySearch_nid_postfix(array, j, item, itemSize, cmp, context);
        if (insertionPoint < 0) {
            insertionPoint = ~insertionPoint;
        } else {
            ++insertionPoint;
        }
        if (insertionPoint < j) {
            char* destination = array + insertionPoint * itemSize;
            std::memcpy(pivot.data(), item, static_cast<std::size_t>(itemSize));
            std::memmove(destination + itemSize, destination, static_cast<std::size_t>(j - insertionPoint) * static_cast<std::size_t>(itemSize));
            std::memcpy(destination, pivot.data(), static_cast<std::size_t>(itemSize));
        }
    }
}


// ICU's semi-recursive quicksort: the smaller partition is sorted recursively
// and the larger one is looped over. The middle item is copied to px and the
// swap buffer is pw.
void SubQuickSort(char* array, std::int32_t start, std::int32_t limit, std::int32_t itemSize, std::int32_t (APS5_VABI* cmp)(const void*, const void*, const void*), const void* context, char* px, char* pw) {
    std::int32_t left, right;
    do {
        if ((start + MinQsort) >= limit) {
            InsertionSort(array + start * itemSize, limit - start, itemSize, cmp, context);
            break;
        }
        left = start;
        right = limit;
        std::memcpy(px, array + ((start + limit) / 2) * itemSize, static_cast<std::size_t>(itemSize));
        do {
            while (cmp(context, array + left * itemSize, px) < 0) ++left;
            while (cmp(context, px, array + (right - 1) * itemSize) < 0) --right;
            if (left < right) {
                --right;
                if (left < right) {
                    std::memcpy(pw, array + left * itemSize, static_cast<std::size_t>(itemSize));
                    std::memcpy(array + left * itemSize, array + right * itemSize, static_cast<std::size_t>(itemSize));
                    std::memcpy(array + right * itemSize, pw, static_cast<std::size_t>(itemSize));
                }
                ++left;
            }
        } while (left < right);

        if ((right - start) < (limit - left)) {
            if (start < right - 1) SubQuickSort(array, start, right, itemSize, cmp, context, px, pw);
            start = left;
        } else {
            if (left < limit - 1) SubQuickSort(array, left, limit, itemSize, cmp, context, px, pw);
            limit = right;
        }
    } while (start < limit - 1);
}

}

extern "C" {

void APS5_VABI uprv_sortArray_nid_postfix(void* array, std::int32_t length, std::int32_t itemSize, std::int32_t (APS5_VABI* cmp)(const void*, const void*, const void*), const void* context, std::int8_t sortStable, std::int32_t* pErrorCode) {
    if (pErrorCode == nullptr || *pErrorCode > ZeroError) return;
    if ((length > 0 && array == nullptr) || length < 0 || itemSize <= 0 || cmp == nullptr) {
        *pErrorCode = IllegalArgumentError;
        return;
    }
    if (length <= 1) return;
    if (length < MinQsort || sortStable != 0) {
        InsertionSort(static_cast<char*>(array), length, itemSize, cmp, context);
        return;
    }
    std::vector<char> scratch(2 * static_cast<std::size_t>(itemSize));
    SubQuickSort(static_cast<char*>(array), 0, length, itemSize, cmp, context, scratch.data(), scratch.data() + itemSize);
}

}
