#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI scePsmlMfsrCreateContext();
int APS5_VABI scePsmlMfsrCreateContext800M3();
int APS5_VABI scePsmlMfsrCreateContextFrom2023July();
int APS5_VABI scePsmlMfsrGetContextBufferRequirement();
int APS5_VABI scePsmlMfsrGetContextBufferRequirement800M3();
int APS5_VABI scePsmlMfsrGetContextBufferRequirementFrom2023July();
int APS5_VABI scePsmlMfsrGetDispatchMfsrPacket();
int APS5_VABI scePsmlMfsrGetDispatchMfsrPacketFrom2023July();

}
