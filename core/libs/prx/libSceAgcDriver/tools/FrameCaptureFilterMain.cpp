#include "FrameCaptureFilter.hpp"
#include <cstdio>
#include <exception>
#include <string_view>

namespace {

int usage() {
    std::fprintf(stderr, "usage: agc_frame_capture_filter inspect <capture>\n"
                         "       agc_frame_capture_filter apply <capture> <plan> <output>\n");
    return 2;
}

}

int main(int argc, char** argv) {
    if (argc == 3 && std::string_view(argv[1]) == "inspect") {
        try {
            std::fputs(AgcDriverTools::DescribeFrameCapture(argv[2]).c_str(), stdout);
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
    if (argc == 5 && std::string_view(argv[1]) == "apply") {
        try {
            AgcDriverTools::FilterFrameCapture(argv[2], argv[4], AgcDriverTools::ReadFrameCaptureFilterPlan(argv[3]));
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
    return usage();
}
