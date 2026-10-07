#include <pkg/OodleLibrary.hpp>
#include <pkg/PackageError.hpp>

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace Pkg {

namespace {

void release(void* handle) {
    if (handle == nullptr) return;
#ifdef _WIN32
    FreeLibrary(static_cast<HMODULE>(handle));
#else
    dlclose(handle);
#endif
}

}

OodleLibrary::OodleLibrary(const std::filesystem::path& path) {
#ifdef _WIN32
    _handle = LoadLibraryW(path.wstring().c_str());
    if (_handle == nullptr) throw PackageError("Cannot load Oodle library: " + path.string() + " (error " + std::to_string(GetLastError()) + ")");
    _decompress = reinterpret_cast<DecompressFunction>(reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(_handle), "OodleLZ_Decompress")));
#else
    _handle = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (_handle == nullptr) throw PackageError("Cannot load Oodle library: " + path.string() + " (" + dlerror() + ")");
    _decompress = reinterpret_cast<DecompressFunction>(dlsym(_handle, "OodleLZ_Decompress"));
#endif
    if (_decompress == nullptr) {
        release(_handle);
        throw PackageError("Oodle library does not export OodleLZ_Decompress: " + path.string());
    }
}

OodleLibrary::~OodleLibrary() {
    release(_handle);
}

void OodleLibrary::Decompress(const std::vector<std::uint8_t>& stream, std::uint8_t* destination, const std::size_t size) const {
    constexpr int fuzzSafe = 1;
    constexpr int decodeAllPhases = 3;
    const auto produced = _decompress(stream.data(), static_cast<std::intptr_t>(stream.size()), destination, static_cast<std::intptr_t>(size), fuzzSafe, 0, 0,
                                      nullptr, 0, nullptr, nullptr, nullptr, 0, decodeAllPhases);
    if (produced != static_cast<std::intptr_t>(size)) throw PackageError("Oodle failed to decode a Kraken block");
}

}
