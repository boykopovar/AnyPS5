#ifndef IO_FILEREADER_HPP
#define IO_FILEREADER_HPP

#include <io/IFileReader.hpp>

namespace Io {

class FileReader : public IFileReader {
public:
    std::vector<std::uint8_t> Read(const std::string& path) override;
};

}

#endif
