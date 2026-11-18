# libSceNKWebKit

## add Functional

- [Inspector frontend dispatcher events](../../../core/libs/prx/libSceNKWebKit/src/InspectorFrontendEvents.cpp) (libSceNKWebKit) - not compared with a console module. The 54 event messages, their enum strings and their JSON text follow the 11.x [protocol](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/protocol) through the [frontend dispatcher generator](https://github.com/neonmodder123/PS5-WebKit-11.x/blob/80f77e94c63ee7202eec3d4b5b1a021dd89011ba/Source/JavaScriptCore/inspector/scripts/codegen/generate_cpp_frontend_dispatcher_implementation.py) and the 11.x JSON writer. An enum argument outside its protocol values throws `std::out_of_range` where WebKit reads past its constant table.

## remove | `_ZN9Inspector21CSSFrontendDispatcher17styleSheetChangedERKN3WTF6StringE` | `ki3Vuu7oPhg` |

## remove | `_ZN9Inspector21CSSFrontendDispatcher17styleSheetRemovedERKN3WTF6StringE` | `OxDS41BqXws` |

## remove | `_ZN9Inspector21CSSFrontendDispatcher23mediaQueryResultChangedEv` | `PQ3n7JgVcUw` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher15documentUpdatedEv` | `jwsSD7ZhRvw` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher16attributeRemovedEiRKN3WTF6StringE` | `xzkqkH5qWbc` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher16childNodeRemovedEii` | `v+mo-+xOSx8` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher16shadowRootPoppedEii` | `73OOfmlKsoY` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher17attributeModifiedEiRKN3WTF6StringES4_` | `HRpZp6CCUcM` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher19didAddEventListenerEi` | `BWgXtAe6mPM` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher20pseudoElementRemovedEii` | `tMyRfRbu-90` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher21characterDataModifiedEiRKN3WTF6StringE` | `NMTIQpo3uXs` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher21childNodeCountUpdatedEii` | `vw0sBeeULiY` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher23willRemoveEventListenerEi` | `WqzBma32q5c` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher25customElementStateChangedEiNS_8Protocol3DOM18CustomElementStateE` | `GYqyvAJtRdw` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher34powerEfficientPlaybackStateChangedEidb` | `B-fuSvib9CA` |

## remove | `_ZN9Inspector21DOMFrontendDispatcher7inspectEi` | `JcUZCNbdFGA` |

## remove | `_ZN9Inspector22HeapFrontendDispatcher16trackingCompleteEdRKN3WTF6StringE` | `kD4ppqGHUsI` |

## remove | `_ZN9Inspector22PageFrontendDispatcher13frameDetachedERKN3WTF6StringE` | `cOEmyDiWGOM` |

## remove | `_ZN9Inspector22PageFrontendDispatcher14loadEventFiredEd` | `O7LqfJ24JqM` |

## remove | `_ZN9Inspector22PageFrontendDispatcher19frameStartedLoadingERKN3WTF6StringE` | `RWWT2HmVAGQ` |

## remove | `_ZN9Inspector22PageFrontendDispatcher19frameStoppedLoadingERKN3WTF6StringE` | `SNxP9g4ifrE` |

## remove | `_ZN9Inspector22PageFrontendDispatcher20domContentEventFiredEd` | `hk2Vi4aJODY` |

## remove | `_ZN9Inspector22PageFrontendDispatcher24frameScheduledNavigationERKN3WTF6StringEd` | `e50w9CgqAVM` |

## remove | `_ZN9Inspector24CanvasFrontendDispatcher13canvasRemovedERKN3WTF6StringE` | `8A0knFNsfrQ` |

## remove | `_ZN9Inspector24CanvasFrontendDispatcher16extensionEnabledERKN3WTF6StringES4_` | `mHLewiXwbMo` |

## remove | `_ZN9Inspector24CanvasFrontendDispatcher16recordingStartedERKN3WTF6StringENS_8Protocol9Recording9InitiatorE` | `16iE1U5SFuk` |

## remove | `_ZN9Inspector24CanvasFrontendDispatcher18clientNodesChangedERKN3WTF6StringE` | `6Ah3qEzF5fg` |

## remove | `_ZN9Inspector24CanvasFrontendDispatcher19canvasMemoryChangedERKN3WTF6StringEd` | `XrTJtDbKnHg` |

## remove | `_ZN9Inspector24TargetFrontendDispatcher15targetDestroyedERKN3WTF6StringE` | `eVUG1wzRuAA` |

## remove | `_ZN9Inspector24TargetFrontendDispatcher25dispatchMessageFromTargetERKN3WTF6StringES4_` | `E6XSG7zCNk8` |

## remove | `_ZN9Inspector24TargetFrontendDispatcher26didCommitProvisionalTargetERKN3WTF6StringES4_` | `NWZbcznrH4E` |

## remove | `_ZN9Inspector24WorkerFrontendDispatcher13workerCreatedERKN3WTF6StringES4_S4_` | `Nk-XIvILlUQ` |

## remove | `_ZN9Inspector24WorkerFrontendDispatcher16workerTerminatedERKN3WTF6StringE` | `apl6KD3oTLg` |

## remove | `_ZN9Inspector24WorkerFrontendDispatcher25dispatchMessageFromWorkerERKN3WTF6StringES4_` | `4Mbb6GJcxos` |

## remove | `_ZN9Inspector25NetworkFrontendDispatcher12dataReceivedERKN3WTF6StringEdii` | `ogFCy9EBtYo` |

## remove | `_ZN9Inspector25NetworkFrontendDispatcher15webSocketClosedERKN3WTF6StringEd` | `NgTYaNYsSsk` |

## remove | `_ZN9Inspector25NetworkFrontendDispatcher16webSocketCreatedERKN3WTF6StringES4_` | `X9OuYB-jm2A` |

## remove | `_ZN9Inspector25NetworkFrontendDispatcher19webSocketFrameErrorERKN3WTF6StringEdS4_` | `RZsx7hJC89c` |

## remove | `_ZN9Inspector26DebuggerFrontendDispatcher19globalObjectClearedEv` | `To0+90ONW9w` |

## remove | `_ZN9Inspector26DebuggerFrontendDispatcher19scriptFailedToParseERKN3WTF6StringES4_iiS4_` | `a-YNP4O13AU` |

## remove | `_ZN9Inspector26DebuggerFrontendDispatcher25playBreakpointActionSoundEi` | `nz2Vn7gXKYE` |

## remove | `_ZN9Inspector26DebuggerFrontendDispatcher7resumedEv` | `CNQLeYqffyw` |

## remove | `_ZN9Inspector26TimelineFrontendDispatcher16recordingStartedEd` | `zptosxCqOgw` |

## remove | `_ZN9Inspector26TimelineFrontendDispatcher16recordingStoppedEd` | `MzZ1vXfWlaY` |

## remove | `_ZN9Inspector26TimelineFrontendDispatcher18autoCaptureStartedEv` | `gSjDXUEB5Uk` |

## remove | `_ZN9Inspector27AnimationFrontendDispatcher13targetChangedERKN3WTF6StringE` | `hd9m4Ozb-zU` |

## remove | `_ZN9Inspector27AnimationFrontendDispatcher13trackingStartEd` | `bb5o-YHu8vM` |

## remove | `_ZN9Inspector27AnimationFrontendDispatcher16trackingCompleteEd` | `RZHQT5QnVPQ` |

## remove | `_ZN9Inspector27AnimationFrontendDispatcher18animationDestroyedERKN3WTF6StringE` | `bMlLM2uzdB8` |

## remove | `_ZN9Inspector27InspectorFrontendDispatcher25evaluateForTestInFrontendERKN3WTF6StringE` | `jALEBkxiGfU` |

## remove | `_ZN9Inspector27LayerTreeFrontendDispatcher18layerTreeDidChangeEv` | `eS6QfGn5eCY` |

## remove | `_ZN9Inspector32ScriptProfilerFrontendDispatcher13trackingStartEd` | `wf6EuM9IZW0` |

## remove | `_ZN9Inspector34ApplicationCacheFrontendDispatcher19networkStateUpdatedEb` | `hMmahzTAYpo` |

## remove | `_ZN9Inspector34ApplicationCacheFrontendDispatcher29applicationCacheStatusUpdatedERKN3WTF6StringES4_i` | `WuHl28C+7k0` |
