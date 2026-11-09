#include "prx/libScePad/include/InputCommandFile.hpp"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>

void Check(bool condition) {
    if (!condition) throw std::runtime_error("input command assertion failed");
}

int main() {
    const auto path = std::filesystem::temp_directory_path() /
        ("anyps5-pad-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto write = [&](const std::string& text) { std::ofstream(path) << text; };
    try {
        write("1 16384 300\n");
        Pad::InputCommandFile input(path);
        Check(input.Buttons(0) == 0);
        write("APS5_PAD_TIMELINE 1 2 3\n200 100 18432\n500 200 16\n800 100 0\n");
        Check(input.Buttons(100000) == 0);
        Check(input.Buttons(299999) == 0);
        Check(input.Buttons(300000) == 18432);
        Check(input.Buttons(399999) == 18432);
        Check(input.Buttons(400000) == 0);
        Check(input.Buttons(600000) == 16);
        Check(input.Buttons(800000) == 0);
        Check(input.Buttons(1100000) == 0);
        write("APS5_PAD_TIMELINE 1 3 2\n0 500 32768\n300 100 16\n");
        Check(input.Buttons(1200000) == 0);
        write("APS5_PAD_TIMELINE 1 3 2\n0 500 32768\n600 100 16\n");
        Check(input.Buttons(1300000) == 32768);
        write("APS5_PAD_TIMELINE 1 4 1\n0 6000 16\n");
        Check(input.Buttons(1400000) == 32768);
        write("APS5_PAD_TIMELINE 1 4 1\n0 500 16\nextra\n");
        Check(input.Buttons(1500000) == 32768);
        write("APS5_PAD_TIMELINE 1 4 2\n0 100 16\n");
        Check(input.Buttons(1600000) == 32768);
        write("APS5_PAD_TIMELINE 2 4 1\n0 100 16\n");
        Check(input.Buttons(1700000) == 32768);
        Check(input.Buttons(1800000) == 0);
        write("4 8 200\n");
        Check(input.Buttons(1900000) == 8);
        Check(input.Buttons(2100000) == 0);
        write("3 16 200\n");
        Check(input.Buttons(2200000) == 0);
        write("5 0 1\n");
        Check(input.Buttons(2300000) == 0);
        const std::string bad[] = {
            "APS5_PAD_TIMELINE 1 6 0\n", "APS5_PAD_TIMELINE 1 6 4097\n",
            "APS5_PAD_TIMELINE 1 6 1\n0 1 1\n", "APS5_PAD_TIMELINE 1 6 1\n-1 1 16\n",
            "APS5_PAD_TIMELINE 1 6 1\n3600000 1 16\n",
            "APS5_PAD_TIMELINE 1 18446744073709551616 1\n0 1 16\n",
            "APS5_PAD_TIMELINE 1 6 1\n18446744073709551615 1 16\n",
            "APS5_PAD_TIMELINE 1 6 1\n0 1 4294967296\n"
        };
        std::uint64_t clock = 2400000;
        for (const auto& text : bad) {
            write(text);
            Check(input.Buttons(clock) == 0);
            clock += 100000;
        }
        write("APS5_PAD_TIMELINE 1 6 1\n3599999 1 16384\n");
        Check(input.Buttons(clock) == 0);
        Check(input.Buttons(clock + 3599999000ull) == 16384);
        Check(input.Buttons(clock + 3600000000ull) == 0);
        write("APS5_PAD_TIMELINE 1 7 1\n0 100 16\n");
        const auto maximum = std::numeric_limits<std::uint64_t>::max();
        Check(input.Buttons(maximum - 10) == 16);
        Check(input.Buttons(maximum) == 16);
        Pad::InputCommandFile stale(path);
        Check(stale.Buttons(0) == 0);
        Pad::InputCommandFile disabled(std::filesystem::path{});
        Check(disabled.Buttons(0) == 0);
        write("10 16384 300 0 255 128 64\n");
        Pad::InputCommandFile sticks(path);
        Check(sticks.Poll(0) == Pad::InputCommandFile::Input{});
        write("11 16 200 0 255 128 64\n");
        Check(sticks.Poll(100000) == Pad::InputCommandFile::Input{16, true, {0, 255, 128, 64}});
        write("12 16 200 0 256 128 64\n");
        Check(sticks.Poll(200000) == Pad::InputCommandFile::Input{16, true, {0, 255, 128, 64}});
        Check(sticks.Poll(300000) == Pad::InputCommandFile::Input{});
        write("12 16 200 0 255 128 64 1\n");
        Check(sticks.Poll(400000) == Pad::InputCommandFile::Input{});
        write("APS5_PAD_TIMELINE 1 12 1\n0 500 16384\n");
        Check(sticks.Poll(500000) == Pad::InputCommandFile::Input{16384});
        write("13 8 200 255 0 64 128\n");
        Check(sticks.Poll(600000) == Pad::InputCommandFile::Input{8, true, {255, 0, 64, 128}});
        write("14 16 100\n");
        Check(sticks.Poll(700000) == Pad::InputCommandFile::Input{16});
        Check(sticks.Poll(800000) == Pad::InputCommandFile::Input{});
        write("15 2113536 100 0 255 128 64\n");
        Check(sticks.Poll(900000) == Pad::InputCommandFile::Input{16384, true, {0, 255, 128, 64}, true, false});
        Check(sticks.Poll(1000000) == Pad::InputCommandFile::Input{});
        write("APS5_PAD_TIMELINE 1 16 3\n0 100 2113536\n100 100 4194304\n200 100 6291456\n");
        Check(sticks.Poll(1100000) == Pad::InputCommandFile::Input{16384, false, {}, true, false});
        Check(sticks.Poll(1200000) == Pad::InputCommandFile::Input{0, false, {}, false, true});
        Check(sticks.Poll(1300000) == Pad::InputCommandFile::Input{0, false, {}, true, true});
        Check(sticks.Poll(1400000) == Pad::InputCommandFile::Input{});
        write("APS5_PAD_TIMELINE 1 17 1\n0 100 8388608\n");
        Check(sticks.Poll(1500000) == Pad::InputCommandFile::Input{});
        std::filesystem::remove(path);
        std::cout << "Controller timeline runtime cases passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::filesystem::remove(path);
        std::cerr << error.what() << '\n';
        return 1;
    }
}
