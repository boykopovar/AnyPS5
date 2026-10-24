#include "prx/libc/include/general/VabiMacros.hpp"
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>

extern "C" {
int APS5_VABI X509_EXTENSION_get_critical_nid_postfix(const void*);
void* APS5_VABI X509_EXTENSION_get_data_nid_postfix(const void*);
void* APS5_VABI X509_EXTENSION_get_object_nid_postfix(const void*);
}

static void Require(bool value, const char* what) {
    if (value) return;
    std::fprintf(stderr, "failed: %s\n", what);
    std::abort();
}

int main() {
    static_assert(sizeof(void*) == 8);
    Require(X509_EXTENSION_get_critical_nid_postfix(nullptr) == 0, "null extension is not critical");
    Require(X509_EXTENSION_get_data_nid_postfix(nullptr) == nullptr, "null extension has no data");
    Require(X509_EXTENSION_get_object_nid_postfix(nullptr) == nullptr, "null extension has no object");

    struct CriticalCase {
        std::int32_t stored;
        int expected;
    };
    constexpr CriticalCase criticalCases[] {
        {std::numeric_limits<std::int32_t>::min(), 0}, {-256, 0}, {-1, 0}, {0, 0},
        {1, 1}, {127, 1}, {128, 1}, {255, 1}, {256, 1}, {65536, 1},
        {std::numeric_limits<std::int32_t>::max(), 1}
    };
    unsigned char object[16] {};
    unsigned char octetString[24] {};
    alignas(8) std::array<unsigned char, 40> storage;
    void* extension = storage.data() + 8;
    void* pointers[] {nullptr, object, octetString, extension,
        reinterpret_cast<void*>(std::uintptr_t{0x00007abc12345678})};
    for (void* objectPointer : pointers) {
        for (void* dataPointer : pointers) {
            for (const auto& item : criticalCases) {
                for (unsigned char padding : {0x00, 0x7f, 0x80, 0xff}) {
                    storage.fill(padding);
                    std::memcpy(storage.data() + 8, &objectPointer, 8);
                    std::memcpy(storage.data() + 16, &item.stored, 4);
                    std::memcpy(storage.data() + 24, &dataPointer, 8);
                    const auto before = storage;
                    Require(X509_EXTENSION_get_critical_nid_postfix(extension) == item.expected, "signed critical field ignores padding");
                    Require(X509_EXTENSION_get_object_nid_postfix(extension) == objectPointer, "borrowed object pointer is preserved");
                    Require(X509_EXTENSION_get_data_nid_postfix(extension) == dataPointer, "borrowed ASN1 string pointer is preserved");
                    Require(storage == before, "getters preserve the extension and adjacent bytes");
                }
            }
        }
    }

    void* dataPointer = octetString;
    std::memcpy(storage.data() + 24, &dataPointer, 8);
    auto* borrowed = static_cast<unsigned char*>(X509_EXTENSION_get_data_nid_postfix(extension));
    borrowed[3] = 0x5a;
    Require(octetString[3] == 0x5a, "data is borrowed, not copied or decoded");
    std::puts("libSceLibreSsl X509 extension tests passed");
}
