#pragma once

#include <Testing/Test.hpp>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <vector>

class ScopedEnvironment {
public:
    ScopedEnvironment(const char* name, const std::string& value) : name(name) {
        if (const char* current = std::getenv(name)) previous = current;
        Set(value);
    }

    ~ScopedEnvironment() {
        if (previous) {
            Set(*previous);
        } else {
#ifdef _WIN32
            _putenv_s(name, "");
#else
            unsetenv(name);
#endif
        }
    }

    ScopedEnvironment(const ScopedEnvironment&) = delete;
    ScopedEnvironment& operator=(const ScopedEnvironment&) = delete;

private:
    void Set(const std::string& value) const {
#ifdef _WIN32
        _putenv_s(name, value.c_str());
#else
        setenv(name, value.c_str(), 1);
#endif
    }

    const char* const name;
    std::optional<std::string> previous;
};

class DiskAudioCapture {
public:
    DiskAudioCapture() : driver("SDL_AUDIODRIVER", "disk"), file(directory.Path() / "capture.raw"), output("SDL_DISKAUDIOFILE", file.string()) {}

    template<typename TSample>
    std::vector<TSample> Samples() const {
        std::ifstream stream(file, std::ios::binary);
        const std::vector<char> bytes{std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>()};
        Testing::RequireEqual(bytes.size() % sizeof(TSample), std::size_t{0}, "captured byte count is a whole number of samples");
        const auto* samples = reinterpret_cast<const TSample*>(bytes.data());
        return {samples, samples + bytes.size() / sizeof(TSample)};
    }

    template<typename TSample>
    std::vector<TSample> TrimmedSamples() const {
        const auto samples = Samples<TSample>();
        std::size_t first = 0;
        std::size_t last = samples.size();
        while (first < last && samples[first] == TSample{}) first++;
        while (last > first && samples[last - 1] == TSample{}) last--;
        return {samples.begin() + static_cast<std::ptrdiff_t>(first), samples.begin() + static_cast<std::ptrdiff_t>(last)};
    }

private:
    const Testing::TemporaryDirectory directory;
    const ScopedEnvironment driver;
    const std::filesystem::path file;
    const ScopedEnvironment output;
};
