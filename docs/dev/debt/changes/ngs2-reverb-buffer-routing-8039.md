# libSceNgs2.native

## change [NGS2 custom buffers]

- [NGS2 custom buffers](../../../core/libs/prx/libSceNgs2.native/src/Custom.cpp) - multiple buffers on samplers, built-in submixer channels above 8 and inputs beyond 0/1 throw. Multi-buffer submixers take only Gain, UserFx `0x19` and configured Reverb. UserFx handlers throw: the extra channel count and whether extra buffer 0 means none are unknown; PPSA33655 (#10271) needs both. Input routing, grain clearing and module order follow the [12.70 reading](https://github.com/boykopovar/AnyPS5/issues/8039#issuecomment-6391072574), items 4, 13, 15 and 16; port selection follows [KytyPS5](https://github.com/KytyPS5/KytyPS5/blob/83f8bfd3c9c35fcb135ef6c33dfd88bbfe066ee2/src/libs/ngs2.cpp#L417-L419). Voice-control reordering, invalid port-buffer errors and creation/query check order are unverified; queries throw where creation fails. No console comparison.

## change [NGS2 custom Reverb]

- [NGS2 custom Reverb](../../../core/libs/prx/libSceNgs2.native/src/Reverb.cpp) - NaN controls, custom mastering, multi-buffer voice transitions/paused processing and inactive out-of-place processing throw. Native low-rate/half-rate modes remain unverified. Initial/setup bypass is verified only in place. Configured routing follows the [12.70 reading](https://github.com/boykopovar/AnyPS5/issues/8039#issuecomment-6391072574), items 6 and 13: overwrite dest, preserve source, ignore extra. Numerical equivalence is not bit-exact: 3.20/13.60 comparisons at 48 kHz cover both sizes and a hot update; stereo differs by less than 2e-8 and strong HF attenuation by less than 8e-8. The 13.60 graph confirms initial/setup bypass, history cleared with parameters retained on fresh PLAY, and sampler completion ending the voice. Checks use host adapters and equivalent SSE4a extraction lowering for Rosetta, not console execution or a game unblock.

## add Unknown function info

- [NGS2 custom Reverb transition recovery](../../../core/libs/prx/libSceNgs2.native/src/Reverb.cpp) - rejected deferred PLAY+PAUSE can retain the PAUSED flag. Immediate STOP_IMM then PLAY remains refused; STOP_IMM, one silent render, then PLAY clears the flag and resets history. Native transition semantics remain unverified.
