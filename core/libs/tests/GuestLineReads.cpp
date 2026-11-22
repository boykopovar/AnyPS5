#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/ApplicationHeap.hpp"
#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <stdexcept>
#include <string>

namespace {

void Require(bool condition, const char* message = "line read check failed") {
    if (!condition) throw std::runtime_error(message);
}

void* APS5_VABI Allocate(std::size_t size) { return std::malloc(size); }
void APS5_VABI Free(void* pointer) { std::free(pointer); }
void* APS5_VABI Calloc(std::size_t count, std::size_t size) { return std::calloc(count, size); }
void* APS5_VABI Reallocate(void* pointer, std::size_t size) { return std::realloc(pointer, size); }
void* APS5_VABI AlignedAllocate(std::size_t, std::size_t size) { return Allocate(size); }
void* APS5_VABI Realign(void* pointer, std::size_t size, std::size_t) { return Reallocate(pointer, size); }
int APS5_VABI PosixAlign(void** pointer, std::size_t, std::size_t size) {
    if (pointer == nullptr) return 22;
    *pointer = Allocate(size);
    return *pointer == nullptr ? 12 : 0;
}

void RegisterAllocator() {
    std::array<void*, 10> api{};
    api[0] = reinterpret_cast<void*>(&Allocate);
    api[1] = reinterpret_cast<void*>(&Free);
    api[2] = reinterpret_cast<void*>(&Calloc);
    api[3] = reinterpret_cast<void*>(&Reallocate);
    api[4] = reinterpret_cast<void*>(&AlignedAllocate);
    api[5] = reinterpret_cast<void*>(&Realign);
    api[6] = reinterpret_cast<void*>(&PosixAlign);
    ApplicationHeapRegister_nid_no_patch(api.data());
}

}

int main(int argc, char** argv) {
    Require(argc == 2);
    RegisterAllocator();

    const std::string base = argv[1];
    const std::string path = base + ".lines";
    auto* stream = fopen_nid_postfix(path.c_str(), "w+b");
    Require(stream != nullptr);
    constexpr char text[] = "alpha\nbeta;gamma\ntail";
    Require(fwrite_nid_postfix(text, 1, sizeof(text) - 1, stream) == sizeof(text) - 1);
    Require(fflush_nid_postfix(stream) == 0);
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);

    char* line = nullptr;
    std::size_t capacity = 0;
    Require(getline_nid_postfix(&line, &capacity, stream) == 6);
    Require(std::strcmp(line, "alpha\n") == 0 && capacity >= 7);
    Require(getdelim_nid_postfix(&line, &capacity, ';', stream) == 5);
    Require(std::strcmp(line, "beta;") == 0);
    Require(getdelim_nid_postfix(&line, &capacity, '\n', stream) == 6);
    Require(std::strcmp(line, "gamma\n") == 0);
    Require(getline_nid_postfix(&line, &capacity, stream) == 4);
    Require(std::strcmp(line, "tail") == 0);
    char* const remaining = line;
    Require(getline_nid_postfix(&line, &capacity, stream) == -1);
    Require(line == remaining && feof_nid_postfix(stream) != 0 && ferror_nid_postfix(stream) == 0);
    errno = 0;
    Require(getline_nid_postfix(nullptr, &capacity, stream) == -1 && errno == EINVAL);
    Require(getline_nid_postfix(&line, nullptr, stream) == -1 && errno == EINVAL);
    Require(getline_nid_postfix(&line, &capacity, nullptr) == -1 && errno == EINVAL);
    Require(fclose_nid_postfix(stream) == 0);

    const std::string nulPath = base + ".nul";
    stream = fopen_nid_postfix(nulPath.c_str(), "w+b");
    Require(stream != nullptr);
    constexpr char embedded[] = {'x', '\0', 'y', '\n'};
    Require(fwrite_nid_postfix(embedded, 1, sizeof(embedded), stream) == sizeof(embedded));
    Require(fseek_nid_postfix(stream, 0, SEEK_SET) == 0);
    Require(getline_nid_postfix(&line, &capacity, stream) == 4);
    Require(std::memcmp(line, embedded, sizeof(embedded)) == 0);
    Require(fclose_nid_postfix(stream) == 0);

    free_nid_postfix(line);
    Require(std::filesystem::remove(path));
    Require(std::filesystem::remove(nulPath));
    return 0;
}
