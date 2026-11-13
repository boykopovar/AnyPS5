// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
*******************************************************************************
*
*   Copyright (C) 2005-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
*******************************************************************************
*   file name:  utext.cpp
*   encoding:   UTF-8
*   tab size:   8 (not used)
*   indentation:4
*
*   created on: 2005apr12
*   created by: Markus W. Scherer
*/

#include "prx/libSceIcu/RecoveredSignatures.hpp"
#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include <cstddef>
#include <cstdint>
#include <cstring>

struct ExtendedUText {
    UText               ut;
    std::max_align_t    extension;
};
static const UText emptyText = {UTEXT_MAGIC, 0, 0, sizeof(UText)};
extern "C" UText* APS5_VABI
utext_setup(UText *ut, std::int32_t extraSpace, std::int32_t *status) {
    if (!status) NotImplemented_nid_no_patch(__func__);
    if (*status > 0) {
        return ut;
    }
    if (ut == nullptr) {
        std::int32_t spaceRequired = sizeof(UText);
        if (extraSpace > 0) {
            if (extraSpace > INT32_MAX - (sizeof(ExtendedUText) - sizeof(std::max_align_t))) NotImplemented_nid_no_patch(__func__);
            spaceRequired = sizeof(ExtendedUText) + extraSpace - sizeof(std::max_align_t);
        }
        ut = (UText *)malloc_nid_postfix(spaceRequired);
        if (ut == nullptr) {
            *status = 7;
            return nullptr;
        } else {
            *ut = emptyText;
            ut->flags |= UTEXT_HEAP_ALLOCATED;
            if (spaceRequired > 0) {
                ut->extraSize = extraSpace;
                ut->pExtra    = &((ExtendedUText *)ut)->extension;
            }
        }
    } else {
        if (ut->magic != UTEXT_MAGIC) {
            *status = 1;
            return ut;
        }
        if ((ut->flags & UTEXT_OPEN) && !ut->pFuncs) NotImplemented_nid_no_patch(__func__);
        if ((ut->flags & UTEXT_OPEN) && ut->pFuncs->close != nullptr)  {
            ut->pFuncs->close(ut);
        }
        ut->flags &= ~UTEXT_OPEN;
        if (extraSpace > ut->extraSize) {
            if (ut->flags & UTEXT_EXTRA_HEAP_ALLOCATED) {
                free_nid_postfix(ut->pExtra);
                ut->extraSize = 0;
            }
            ut->pExtra = malloc_nid_postfix(extraSpace);
            if (ut->pExtra == nullptr) {
                *status = 7;
            } else {
                ut->extraSize = extraSpace;
                ut->flags |= UTEXT_EXTRA_HEAP_ALLOCATED;
            }
        }
    }
    if (*status <= 0) {
        ut->flags |= UTEXT_OPEN;
        ut->context             = nullptr;
        ut->chunkContents       = nullptr;
        ut->p                   = nullptr;
        ut->q                   = nullptr;
        ut->r                   = nullptr;
        ut->a                   = 0;
        ut->b                   = 0;
        ut->c                   = 0;
        ut->chunkOffset         = 0;
        ut->chunkLength         = 0;
        ut->chunkNativeStart    = 0;
        ut->chunkNativeLimit    = 0;
        ut->nativeIndexingLimit = 0;
        ut->providerProperties  = 0;
        ut->privA               = 0;
        ut->privB               = 0;
        ut->privC               = 0;
        ut->privP               = nullptr;
        if (ut->pExtra != nullptr && ut->extraSize > 0)
            std::memset(ut->pExtra, 0, ut->extraSize);
    }
    return ut;
}
extern "C" UText* APS5_VABI
utext_close(UText *ut) {
    if (!ut) return nullptr;
    if (ut->magic != UTEXT_MAGIC) NotImplemented_nid_no_patch(__func__);
    if ((ut->flags & UTEXT_OPEN) == 0) {
        return ut;
    }
    if (!ut->pFuncs) NotImplemented_nid_no_patch(__func__);
    if (ut->pFuncs->close != nullptr) {
        ut->pFuncs->close(ut);
    }
    ut->flags &= ~UTEXT_OPEN;
    if (ut->flags & UTEXT_EXTRA_HEAP_ALLOCATED) {
        free_nid_postfix(ut->pExtra);
        ut->pExtra = nullptr;
        ut->flags &= ~UTEXT_EXTRA_HEAP_ALLOCATED;
        ut->extraSize = 0;
    }
    ut->pFuncs        = nullptr;
    if (ut->flags & UTEXT_HEAP_ALLOCATED) {
        ut->magic = 0;
        free_nid_postfix(ut);
        ut = nullptr;
    }
    return ut;
}
