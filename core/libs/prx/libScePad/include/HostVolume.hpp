#ifndef CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTVOLUME_HPP
#define CORE_LIBS_PRX_LIBSCEPAD_INCLUDE_HOSTVOLUME_HPP

#include <cstdint>
#include <stdexcept>

class HostVolume {
public:
    static constexpr int MaxVolume = 100;
    static constexpr int VolumeStep = 5;
    static constexpr std::uint32_t UnityGain = 65536;

    int Volume() const { return volume; }
    bool Muted() const { return muted; }
    bool MuteOnFocusLoss() const { return muteOnFocusLoss; }
    bool FocusMuted() const { return muteOnFocusLoss && !focused; }

    bool Silent() const { return muted || FocusMuted(); }

    std::uint32_t Gain() const {
        if (Silent()) return 0;
        const auto level = static_cast<std::uint32_t>(volume);
        return (level * level * UnityGain + (MaxVolume * MaxVolume) / 2) / (MaxVolume * MaxVolume);
    }

    bool SetMuteOnFocusLoss(bool enabled) {
        const bool before = FocusMuted();
        muteOnFocusLoss = enabled;
        return before != FocusMuted();
    }

    bool FocusChanged(bool hasFocus) {
        const bool before = FocusMuted();
        focused = hasFocus;
        return before != FocusMuted();
    }

    bool SetVolume(int value) {
        if (value < 0 || value > MaxVolume) throw std::invalid_argument("HostVolume: volume out of range");
        const bool changed = value != volume;
        volume = value;
        return changed;
    }

    bool ToggleMute() {
        muted = !muted;
        return true;
    }

    bool VolumeUp() {
        const bool wasMuted = muted;
        muted = false;
        if (volume >= MaxVolume) return wasMuted;
        volume = volume + VolumeStep > MaxVolume ? MaxVolume : volume + VolumeStep;
        return true;
    }

    bool VolumeDown() {
        const bool wasMuted = muted;
        muted = false;
        if (volume <= 0) return wasMuted;
        volume = volume - VolumeStep < 0 ? 0 : volume - VolumeStep;
        return true;
    }

private:
    int volume = MaxVolume;
    bool muted = false;
    bool focused = true;
    bool muteOnFocusLoss = false;
};

#endif
