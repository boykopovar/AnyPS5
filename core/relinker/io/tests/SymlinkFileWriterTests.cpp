#include <io/FileWriter.hpp>
#include <domain/Types.hpp>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <vector>
#include <sys/resource.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

void require(bool condition, const std::string& message) {
    if (!condition) throw std::runtime_error(message);
}

std::string readFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
}

class TemporaryDirectory {
public:
    std::filesystem::path Path;

    TemporaryDirectory() {
        auto name = (std::filesystem::temp_directory_path() / "anyps5-symlink-writer-XXXXXX").string();
        require(mkdtemp(name.data()) != nullptr, "Cannot create test directory");
        Path = name;
    }

    ~TemporaryDirectory() {
        std::error_code error;
        std::filesystem::remove_all(Path, error);
    }
};

template<typename TValue>
void checkReplacement(const std::filesystem::path& root, const TValue& data, const std::string& kind) {
    const auto directory = root / kind;
    std::filesystem::create_directories(directory / "targets");
    const auto target = directory / "targets" / "output";
    const auto link = directory / "link";
    const std::string previous = "previous output";
    std::ofstream(target, std::ios::binary) << previous;
    const auto permissions = std::filesystem::perms::owner_all | std::filesystem::perms::group_read;
    std::filesystem::permissions(target, permissions);
    if (kind == "absolute") {
        std::filesystem::create_symlink(std::filesystem::absolute(target), link);
    } else if (kind == "relative") {
        std::filesystem::create_symlink("targets/output", link);
    } else {
        std::filesystem::create_symlink("output", directory / "targets" / "indirect");
        std::filesystem::create_symlink("targets/indirect", link);
    }
    const auto linkValue = std::filesystem::read_symlink(link);
    const auto child = fork();
    require(child >= 0, "Cannot fork write-failure probe");
    if (child == 0) {
        rlimit limit{};
        if (getrlimit(RLIMIT_FSIZE, &limit) != 0) _exit(3);
        limit.rlim_cur = 1;
        if (std::signal(SIGXFSZ, SIG_IGN) == SIG_ERR || setrlimit(RLIMIT_FSIZE, &limit) != 0) _exit(4);
        try {
            Io::FileWriter().Write(link.string(), data);
        } catch (const Domain::RelinkerException& error) {
            _exit(std::string(error.what()) == "Failed to write file: " + link.string() ? 0 : 5);
        } catch (...) {
            _exit(6);
        }
        _exit(7);
    }
    int status = 0;
    require(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0,
            "Write failure did not report the requested symlink path");
    require(readFile(target) == previous, kind + " failed write changed the previous target");
    require(std::filesystem::read_symlink(link) == linkValue, "Failed write changed the symlink");
    const auto alias = directory / "old-output";
    std::filesystem::create_hard_link(target, alias);
    std::ifstream opened(target, std::ios::binary);
    require(opened.is_open(), "Cannot open the previous target");
    const auto mask = umask(077);
    try {
        Io::FileWriter().Write(link.string(), data);
    } catch (...) {
        umask(mask);
        throw;
    }
    umask(mask);
    require(readFile(target) == std::string(data.begin(), data.end()), "Target was not replaced");
    require(std::filesystem::read_symlink(link) == linkValue, "Successful write replaced the symlink");
    require((std::filesystem::status(target).permissions() & std::filesystem::perms::all) == permissions,
            "Replacement changed the target permissions");
    require(readFile(alias) == previous, "Replacement rewrote another hard link");
    require(std::string(std::istreambuf_iterator<char>(opened), std::istreambuf_iterator<char>()) == previous,
            "Replacement changed the previous open file");
    if (kind == "chained")
        require(std::filesystem::read_symlink(directory / "targets" / "indirect") == "output", "Intermediate link changed");
    for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
        require(!entry.path().filename().string().starts_with(".anyps5-"), "Temporary file left behind");
}

template<typename TValue>
void checkFallbacks(const std::filesystem::path& directory, const TValue& data) {
    const auto dangling = directory / "dangling";
    std::filesystem::create_symlink("new-output", dangling);
    Io::FileWriter().Write(dangling.string(), data);
    require(std::filesystem::read_symlink(dangling) == "new-output", "Dangling link was replaced");
    require(readFile(directory / "new-output") == std::string(data.begin(), data.end()), "Dangling target not written");
    const auto device = directory / "device";
    std::filesystem::create_symlink("/dev/null", device);
    Io::FileWriter().Write(device.string(), data);
    require(std::filesystem::read_symlink(device) == "/dev/null", "Device symlink was replaced");
}

template<typename TValue>
void check(const TValue& data) {
    TemporaryDirectory directory;
    for (const auto& kind : {"absolute", "relative", "chained"})
        checkReplacement(directory.Path, data, kind);
    checkFallbacks(directory.Path, data);
}

}

int main(int argc, char* argv[]) {
    try {
        require(argc == 2, "Expected binary or text");
        for (const std::size_t size : {std::size_t{3}, std::size_t{65536}}) {
            if (std::string(argv[1]) == "binary") check(std::vector<std::uint8_t>(size, 0x41));
            else if (std::string(argv[1]) == "text") check(std::string(size, 'A'));
            else throw std::runtime_error("Expected binary or text");
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
