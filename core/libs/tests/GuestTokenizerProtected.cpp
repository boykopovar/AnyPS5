#include "prx/libc/include/Api.hpp"
#include "tests/GuardedBuffer.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void Require(bool condition) {
    if (!condition) {
        std::fputs("GuestTokenizerProtected check failed\n", stderr);
        std::abort();
    }
}

#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#include <unistd.h>
#endif

int main() {
    GuardedBuffer data, delimiters;
    for (unsigned value : {1u, 44u, 127u, 128u, 255u}) {
        const char delimiter = static_cast<char>(value);
        std::string separators(1, delimiter);
        separators += ':';
        auto* set = reinterpret_cast<char*>(delimiters.At(3));
        std::memcpy(set, separators.c_str(), 3);
        const char payload = value == 'x' ? 'q' : 'x';
        for (std::size_t repeats = 0; repeats <= 32; ++repeats) {
            std::string input(1, delimiter);
            input += std::string(repeats, payload);
            input += ":";
            input += delimiter;
            input += "tail:";
            std::vector<std::string> fields;
            std::size_t begin = 0;
            for (std::size_t i = 0; i <= input.size(); ++i) {
                if (i == input.size() || input[i] == delimiter || input[i] == ':') {
                    fields.push_back(input.substr(begin, i - begin));
                    begin = i + 1;
                }
            }
            auto* text = reinterpret_cast<char*>(data.At(input.size() + 1));
            std::memcpy(text, input.c_str(), input.size() + 1);
            char* cursor = text;
            for (const auto& field : fields) {
                char* token = strsep_nid_postfix(&cursor, set);
                Require(token != nullptr && std::string(token) == field);
            }
            Require(strsep_nid_postfix(&cursor, set) == nullptr);
            std::memcpy(text, input.c_str(), input.size() + 1);
            char* state = nullptr;
            char* token = strtok_r_nid_postfix(text, set, &state);
            for (const auto& field : fields) {
                if (field.empty()) continue;
                Require(token != nullptr && std::string(token) == field);
                token = strtok_r_nid_postfix(nullptr, set, &state);
            }
            Require(token == nullptr);
            for (std::size_t i = 0; i < input.size(); ++i) {
                if (input[i] != delimiter && input[i] != ':') Require(text[i] == input[i]);
            }
            Require(std::memcmp(set, separators.c_str(), 3) == 0);
        }
    }
}
