#pragma once

#include "prx/libc/include/Api.hpp"
#include "SceTypes.hpp"

struct MinMaxFloat {
    float minimum;
    float maximum;
};

struct MinMaxDouble {
    double minimum;
    double maximum;
};

extern "C" {

float APS5_VABI __std_min_f_nid_postfix(const void* first, const void* last);
double APS5_VABI __std_min_d_nid_postfix(const void* first, const void* last);
float APS5_VABI __std_max_f_nid_postfix(const void* first, const void* last);
double APS5_VABI __std_max_d_nid_postfix(const void* first, const void* last);
MinMaxFloat APS5_VABI __std_minmax_f_nid_postfix(const void* first, const void* last);
MinMaxDouble APS5_VABI __std_minmax_d_nid_postfix(const void* first, const void* last);
const void* APS5_VABI __std_min_element_f_nid_postfix(const void* first, const void* last, bool unused);
const void* APS5_VABI __std_min_element_d_nid_postfix(const void* first, const void* last, bool unused);
const void* APS5_VABI __std_max_element_f_nid_postfix(const void* first, const void* last, bool unused);
const void* APS5_VABI __std_max_element_d_nid_postfix(const void* first, const void* last, bool unused);
MinMaxPointers APS5_VABI __std_minmax_element_f_nid_postfix(const void* first, const void* last, bool unused);
MinMaxPointers APS5_VABI __std_minmax_element_d_nid_postfix(const void* first, const void* last, bool unused);

}
