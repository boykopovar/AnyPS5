# Tested compatibility

| Game           | ID        | Windows           | Linux | GTX 1050 Ti / i5-7500 3.4GHz | Intel(R) HD Graphic 620 / i5-7200 2.5GHz |
|----------------|-----------|-------------------|-------|------------------------------|------------------------------------------|
| Dreaming Sarah | PPSA02929 | In game, playable | ?     | 60 FPS                       | 36 FPS                                   |
| DOOM Shareware (homebrew) | PPSA99666 | In game (E1M1), pending dependencies | ? | ? | ? |

DOOM version 01.001.000 was tested on Windows 11 with an RTX 4070 in an integration build containing [#553](https://github.com/boykopovar/AnyPS5/pull/553), [#683](https://github.com/boykopovar/AnyPS5/pull/683), [#684](https://github.com/boykopovar/AnyPS5/pull/684) and [#685](https://github.com/boykopovar/AnyPS5/pull/685); these changes are not yet merged into `main`. Movement and shooting were checked in E1M1; [in-game screenshots](https://github.com/boykopovar/AnyPS5/pull/685#issuecomment-5997547697) are attached to #685. Linux, audible audio and extended gameplay were not verified.

AGC compute presentation was verified with the compiled shader cache. A cold start during concurrent GPU tests triggered the homebrew's CPU scaler fallback, and a nonterminating UCRT invalid-parameter diagnostic was observed at startup.
