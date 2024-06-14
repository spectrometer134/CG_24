#include "NODES_FILES_DIR.h"
#include "Nodes/node.hpp"
#include "Nodes/node_declare.hpp"
#include "Nodes/node_register.h"
#include "RCore/Backend.hpp"
#include "nvrhi/utils.h"
#include "render_node_base.h"
#include "resource_allocator_instance.hpp"
#include "utils/cam_to_view_contants.h"

namespace USTC_CG::node_scene_ray_launch {
static void node_declare(NodeDeclarationBuilder& b)
{
    b.add_input<decl::AccelStruct>("Accel Struct");
    b.add_input<decl::Camera>("Camera");
    b.add_output<decl::Texture>("Barycentric");
}

static void node_exec(ExeParams params)
{
    Hd_USTC_CG_Camera* free_camera = get_free_camera(params);
    auto size = free_camera->_dataWindow.GetSize();

    // 0. Prepare the output texture
    nvrhi::TextureDesc output_desc;
    output_desc.width = size[0];
    output_desc.height = size[1];
    output_desc.format = nvrhi::Format::RGBA32_FLOAT;
    output_desc.initialState = nvrhi::ResourceStates::UnorderedAccess;
    output_desc.keepInitialState = true;
    output_desc.isUAV = true;

    auto result_texture = resource_allocator.create(output_desc);

    auto m_CommandList = resource_allocator.create(CommandListDesc{});

    // 2. Prepare the shader

    ShaderCompileDesc shader_compile_desc;
    shader_compile_desc.set_path(
        std::filesystem::path(RENDER_NODES_FILES_DIR) /
        std::filesystem::path("shaders/ray_launch.hlsl"));
    shader_compile_desc.shaderType = nvrhi::ShaderType::AllRayTracing;
    // shader_compile_desc.set_entry_name("ClosestHit");

    auto raytrace_compiled = resource_allocator.create(shader_compile_desc);

    if (raytrace_compiled->get_error_string().empty()) {
        ShaderDesc shader_desc;
        shader_desc.entryName = "RayGen";
        shader_desc.shaderType = nvrhi::ShaderType::RayGeneration;
        shader_desc.debugName = std::to_string(
            reinterpret_cast<long long>(raytrace_compiled->getBufferPointer()));
        auto raygen_shader = resource_allocator.create(
            shader_desc,
            raytrace_compiled->getBufferPointer(),
            raytrace_compiled->getBufferSize());

        shader_desc.entryName = "ClosestHit";
        shader_desc.shaderType = nvrhi::ShaderType::ClosestHit;
        auto chs_shader = resource_allocator.create(
            shader_desc,
            raytrace_compiled->getBufferPointer(),
            raytrace_compiled->getBufferSize());

        shader_desc.entryName = "Miss";
        shader_desc.shaderType = nvrhi::ShaderType::Miss;
        auto miss_shader = resource_allocator.create(
            shader_desc,
            raytrace_compiled->getBufferPointer(),
            raytrace_compiled->getBufferSize());

        // 3. Prepare the hitgroup and pipeline

        nvrhi::BindingLayoutDesc globalBindingLayoutDesc;
        globalBindingLayoutDesc.visibility = nvrhi::ShaderType::All;
        globalBindingLayoutDesc.bindings = {
            { 0, nvrhi::ResourceType::ConstantBuffer },
            { 0, nvrhi::ResourceType::RayTracingAccelStruct },
            { 0, nvrhi::ResourceType::Texture_UAV }
        };
        auto globalBindingLayout =
            resource_allocator.create(globalBindingLayoutDesc);

        nvrhi::rt::PipelineDesc pipeline_desc;
        pipeline_desc.maxPayloadSize = 4 * sizeof(float);
        pipeline_desc.globalBindingLayouts = { globalBindingLayout };
        pipeline_desc.shaders = { { "", raygen_shader, nullptr },
                                  { "", miss_shader, nullptr } };

        pipeline_desc.hitGroups = { {
            "HitGroup",
            chs_shader,
            nullptr,  // anyHitShader
            nullptr,  // intersectionShader
            nullptr,  // bindingLayout
            false     // isProceduralPrimitive
        } };
        auto m_TopLevelAS = params.get_input<AccelStructHandle>("Accel Struct");
        auto raytracing_pipeline = resource_allocator.create(pipeline_desc);
        BindingSetDesc binding_set_desc;

        auto constant_buffer = resource_allocator.create(
            BufferDesc{ .byteSize = sizeof(PlanarViewConstants),
                        .debugName = "constantBuffer",
                        .isConstantBuffer = true,
                        .initialState = nvrhi::ResourceStates::ConstantBuffer,
                        .cpuAccess = nvrhi::CpuAccessMode::Write });

        binding_set_desc.bindings = nvrhi::BindingSetItemArray{
            nvrhi::BindingSetItem::ConstantBuffer(0, constant_buffer.Get()),
            nvrhi::BindingSetItem::RayTracingAccelStruct(0, m_TopLevelAS.Get()),
            nvrhi::BindingSetItem::Texture_UAV(0, result_texture.Get())
        };
        auto binding_set = resource_allocator.create(
            binding_set_desc, globalBindingLayout.Get());

        nvrhi::rt::State state;
        nvrhi::rt::ShaderTableHandle sbt =
            raytracing_pipeline->createShaderTable();
        sbt->setRayGenerationShader("RayGen");
        sbt->addHitGroup("HitGroup");
        sbt->addMissShader("Miss");
        state.setShaderTable(sbt).addBindingSet(binding_set);

        m_CommandList->open();
        PlanarViewConstants view_constant =
            camera_to_view_constants(free_camera);
        m_CommandList->writeBuffer(
            constant_buffer.Get(), &view_constant, sizeof(PlanarViewConstants));

        m_CommandList->setRayTracingState(state);
        nvrhi::rt::DispatchRaysArguments args;
        args.width = size[0];
        args.height = size[1];
        m_CommandList->dispatchRays(args);
        m_CommandList->close();
        resource_allocator.device->executeCommandList(m_CommandList);
        resource_allocator.device
            ->waitForIdle();  // This is not fully efficient.

        resource_allocator.destroy(constant_buffer);
        resource_allocator.destroy(raytracing_pipeline);
        resource_allocator.destroy(globalBindingLayout);
        resource_allocator.destroy(binding_set);
        resource_allocator.destroy(raygen_shader);
        resource_allocator.destroy(chs_shader);
        resource_allocator.destroy(miss_shader);
    }

    resource_allocator.destroy(m_CommandList);
    auto error = raytrace_compiled->get_error_string();
    resource_allocator.destroy(raytrace_compiled);

    params.set_output("Barycentric", result_texture);
    if (error.size()) {
        throw std::runtime_error(error);
    }
}

static void node_register()
{
    static NodeTypeInfo ntype;

    strcpy(ntype.ui_name, "Ray Launch");
    strcpy(ntype.id_name, "render_ray_launch");

    render_node_type_base(&ntype);
    ntype.node_execute = node_exec;
    ntype.declare = node_declare;
    nodeRegisterType(&ntype);
}

NOD_REGISTER_NODE(node_register)
}  // namespace USTC_CG::node_scene_ray_launch
