#include "PeImportFirst.hpp"

#include <cstdio>
#include <exception>

int main(int argc, char** argv) {
    if (argc != 3) {
        std::fprintf(stderr, "usage: pe_import_first <file> <dll>\n");
        return 2;
    }
    try {
        Allocator::MoveImportFirstInFile(argv[1], argv[2]);
    } catch (const std::exception& error) {
        std::fprintf(stderr, "pe_import_first: %s: %s\n", argv[1], error.what());
        return 1;
    }
    return 0;
}
