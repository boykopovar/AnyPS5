#include "prx/libc/include/Api.hpp"
#include "prx/libc/include/General.hpp"
#include "tests/GuardedBuffer.hpp"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <stdexcept>
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

static void ReadOnly(GuardedBuffer& buffer) {
#ifdef _WIN32
    DWORD previous;
    Require(VirtualProtect(buffer.At(1), 1, PAGE_READONLY, &previous) != 0);
#else
    const long page = sysconf(_SC_PAGESIZE);
    Require(page > 0);
    Require(mprotect(buffer.At(1) + 1 - page, static_cast<std::size_t>(page), PROT_READ) == 0);
#endif
}

static void CheckStrsepFaults() {
    const auto rejects = [](auto call) {
        errno = 71;
        try {
            call();
        } catch (const std::runtime_error&) {
            Require(errno == 71);
            return;
        }
        Require(false);
    };
    GuardedBuffer data, delimiters, state;
    auto* const inaccessible = data.At<char>(1) + 1;
    char probe = 0;
    Require(!GuestCopyIn_nid_no_patch(&probe, inaccessible, 1));
    char input[] = "a,b";
    char* cursor = input;
    rejects([&] { strsep_nid_postfix(nullptr, ","); });
    rejects([&] { strsep_nid_postfix(reinterpret_cast<char**>(inaccessible), ","); });
    rejects([&] { strsep_nid_postfix(reinterpret_cast<char**>(state.At(sizeof(char*) - 1)), ","); });
    rejects([&] { strsep_nid_postfix(&cursor, nullptr); });
    rejects([&] { strsep_nid_postfix(&cursor, inaccessible); });
    Require(cursor == input && std::strcmp(input, "a,b") == 0);
    auto* const unterminated = delimiters.At<char>(1);
    *unterminated = ',';
    rejects([&] { strsep_nid_postfix(&cursor, unterminated); });
    Require(cursor == input && std::strcmp(input, "a,b") == 0);
    cursor = inaccessible;
    rejects([&] { strsep_nid_postfix(&cursor, ","); });
    Require(cursor == inaccessible);
    auto* const lastByte = data.At<char>(1);
    *lastByte = 'a';
    cursor = lastByte;
    rejects([&] { strsep_nid_postfix(&cursor, ","); });
    Require(cursor == lastByte && *lastByte == 'a');
    *lastByte = ',';
    errno = 71;
    Require(strsep_nid_postfix(&cursor, ",") == lastByte && *lastByte == 0 && cursor == inaccessible);
    Require(errno == 71);
    rejects([&] { strsep_nid_postfix(&cursor, ","); });
    cursor = lastByte;
    Require(strsep_nid_postfix(&cursor, ",") == lastByte && cursor == nullptr);
    Require(strsep_nid_postfix(&cursor, inaccessible) == nullptr);

    auto* const slot = state.At<char*>(1);
    *slot = input;
    ReadOnly(state);
    Require(!GuestWritable_nid_no_patch(slot, sizeof(*slot)));
    rejects([&] { strsep_nid_postfix(slot, ","); });
    Require(*slot == input && std::strcmp(input, "a,b") == 0);

    GuardedBuffer readonly;
    auto* const text = readonly.At<char>(6);
    std::memcpy(text, "ro,rw", 6);
    ReadOnly(readonly);
    Require(!GuestWritable_nid_no_patch(text, 1));
    cursor = text;
    rejects([&] { strsep_nid_postfix(&cursor, ","); });
    Require(cursor == text && std::strcmp(text, "ro,rw") == 0);
    errno = 71;
    Require(strsep_nid_postfix(&cursor, ":") == text && cursor == nullptr && errno == 71);

    char changing[] = "a:b,c";
    cursor = changing;
    Require(strsep_nid_postfix(&cursor, ":") == changing && cursor == changing + 2);
    Require(strsep_nid_postfix(&cursor, ",") == changing + 2 && cursor == changing + 4);
    Require(strsep_nid_postfix(&cursor, "") == changing + 4 && cursor == nullptr);
}

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
    CheckStrsepFaults();
}
