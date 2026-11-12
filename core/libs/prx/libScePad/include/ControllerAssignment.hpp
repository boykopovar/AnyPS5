#ifndef CORE_LIBS_PRX_LIBSCEPAD_CONTROLLERASSIGNMENT_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_CONTROLLERASSIGNMENT_HPP

#include <array>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>

namespace Pad {

inline constexpr std::size_t ControllerAssignmentSlots = 4;
using ControllerSerials = std::array<std::string, ControllerAssignmentSlots>;

class ControllerAssignment {
public:
    explicit ControllerAssignment(ControllerSerials configured = {}) : configured(std::move(configured)) {
        for (std::size_t slot = 0; slot < this->configured.size(); ++slot) {
            if (this->configured[slot].empty()) continue;
            for (std::size_t earlier = 0; earlier < slot; ++earlier) {
                if (this->configured[slot] == this->configured[earlier]) {
                    throw std::runtime_error("Pad: the same controller serial is assigned to multiple player slots");
                }
            }
        }
    }

    static ControllerAssignment FromEnvironment() {
        ControllerSerials serials;
        for (std::size_t slot = 0; slot < serials.size(); ++slot) {
            const std::string name = "ANYPS5_PAD_PLAYER_" + std::to_string(slot + 1) + "_SERIAL";
            if (const char* value = std::getenv(name.c_str()); value != nullptr) serials[slot] = value;
        }
        return ControllerAssignment(std::move(serials));
    }

    int SelectSlot(std::string_view serial, const std::array<bool, ControllerAssignmentSlots>& occupied) const {
        if (!serial.empty()) {
            for (std::size_t slot = 0; slot < configured.size(); ++slot) {
                if (configured[slot] != serial) continue;
                return occupied[slot] || AlreadyConnected(serial) ? -1 : static_cast<int>(slot);
            }
            if (AlreadyConnected(serial)) return -1;
        }
        for (std::size_t slot = 0; slot < configured.size(); ++slot) {
            if (!occupied[slot] && configured[slot].empty()) return static_cast<int>(slot);
        }
        return -1;
    }

    void Connect(int slot, std::string_view serial) {
        if (slot < 0 || static_cast<std::size_t>(slot) >= connected.size()) throw std::out_of_range("Pad: controller slot is out of range");
        connected[static_cast<std::size_t>(slot)] = serial;
    }

    void Disconnect(int slot) {
        if (slot < 0 || static_cast<std::size_t>(slot) >= connected.size()) return;
        connected[static_cast<std::size_t>(slot)].clear();
    }

private:
    bool AlreadyConnected(std::string_view serial) const {
        for (const auto& current : connected) {
            if (!current.empty() && current == serial) return true;
        }
        return false;
    }

    ControllerSerials configured;
    ControllerSerials connected;
};

}

#endif
