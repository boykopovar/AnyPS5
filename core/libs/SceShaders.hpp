#ifndef CORE_LIBS_SCE_SHADERS_HPP
#define CORE_LIBS_SCE_SHADERS_HPP

#include <cstdint>

struct ShaderSharp {
    std::uint16_t offset_dw : 15;
    std::uint16_t size : 1;
};

struct ShaderRegister {
    std::uint32_t offset;
    std::uint32_t value;
};

struct ShaderUserData {
    std::uint16_t* direct_resource_offset;
    ShaderSharp* sharp_resource_offset[4];
    std::uint16_t eud_size_dw;
    std::uint16_t srt_size_dw;
    std::uint16_t direct_resource_count;
    std::uint16_t sharp_resource_count[4];
};

struct ShaderRegisterRange {
    std::uint16_t start;
    std::uint16_t end;
};

struct ShaderDrawModifier {
    std::uint32_t enbl_start_vertex_offset : 1;
    std::uint32_t enbl_start_index_offset : 1;
    std::uint32_t enbl_start_instance_offset : 1;
    std::uint32_t enbl_draw_index : 1;
    std::uint32_t enbl_user_vgprs : 1;
    std::uint32_t render_target_slice_offset : 3;
    std::uint32_t fuse_draws : 1;
    std::uint32_t compiler_flags : 23;
    std::uint32_t is_default : 1;
    std::uint32_t reserved : 31;
};

struct ShaderSpecialRegs {
    ShaderRegister ge_cntl;
    ShaderRegister vgt_shader_stages_en;
    std::uint32_t dispatch_modifier;
    ShaderRegisterRange user_data_range;
    ShaderDrawModifier draw_modifier;
    ShaderRegister vgt_gs_out_prim_type;
    ShaderRegister ge_user_vgpr_en;
};

struct ShaderSemantic {
    std::uint32_t semantic : 8;
    std::uint32_t hardware_mapping : 8;
    std::uint32_t size_in_elements : 4;
    std::uint32_t is_f16 : 2;
    std::uint32_t is_flat_shaded : 1;
    std::uint32_t is_linear : 1;
    std::uint32_t is_custom : 1;
    std::uint32_t static_vb_index : 1;
    std::uint32_t static_attribute : 1;
    std::uint32_t reserved : 1;
    std::uint32_t default_value : 2;
    std::uint32_t default_value_hi : 2;
};

struct Shader {
    std::uint32_t file_header;
    std::uint32_t version;
    ShaderUserData* user_data;
    const volatile void* code;
    ShaderRegister* cx_registers;
    ShaderRegister* sh_registers;
    ShaderSpecialRegs* specials;
    ShaderSemantic* input_semantics;
    ShaderSemantic* output_semantics;
    std::uint32_t header_size;
    std::uint32_t shader_size;
    std::uint32_t embedded_constant_buffer_size_dqw;
    std::uint32_t target;
    std::uint32_t num_input_semantics;
    std::uint16_t scratch_size_dw_per_thread;
    std::uint16_t num_output_semantics;
    std::uint16_t special_sizes_bytes;
    std::uint8_t type;
    std::uint8_t num_cx_registers;
    std::uint8_t num_sh_registers;
};

#endif
