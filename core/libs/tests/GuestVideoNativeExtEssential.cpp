#include "prx/libSceVideoNativeExtEssential/libSceVideoNativeExtEssential.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceVideoNativeExtEssential_IsInit() == 0);
    Require(sceVideoNativeExtEssential_Initialize() == 0);
    Require(sceVideoNativeExtEssential_IsInit() == 1);
    Require(sceVideoNativeExtEssential_Finalize() == 0);
    Require(sceVideoNativeExtEssential_IsInit() == 0);
    return 0;
}
