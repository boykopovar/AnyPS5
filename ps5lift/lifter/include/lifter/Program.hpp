#ifndef PS5LIFT_PROGRAM_HPP
#define PS5LIFT_PROGRAM_HPP

#include <lifter/GuestImage.hpp>

#include <cstddef>
#include <string>
#include <vector>

namespace Ps5Lift {

struct GuestProgram {
    std::vector<GuestImage> Images;
    std::vector<std::size_t> Initialization;

    static GuestProgram Load(const std::string& executable);
    std::string LoaderName(std::size_t index) const;
};

}

#endif
