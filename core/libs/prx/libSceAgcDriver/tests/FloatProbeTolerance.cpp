#include "prx/libSceAgcDriver/Execution/include/FloatProbeTolerance.hpp"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

using AgcDriver::Float32ProbeOp;
using AgcDriver::Float32ResultsAgree;

namespace {

void Require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

constexpr std::array<std::array<std::uint32_t, 3>, 10> kRdna2Rcp{{
    {0x40400000u, 0x3eaaaaaau, 0x3eaaaaabu}, {0x40e00000u, 0x3e124924u, 0x3e124925u}, {0xc0e00000u, 0xbe124924u, 0xbe124925u},
    {0x3fc00000u, 0x3f2aaaaau, 0x3f2aaaabu}, {0xbfc00000u, 0xbf2aaaaau, 0xbf2aaaabu}, {0x3f400000u, 0x3faaaaaau, 0x3faaaaabu},
    {0x41f00000u, 0x3d088888u, 0x3d088889u}, {0xc1f00000u, 0xbd088888u, 0xbd088889u}, {0x4640e6b7u, 0x38a9de8bu, 0x38a9de8cu},
    {0x40400000u, 0x3eaaaaabu, 0x3eaaaaaau},
}};

void checkWithinBounds() {
    for (const auto& [input, unpreserved, preserved] : kRdna2Rcp) {
        Require(Float32ResultsAgree(Float32ProbeOp::Rcp, input, unpreserved, preserved), "AMD RDNA2 rcp pair 1 ulp apart rejected");
    }
    Require(Float32ResultsAgree(Float32ProbeOp::Rcp, 0x40400000u, 0x3eaaaaaau, 0x3eaaaaafu), "rcp(3.0) 5 ulp apart rejected");
    Require(Float32ResultsAgree(Float32ProbeOp::Rsq, 0x40000000u, 0x3f3504f3u, 0x3f3504f7u), "rsq(2.0) 4 ulp apart rejected");
    Require(Float32ResultsAgree(Float32ProbeOp::Sqrt, 0x40000000u, 0x3fb504f3u, 0x3fb504fcu), "sqrt(2.0) 9 ulp apart rejected");
    Require(Float32ResultsAgree(Float32ProbeOp::Exp2, 0x42c80000u, 0x71800000u, 0x71800000u + 406u), "exp2(100) 406 ulp apart rejected");
    Require(Float32ResultsAgree(Float32ProbeOp::Log2, 0x3f400000u, 0xbed47fccu, 0xbed47fccu + 32u), "log2(0.75) 32 ulp apart rejected");
    Require(Float32ResultsAgree(Float32ProbeOp::Log2, 0x41200000u, 0x40549a78u, 0x40549a7eu), "log2(10) 6 ulp apart rejected");
    Require(Float32ResultsAgree(Float32ProbeOp::Log2, 0xbf800000u, 0x7fc00000u, 0xffc00000u), "two NaNs rejected");
}

void checkOutsideBounds() {
    Require(!Float32ResultsAgree(Float32ProbeOp::Log2, 0x40000000u, 0x3f800000u, 0x41c80000u), "log2(2.0) = 25.0 accepted");
    Require(!Float32ResultsAgree(Float32ProbeOp::Log2, 0x3abb690au, 0xc11732f8u, 0xc3ae99c6u), "log2(0.00143) = -349.2 accepted");
    Require(!Float32ResultsAgree(Float32ProbeOp::Rcp, 0x40400000u, 0x3eaaaaaau, 0x3eaaaab0u), "rcp(3.0) 6 ulp apart accepted");
    Require(!Float32ResultsAgree(Float32ProbeOp::Exp2, 0x42c80000u, 0x71800000u, 0x71800000u + 407u), "exp2(100) 407 ulp apart accepted");
    Require(!Float32ResultsAgree(Float32ProbeOp::Log2, 0x41200000u, 0x40549a78u, 0x40549a7fu), "log2(10) 7 ulp apart accepted");
    Require(!Float32ResultsAgree(Float32ProbeOp::Rsq, 0x40000000u, 0x3f3504f3u, 0x3f3504f8u), "rsq(2.0) 5 ulp apart accepted");
    Require(!Float32ResultsAgree(Float32ProbeOp::Sqrt, 0xbf800000u, 0x7fc00000u, 0x00000000u), "NaN against 0.0 accepted");
    Require(!Float32ResultsAgree(Float32ProbeOp::Rcp, 0x00000000u, 0x7f800000u, 0xff800000u), "+inf against -inf accepted");
}

}

int main() {
    try {
        checkWithinBounds();
        checkOutsideBounds();
        std::cout << "Float probe tolerance bounds, NaN and RADV log2 error tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
