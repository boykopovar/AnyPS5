#ifndef IO_FILEWRITER_HPP
#define IO_FILEWRITER_HPP

#include <io/IFileWriter.hpp>

namespace Io {

class FileWriter : public IFileWriter {
public:
    void Write(const std::string& path, const std::vector<std::uint8_t>& data) override;
    void Write(const std::string& path, const std::string& content) override;
};

}

#endif
