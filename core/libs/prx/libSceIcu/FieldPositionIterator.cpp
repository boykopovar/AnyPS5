// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*****************************************************************************************
* Copyright (C) 2015, International Business Machines
* Corporation and others. All Rights Reserved.
*****************************************************************************************
*/
// https://github.com/unicode-org/icu/blob/049e0d6a420629ac7db77256987d083a563287b5/icu4c/source/i18n/ufieldpositer.cpp

#include "prx/libSceIcu/FieldPositionIterator.hpp"
#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/GuestArena.hpp"
#include "unicode/fpositer.h"
#include "unicode/ufieldpositer.h"
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <unordered_set>

namespace {

std::mutex iteratorMutex;
std::unordered_set<UFieldPositionIterator*> iterators;

void WriteStatus(std::int32_t* destination, std::int32_t previous, std::int32_t value, const char* function) {
    if (value != previous && !GuestCopyOut_nid_no_patch(destination, &value, sizeof(value))) {
        NotImplemented_nid_no_patch(function);
    }
}

}

std::unique_lock<std::mutex> Icu::LockFieldPositionIterator_nid_no_patch(UFieldPositionIterator* iterator, const char* caller) {
    std::unique_lock lock(iteratorMutex);
    if (!iterators.contains(iterator)) NotImplemented_nid_no_patch(caller);
    return lock;
}

extern "C" {

void APS5_VABI ufieldpositer_close_nid_postfix(UFieldPositionIterator* fpositer) {
    if (!fpositer) return;
    const std::lock_guard lock(iteratorMutex);
    if (!iterators.erase(fpositer)) NotImplemented_nid_no_patch(__func__);
    ufieldpositer_close(fpositer);
}

UFieldPositionIterator* APS5_VABI ufieldpositer_open_nid_postfix(std::int32_t* status) {
    std::int32_t previous = 0;
    if (!GuestCopyIn_nid_no_patch(&previous, status, sizeof(previous))) NotImplemented_nid_no_patch(__func__);
    if (previous > 0) return nullptr;
    auto error = static_cast<UErrorCode>(previous);
    std::unique_ptr<UFieldPositionIterator, decltype(&ufieldpositer_close)> iterator(ufieldpositer_open(&error),
                                                                                 &ufieldpositer_close);
    WriteStatus(status, previous, error, __func__);
    if (!iterator) return nullptr;
    try {
        const std::lock_guard lock(iteratorMutex);
        iterators.insert(iterator.get());
    } catch (const std::bad_alloc&) {
        WriteStatus(status, previous, U_MEMORY_ALLOCATION_ERROR, __func__);
        return nullptr;
    }
    return iterator.release();
}

std::int32_t APS5_VABI ufieldpositer_next_nid_postfix(UFieldPositionIterator* fpositer, std::int32_t* beginIndex,
                                                    std::int32_t* endIndex) {
    const auto lock = Icu::LockFieldPositionIterator_nid_no_patch(fpositer, __func__);
    icu::FieldPosition field;
    if (!reinterpret_cast<icu::FieldPositionIterator*>(fpositer)->next(field)) return -1;
    const GuestArena::HostWrite beginAccess(beginIndex, beginIndex ? sizeof(*beginIndex) : 0);
    const GuestArena::HostWrite endAccess(endIndex, endIndex ? sizeof(*endIndex) : 0);
    if (!beginAccess.Open() || !endAccess.Open() ||
        (beginIndex && !GuestWritable_nid_no_patch(beginIndex, sizeof(*beginIndex))) ||
        (endIndex && !GuestWritable_nid_no_patch(endIndex, sizeof(*endIndex)))) {
        NotImplemented_nid_no_patch(__func__);
    }
    const std::int32_t begin = field.getBeginIndex();
    const std::int32_t end = field.getEndIndex();
    if ((beginIndex && !GuestCopyOut_nid_no_patch(beginIndex, &begin, sizeof(begin))) ||
        (endIndex && !GuestCopyOut_nid_no_patch(endIndex, &end, sizeof(end)))) {
        NotImplemented_nid_no_patch(__func__);
    }
    return field.getField();
}

}
