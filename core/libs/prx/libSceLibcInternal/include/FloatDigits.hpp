#pragma once

#include "SceTypes.hpp"
#include <cstdint>

extern "C" {

int APS5_VABI _Stoflt_nid_postfix(const char* start, const char* text, char** end, std::int64_t* words, int maxWords);
int APS5_VABI _Stoxflt_nid_postfix(const char* start, const char* text, char** end, std::int64_t* words, int maxWords);
int APS5_VABI _WStoflt_nid_postfix(const char16_t* start, const char16_t* text, char16_t** end, std::int64_t* words, int maxWords);
int APS5_VABI _WStoxflt_nid_postfix(const char16_t* start, const char16_t* text, char16_t** end, std::int64_t* words, int maxWords);
double APS5_VABI _WStodx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error);
float APS5_VABI _WStofx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error);
long double APS5_VABI _WStoldx_nid_postfix(const char16_t* text, char16_t** end, std::int64_t pten, int* error);

}
