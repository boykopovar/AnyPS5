#include "prx/libSceShareFactoryUtil/include/Api.hpp"
#include <cstdlib>

namespace {

void Require(bool value) {
    if (!value) {
        std::abort();
    }
}

}

int main() {
    Require(sceShareFactoryUtilInitialize() == 0);
    Require(sceShareFactoryUtilTerminate() == 0);

    Require(sceShareFactoryUtilInitialize() == 0);
    Require(sceShareFactoryUtilTerminate() == 0);

    return 0;
}
