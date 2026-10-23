#include <cstddef>
#include <cstring>
#include "prx/libc/include/General.hpp"

using UiSessionFunction = int (APS5_VABI*)(void*);
using UiStringFunction = int (APS5_VABI*)(void*, void*);
using UiPromptFunction = char* (APS5_VABI*)(void*, const char*, const char*);

struct LibreSslUiMethod {
    char* name;
    UiSessionFunction openSession;
    UiStringFunction writeString;
    UiSessionFunction flush;
    UiStringFunction readString;
    UiSessionFunction closeSession;
    UiPromptFunction constructPrompt;
};

static_assert(sizeof(LibreSslUiMethod) == 56);

extern "C" {
void* APS5_VABI malloc_nid_postfix(std::size_t size);

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

}
