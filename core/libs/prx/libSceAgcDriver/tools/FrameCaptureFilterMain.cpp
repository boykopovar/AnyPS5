#include "FrameCaptureFilter.hpp"
#include <algorithm>
#include <cstdio>
#include <exception>
#include <filesystem>
#include <string_view>

namespace {

int usage() {
    std::fprintf(stderr, "usage: agc_frame_capture_filter inspect <capture>\n"
                         "       agc_frame_capture_filter apply <capture> <plan> <output>\n");
    return 2;
}

template <typename TChar>
int run(int argc, TChar** argv) {
    const auto command = [&](std::string_view name) { return std::ranges::equal(std::basic_string_view<TChar>(argv[1]), name); };
    if (argc == 3 && command("inspect")) {
        try {
            std::fputs(AgcDriverTools::DescribeFrameCapture(std::filesystem::path(argv[2])).c_str(), stdout);
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
    if (argc == 5 && command("apply")) {
        try {
            AgcDriverTools::FilterFrameCapture(std::filesystem::path(argv[2]), std::filesystem::path(argv[4]), AgcDriverTools::ReadFrameCaptureFilterPlan(std::filesystem::path(argv[3])));
            return 0;
        } catch (const std::exception& error) {
            std::fprintf(stderr, "%s\n", error.what());
            return 1;
        }
    }
    return usage();
}

}

#ifdef _WIN32
int wmain(int argc, wchar_t** argv) {
    return run(argc, argv);
}
#else
int main(int argc, char** argv) {
    return run(argc, argv);
}
#endif
