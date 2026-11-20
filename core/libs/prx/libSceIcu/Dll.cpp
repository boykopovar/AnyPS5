// Derived from icu4c/source/common/putil.cpp@84e1f26ea77152936e70d53178a816dbfbf69989 (ICU 68.2)
// © 2016 and later: Unicode, Inc. and others.
// License & terms of use: http://www.unicode.org/copyright.html
/*
******************************************************************************
*
*   Copyright (C) 1997-2016, International Business Machines
*   Corporation and others.  All Rights Reserved.
*
******************************************************************************
*/

#include "prx/libSceIcu/Dll.hpp"
#include "prx/libc/include/General.hpp"
#include <cerrno>
#include <cstdint>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace {

constexpr std::int32_t MissingResource = 2;
constexpr std::int32_t IllegalArgument = 1;

bool ReadStatus(const std::int32_t* status, std::int32_t& value) {
    if (!status) return false;
    const int saved = errno;
    const bool copied = GuestCopyIn_nid_no_patch(&value, status, sizeof(value));
    errno = saved;
    return copied;
}

void WriteStatus(std::int32_t* status, std::int32_t value) {
    if (!status) return;
    const int saved = errno;
    GuestCopyOut_nid_no_patch(status, &value, sizeof(value));
    errno = saved;
}

std::string ReadName(const char* library) {
    for (std::size_t capacity = 64; capacity <= 4096; capacity *= 2) {
        std::string name;
        const int saved = errno;
        const int error = CopyGuestString_nid_no_patch(library, static_cast<int>(capacity), name);
        errno = saved;
        if (error == 0) return name;
        if (error != 63) break;
    }
    return {};
}

}

extern "C" void* APS5_VABI uprv_dl_open_nid_postfix(const char* library, std::int32_t* status) {
    std::int32_t value = 0;
    if (!ReadStatus(status, value)) return nullptr;
    if (value > 0) return nullptr;
    if (library == nullptr) {
        WriteStatus(status, IllegalArgument);
        return nullptr;
    }
    const std::string name = ReadName(library);
    if (name.empty()) {
        WriteStatus(status, IllegalArgument);
        return nullptr;
    }
#if defined(_WIN32)
    HMODULE handle = LoadLibraryA(name.c_str());
#else
    void* handle = dlopen(name.c_str(), RTLD_NOW | RTLD_GLOBAL);
#endif
    if (handle == nullptr) {
        WriteStatus(status, MissingResource);
        return nullptr;
    }
    return reinterpret_cast<void*>(handle);
}

extern "C" void* APS5_VABI uprv_dlsym_func_nid_postfix(void* library, const char* symbol, std::int32_t* status) {
    std::int32_t value = 0;
    if (!ReadStatus(status, value)) return nullptr;
    if (value > 0) return nullptr;
    if (library == nullptr || symbol == nullptr) {
        WriteStatus(status, IllegalArgument);
        return nullptr;
    }
    const std::string symbolName = ReadName(symbol);
    if (symbolName.empty()) {
        WriteStatus(status, IllegalArgument);
        return nullptr;
    }
#if defined(_WIN32)
    FARPROC address = GetProcAddress(reinterpret_cast<HMODULE>(library), symbolName.c_str());
    void* result = reinterpret_cast<void*>(address);
#else
    void* result = dlsym(library, symbolName.c_str());
#endif
    if (result == nullptr) {
        WriteStatus(status, MissingResource);
        return nullptr;
    }
    return result;
}

extern "C" void APS5_VABI uprv_dl_close_nid_postfix(void* library, std::int32_t* status) {
    std::int32_t value = 0;
    if (!ReadStatus(status, value)) return;
    if (value > 0 || library == nullptr) return;
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(library));
#else
    dlclose(library);
#endif
}

extern "C" std::int32_t APS5_VABI uprv_isASCIILetter_nid_postfix(char value) {
    const auto character = static_cast<unsigned char>(value);
    return ((character >= 0x41 && character <= 0x5a) || (character >= 0x61 && character <= 0x7a)) ? 1 : 0;
}
