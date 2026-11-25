# libSceAvPlayer

## change [sceAvPlayerSetTrickSpeed]

- [sceAvPlayerSetTrickSpeed](../../../core/libs/prx/libSceAvPlayer/src/Player.cpp) (libSceAvPlayer) - requires a source first; rejects speeds -399..399 except 100 and source types other than MP4-family (1) and WebM (2) with `NOT_SUPPORTED`; clamps accepted speeds to +/-3200. This follows Prosper's trace of the native module; the SDK 2.x state-4 special case for speed 100 is not modelled. The local decoder runs the clock backwards for negative speed but delivers no frames; returning to forward speed resumes from the rewound time. Closes when: trick-play frame delivery is implemented and measured on a PS5.