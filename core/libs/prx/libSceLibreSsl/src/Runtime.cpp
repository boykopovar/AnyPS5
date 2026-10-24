#include <cstddef>
#include <cstdint>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"
#include "prx/libc/include/HostEntropy.hpp"

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t size);
void APS5_VABI free_nid_postfix(void* pointer);

void APS5_VABI CONF_modules_free_nid_postfix(void) {
}

void APS5_VABI OPENSSL_load_builtin_modules_nid_postfix(void) {
}

void APS5_VABI OPENSSL_add_all_algorithms_noconf_nid_postfix(void) {
}

void APS5_VABI RAND_add_nid_postfix(const void*, int, double) {
}

int APS5_VABI RAND_status_nid_postfix(void) {
    return 1;
}

int APS5_VABI RAND_bytes_nid_postfix(unsigned char* buffer, int count) {
    if (count > 0) {
        if (!buffer) throw std::invalid_argument(std::string(__func__) + ": null buffer");
        HostEntropy_nid_no_patch(buffer, static_cast<std::size_t>(count));
    }
    return 1;
}

int APS5_VABI RAND_load_file_nid_postfix(const char*, std::int64_t bytes) {
    return bytes == -1 ? 123456 : static_cast<int>(bytes);
}

const char* APS5_VABI RAND_file_name_nid_postfix(char* buffer, std::size_t size) {
    static constexpr char path[] = "/dev/urandom";
    if (size) {
        if (!buffer) throw std::invalid_argument(std::string(__func__) + ": null buffer");
        const std::size_t copied = size - 1 < sizeof(path) - 1 ? size - 1 : sizeof(path) - 1;
        std::memcpy(buffer, path, copied);
        buffer[copied] = '\0';
    }
    return sizeof(path) - 1 >= size ? nullptr : buffer;
}

void* APS5_VABI CRYPTO_malloc_nid_postfix(int count, const char*, int) {
    if (count <= 0) return nullptr;
    return malloc_nid_postfix(static_cast<std::size_t>(count));
}

void APS5_VABI CRYPTO_free_nid_postfix(void* pointer) {
    free_nid_postfix(pointer);
}

int APS5_VABI CRYPTO_set_mem_functions_nid_postfix(void*, void*, void*) {
    return 0;
}

}
