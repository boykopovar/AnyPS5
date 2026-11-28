# libSceAjm.native

## add Unknown function info

- [Opus run without RUN_MULTIPLE_FRAMES](../../../core/libs/prx/libSceAjm.native/src/Ajm.cpp) (libSceAjm.native) - a run job from the PS5 builders decodes its whole input while the output has room for a frame and the instance does not resample, as Grand Theft Auto VI (PPSA38500) needs: it passes two to four packets per run with room for all of them and does not resubmit the rest, so stopping after one packet played its music at two to four times its speed. A job read from a PS4 batch buffer, or one on a resampling instance (whose tests pin one packet per job), stops after one packet. Whether a PS5 stops after one packet is not measured.
