﻿#include "NODES_FILES_DIR.h"
#include "Nodes/node.hpp"
#include "Nodes/node_declare.hpp"
#include "Nodes/node_register.h"
#include "RCore/Backend.hpp"
#include "Utils/Math/math.h"
#include "nvrhi/utils.h"
#include "render_node_base.h"
#include "resource_allocator_instance.hpp"
#include "shaders/utils/cpp_shader_macro.h"
#include "utils/compile_shader.h"

namespace USTC_CG::node_render_scatter_contribution {
static void node_declare(NodeDeclarationBuilder& b)
{
    b.add_input<decl::Buffer>("PixelTarget");
    b.add_input<decl::Buffer>("Eval");
    b.add_input<decl::Texture>("Source Texture");

    b.add_output<decl::Texture>("Result Texture");
}

static void node_exec(ExeParams params)
{
    using namespace nvrhi;

    auto pixel_target_buffer = params.get_input<BufferHandle>("PixelTarget");
    auto eval_buffer = params.get_input<BufferHandle>("Eval");
    auto source_texture = params.get_input<TextureHandle>("Source Texture");

    auto length =
        pixel_target_buffer->getDesc().byteSize / sizeof(pxr::GfVec2i);

    std::string error_string;
    nvrhi::BindingLayoutDescVector binding_layout_desc;
    auto compute_shader = compile_shader(
        "main",
        ShaderType::Compute,
        "shaders/scatter.slang",
        binding_layout_desc,
        error_string);

    auto binding_layout = resource_allocator.create(binding_layout_desc[0]);
    MARK_DESTROY_NVRHI_RESOURCE(binding_layout);

    // BindingSet and BindingSetLayout
    BindingSetDesc binding_set_desc;
    binding_set_desc.bindings = {
        nvrhi::BindingSetItem::TypedBuffer_SRV(0, eval_buffer),
        nvrhi::BindingSetItem::TypedBuffer_SRV(1, pixel_target_buffer),
        nvrhi::BindingSetItem::Texture_UAV(2, source_texture)
    };

    auto binding_set =
        resource_allocator.create(binding_set_desc, binding_layout.Get());
    MARK_DESTROY_NVRHI_RESOURCE(binding_set);
    if (!binding_set) {
        // Handle error
        return;
    }
    auto bindingLayout = resource_allocator.create(binding_layout_desc[0]);
    MARK_DESTROY_NVRHI_RESOURCE(bindingLayout);
    ;
    // Execute the shader
    ComputePipelineDesc pipeline_desc;
    pipeline_desc.CS = compute_shader;
    pipeline_desc.bindingLayouts = { bindingLayout };
    auto pipeline = resource_allocator.create(pipeline_desc);
    if (!pipeline) {
        // Handle error
        return;
    }

    ComputeState compute_state;
    compute_state.pipeline = pipeline;
    compute_state.bindings = { binding_set };

    CommandListHandle command_list =
        resource_allocator.create(CommandListDesc{});
    command_list->open();
    command_list->setComputeState(compute_state);
    command_list->dispatch(div_ceil(length, 64), 1, 1);
    command_list->close();
    resource_allocator.device->executeCommandList(command_list);

    params.set_output("Result Texture", source_texture);
}

static void node_register()
{
    static NodeTypeInfo ntype;

    strcpy(ntype.ui_name, "render_scatter_contribution");
    strcpy(ntype.id_name, "node_render_scatter_contribution");

    render_node_type_base(&ntype);
    ntype.node_execute = node_exec;
    ntype.declare = node_declare;
    nodeRegisterType(&ntype);
}

NOD_REGISTER_NODE(node_register)
}  // namespace USTC_CG::node_render_scatter_contribution
