#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI sceVoiceCommandGetKeywordParameter();
int APS5_VABI sceVoiceCommandGetKeywordLanguage();
int APS5_VABI sceVoiceCommandInit();
int APS5_VABI sceVoiceCommandSetInputID();
int APS5_VABI sceVoiceCommandGetSentenceInfo();
int APS5_VABI sceVoiceCommandSetUnmuteSentence();
int APS5_VABI sceVoiceCommandSetUndefinedKeywordGroup();
int APS5_VABI sceVoiceCommandStopRecognize();
int APS5_VABI sceVoiceCommandSetKeywordParameter();
int APS5_VABI sceVoiceCommandClearUndefinedSentence();
int APS5_VABI sceVoiceCommandSetMuteSentence();
int APS5_VABI sceVoiceCommandProcess();
int APS5_VABI sceVoiceCommandBindSubDictionary();
int APS5_VABI sceVoiceCommandClearUndefinedKeywordGroup();
int APS5_VABI sceVoiceCommandGetKeywordGroupInfo();
int APS5_VABI sceVoiceCommandGetKeywordPortNum();
int APS5_VABI sceVoiceCommandSetUndefinedSentence();
int APS5_VABI sceVoiceCommandCreateMainDictionary();
int APS5_VABI sceVoiceCommandClosePort();
int APS5_VABI sceVoiceCommandStartRecognize();
int APS5_VABI sceVoiceCommandGetResultDetail();
int APS5_VABI sceVoiceCommandOpenMainDictionary();
int APS5_VABI sceVoiceCommandOpenMainDictionary_internal();

}
