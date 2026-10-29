#include "prx/libSceLibreSsl/include/Api.hpp"
#include "prx/libc/include/Api.hpp"
#include <cstddef>
#include <cstring>
#include <stdexcept>
#include <string>
#include "prx/libc/include/General.hpp"

static_assert(sizeof(LibreSslUiMethod) == 56);

extern "C" {
LibreSslUiMethod* APS5_VABI UI_create_method_nid_postfix(char* name) {
    auto* method = static_cast<LibreSslUiMethod*>(malloc_nid_postfix(sizeof(LibreSslUiMethod)));
    if (!method) return nullptr;
    std::memset(method, 0, sizeof(LibreSslUiMethod));
    if (name) {
        const std::size_t size = std::strlen(name) + 1;
        method->name = static_cast<char*>(malloc_nid_postfix(size));
        if (method->name) std::memcpy(method->name, name, size);
    }
    return method;
}

void APS5_VABI UI_destroy_method_nid_postfix(LibreSslUiMethod* method) {
    if (!method) throw std::invalid_argument(std::string(__func__) + ": null method");
    free_nid_postfix(method->name);
    method->name = nullptr;
    free_nid_postfix(method);
}

int APS5_VABI UI_method_set_opener_nid_postfix(LibreSslUiMethod* method, UiSessionFunction opener) {
    if (!method) return -1;
    method->openSession = opener;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_opener_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->openSession : nullptr;
}

int APS5_VABI UI_method_set_writer_nid_postfix(LibreSslUiMethod* method, UiStringFunction writer) {
    if (!method) return -1;
    method->writeString = writer;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_writer_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->writeString : nullptr;
}

int APS5_VABI UI_method_set_reader_nid_postfix(LibreSslUiMethod* method, UiStringFunction reader) {
    if (!method) return -1;
    method->readString = reader;
    return 0;
}

UiStringFunction APS5_VABI UI_method_get_reader_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->readString : nullptr;
}

int APS5_VABI UI_method_set_closer_nid_postfix(LibreSslUiMethod* method, UiSessionFunction closer) {
    if (!method) return -1;
    method->closeSession = closer;
    return 0;
}

UiSessionFunction APS5_VABI UI_method_get_closer_nid_postfix(const LibreSslUiMethod* method) {
    return method ? method->closeSession : nullptr;
}

}
