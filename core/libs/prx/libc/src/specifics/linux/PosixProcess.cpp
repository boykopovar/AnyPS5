#include <cstdlib>
#include <cerrno>
#include <cstring>

extern "C" {

int APS5_VABI setenv_nid_postfix(const char* name, const char* value, int overwrite) {
    if (overwrite == 0 && std::getenv(name) != nullptr) {
        return 0;
    }
    const size_t nameLength = std::strlen(name);
    const size_t valueLength = std::strlen(value);
    char* entry = static_cast<char*>(std::malloc(nameLength + valueLength + 2));
    if (entry == nullptr) {
        errno = ENOMEM;
        return -1;
    }
    std::memcpy(entry, name, nameLength);
    entry[nameLength] = '=';
    std::memcpy(entry + nameLength + 1, value, valueLength);
    entry[nameLength + 1 + valueLength] = '\0';
    return ::putenv(entry);
}

}
