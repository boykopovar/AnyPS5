#include "prx/libc/include/general/VabiMacros.hpp"
#include "prx/libSceNKWebKit/libSceNKWebKit.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>

int main() {
    int constraint = 0;
    int visitor = 0;
    const double estimate = _ZN3JSC17MarkingConstraint17quickWorkEstimateERNS_11SlotVisitorE(&constraint, &visitor);
    if (estimate != 0 || std::signbit(estimate) || constraint != 0 || visitor != 0) {
        std::fprintf(stderr, "quickWorkEstimate returned %g\n", estimate);
        return 1;
    }
    if (_ZN3JSC17MarkingConstraint17quickWorkEstimateERNS_11SlotVisitorE(nullptr, nullptr) != 0) {
        std::fprintf(stderr, "quickWorkEstimate without pointers is not zero\n");
        return 1;
    }
    return 0;
}
