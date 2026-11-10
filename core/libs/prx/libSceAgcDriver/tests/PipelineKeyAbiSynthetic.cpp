#include "PipelineKeyAbi.hpp"
#include <string_view>

int main(int argc, char** argv) {
    if (argc != 2 || (std::string_view(argv[1]) != "conforming" && std::string_view(argv[1]) != "clobbering")) return 1;
    return PipelineKeyAbiSynthetic(std::string_view(argv[1]) == "clobbering");
}
