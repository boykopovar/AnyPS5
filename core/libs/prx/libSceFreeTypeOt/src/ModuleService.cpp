/***************************************************************************/
/*                                                                         */
/*  ftobjs.c                                                               */
/*                                                                         */
/*    The FreeType private base classes (body).                            */
/*                                                                         */
/*  Copyright 1996-2013 by                                                 */
/*  David Turner, Robert Wilhelm, and Werner Lemberg.                      */
/*                                                                         */
/*  This file is part of the FreeType project, and may only be used,       */
/*  modified, and distributed under the terms of the FreeType project      */
/*  license, LICENSE.TXT.  By continuing to use, modify, or distribute     */
/*  this file you indicate that you have read the license and              */
/*  understand and accept it fully.                                        */
/*                                                                         */
/***************************************************************************/
// https://github.com/freetype/freetype/blob/cecf93ef90c660a8b0b45e5adbbfb5ea443fb6b9/src/base/ftobjs.c

#include "prx/libSceFreeTypeOt/include/ModuleService.hpp"
#include "prx/libc/include/General.hpp"

#include <cerrno>
#include <iterator>

namespace {

template <typename TValue>
TValue Read(const TValue* source, const char* function) {
    TValue value{};
    const int savedErrno = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, source, sizeof(value));
    errno = savedErrno;
    if (!copied) NotImplemented_nid_no_patch(function);
    return value;
}

void* Request(FreeType::GuestServiceModuleRec* module, const char* serviceId, const char* function) {
    if (!module) NotImplemented_nid_no_patch(function);
    const auto* clazz = Read(&module->clazz, function);
    if (!clazz) NotImplemented_nid_no_patch(function);
    const auto requester = reinterpret_cast<FreeType::GuestServiceRequester>(Read(&clazz->get_interface, function));
    return requester ? requester(module, serviceId) : nullptr;
}

}

extern "C" void* APS5_VABI ft_module_get_service_nid_postfix(FreeType::GuestServiceModuleRec* module, const char* serviceId) {
    if (!module) return nullptr;
    if (void* service = Request(module, serviceId, __func__)) return service;
    const auto* library = Read(&module->library, __func__);
    if (!library) NotImplemented_nid_no_patch(__func__);
    const auto count = Read(&library->numModules, __func__);
    if (count > std::size(library->modules)) NotImplemented_nid_no_patch(__func__);
    for (std::uint32_t i = 0; i < count; ++i) {
        auto* candidate = Read(&library->modules[i], __func__);
        if (candidate == module) continue;
        if (void* service = Request(candidate, serviceId, __func__)) return service;
    }
    return nullptr;
}
