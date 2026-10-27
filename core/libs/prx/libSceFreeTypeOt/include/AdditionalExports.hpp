#pragma once

#include "SceTypes.hpp"

extern "C" {

int APS5_VABI ft_service_list_lookup_nid_postfix();
int APS5_VABI ft_stub_set_char_sizes_nid_postfix();
int APS5_VABI ft_stub_set_pixel_sizes_nid_postfix();
int APS5_VABI ft_synthesize_vertical_metrics_nid_postfix();
int APS5_VABI ft_validator_error_nid_postfix();
int APS5_VABI ft_validator_init_nid_postfix();
int APS5_VABI ft_validator_run_nid_postfix();

}
