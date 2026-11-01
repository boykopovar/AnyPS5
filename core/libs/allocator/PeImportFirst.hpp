#ifndef CORE_LIBS_ALLOCATOR_PEIMPORTFIRST_HPP
#define CORE_LIBS_ALLOCATOR_PEIMPORTFIRST_HPP

#include <filesystem>
#include <string>
#include <vector>

namespace Allocator {

void MoveImportFirst(std::vector<unsigned char>& image, const std::string& dll);
void MoveImportFirstInFile(const std::filesystem::path& path, const std::string& dll);

}

#endif
