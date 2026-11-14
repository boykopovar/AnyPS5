#pragma once

#include "prx/libSceNpCppWebApi/include/Api.hpp"
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace NpCppWebApiTest {

class LibContext {
public:
    LibContext() {
        std::memset(params_, 0, sizeof(params_));
        _ZN3sce2Np9CppWebApi6Common10InitParamsC1Ev(params_);
        _ZN3sce2Np9CppWebApi6Common10LibContextC1Ev(&value_);
        if (_ZN3sce2Np9CppWebApi6Common10initializeERKNS2_10InitParamsERNS2_10LibContextE(params_, &value_) != 0) std::abort();
    }

    ~LibContext() {
        if (_ZN3sce2Np9CppWebApi6Common9terminateERNS2_10LibContextE(&value_) != 0) std::abort();
    }

    LibContext(const LibContext&) = delete;
    LibContext& operator=(const LibContext&) = delete;

    std::uint64_t* value() { return &value_; }

private:
    alignas(8) unsigned char params_[0x28];
    std::uint64_t value_ = 0;
};

}
