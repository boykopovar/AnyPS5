#include <stdexcept>
#include <string>

extern "C" void NotImplemented_nid_no_patch(const char* funcName) {
    throw std::runtime_error(std::string(funcName) + " not implemented");
}
