#include "inexor/vulkan-renderer/cubemap/cubemap_generator.hpp"

#include "inexor/vulkan-renderer/cubemap/gpu_cubemap.hpp"
#include "inexor/vulkan-renderer/exception.hpp"
#include "inexor/vulkan-renderer/wrapper/descriptor_builder.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"
#include "inexor/vulkan-renderer/wrapper/offscreen_framebuffer.hpp"
#include "inexor/vulkan-renderer/wrapper/once_command_buffer.hpp"
#include "inexor/vulkan-renderer/wrapper/shader.hpp"

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/mat4x4.hpp>
#include <spdlog/spdlog.h>

#define _USE_MATH_DEFINES
#include <cmath>

// TODO: This must be removed before code review!!
// I am not saying C++20 would resolve this issue but C++20 would resolve this issue
#ifndef M_PI
#define M_PI 3.1415926535897932
#endif

#include <vector>

namespace inexor::vulkan_renderer::cubemap {

// TODO: Separate into class methods!
CubemapGenerator::CubemapGenerator(const wrapper::Device &device) {

    enum CubemapTarget { IRRADIANCE = 0, PREFILTEREDENV = 1 };

    for (std::uint32_t target = 0; target < PREFILTEREDENV + 1; target++) {

        VkFormat format;
        std::uint32_t dim;

        switch (target) {
        case IRRADIANCE:
            format = VK_FORMAT_R32G32B32A32_SFLOAT;
            dim = 64;
            break;
        case PREFILTEREDENV:
            format = VK_FORMAT_R16G16B16A16_SFLOAT;
            dim = 512;
            break;
        };

        const VkExtent2D image_extent{dim, dim};

        const std::uint32_t miplevel_count = static_cast<std::uint32_t>(floor(log2(dim))) + 1;

        m_cubemap_texture = std::make_unique<cubemap::GpuCubemap>(device, format, dim, dim, miplevel_count, "cubemap");

        std::vector<VkAttachmentDescription> att_desc(1);
        att_desc[0].format = format;
        att_desc[0].samples = VK_SAMPLE_COUNT_1_BIT;
        att_desc[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        att_desc[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        att_desc[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        att_desc[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        att_desc[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        att_desc[0].finalLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkAttachmentReference color_ref;
        color_ref.attachment = 0;
        color_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        std::vector<VkSubpassDescription> subpass_desc(1);
        subpass_desc[0].pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass_desc[0].colorAttachmentCount = 1;
        subpass_desc[0].pColorAttachments = &color_ref;

        std::vector<VkSubpassDependency> deps(2);
        deps[0].srcSubpass = VK_SUBPASS_EXTERNAL;
        deps[0].dstSubpass = 0;
        deps[0].srcStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[0].dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[0].srcAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        deps[0].dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[0].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        deps[1].srcSubpass = 0;
        deps[1].dstSubpass = VK_SUBPASS_EXTERNAL;
        deps[1].srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        deps[1].dstStageMask = VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        deps[1].srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        deps[1].dstAccessMask = VK_ACCESS_MEMORY_READ_BIT;
        deps[1].dependencyFlags = VK_DEPENDENCY_BY_REGION_BIT;

        const VkRenderPassCreateInfo renderpass_ci = wrapper::make_info(att_desc, subpass_desc, deps);

        VkRenderPass renderpass;

        if (const auto result = vkCreateRenderPass(device.device(), &renderpass_ci, nullptr, &renderpass);
            result != VK_SUCCESS) {
            throw VulkanException("Failed to create renderpass for cubemap generation (vkCreateRenderPass)!", result);
        }

        m_offscreen_framebuffer = std::make_unique<wrapper::OffscreenFramebuffer>(
            device, format, image_extent.width, image_extent.height, renderpass, "offscreen framebuffer");

        const std::vector<VkDescriptorPoolSize> pool_sizes{{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}};

        m_descriptor_pool = std::make_unique<wrapper::DescriptorPool>(device, pool_sizes, "cubemap generator");

        wrapper::DescriptorBuilder builder(device, m_descriptor_pool->descriptor_pool());

        m_descriptor = builder.add_combined_image_sampler(*m_cubemap_texture).build("cubemap generator");

        // TODO: Move this code block to ...?
        struct PushBlockIrradiance {
            glm::mat4 mvp;
            // TODO: Use static_cast here!
            float deltaPhi = (2.0f * float(M_PI)) / 180.0f;
            float deltaTheta = (0.5f * float(M_PI)) / 64.0f;
        } pushBlockIrradiance;

        // TODO: Move this code block to ...?
        struct PushBlockPrefilterEnv {
            glm::mat4 mvp;
            float roughness;
            uint32_t numSamples = 32u;
        } pushBlockPrefilterEnv;

        std::vector<VkPushConstantRange> push_constant_range(1);
        push_constant_range[0].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;

        switch (target) {
        case IRRADIANCE:
            push_constant_range[0].size = sizeof(PushBlockIrradiance);
            break;
        case PREFILTEREDENV:
            push_constant_range[0].size = sizeof(PushBlockPrefilterEnv);
            break;
        };

        const std::vector<VkDescriptorSetLayout> desc_set_layouts{m_descriptor->descriptor_set_layout()};

        const VkPipelineLayoutCreateInfo pipeline_layout_ci = wrapper::make_info(desc_set_layouts, push_constant_range);

        VkPipelineLayout pipeline_layout;

        if (const auto result = vkCreatePipelineLayout(device.device(), &pipeline_layout_ci, nullptr, &pipeline_layout);
            result != VK_SUCCESS) {
            throw VulkanException("Failed to create pipeline layout (vkCreatePipelineLayout)!", result);
        }

        const auto input_assembly_sci = wrapper::make_info<VkPipelineInputAssemblyStateCreateInfo>();

        const auto rast_sci = wrapper::make_info<VkPipelineRasterizationStateCreateInfo>();

        VkPipelineColorBlendAttachmentState blend_att_state{};
        blend_att_state.colorWriteMask =
            VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        blend_att_state.blendEnable = VK_FALSE;

        // TODO: Move this into make_info wrapper function?
        auto color_blend_sci = wrapper::make_info<VkPipelineColorBlendStateCreateInfo>();
        color_blend_sci.attachmentCount = 1;
        color_blend_sci.pAttachments = &blend_att_state;

        auto depth_stencil_sci = wrapper::make_info<VkPipelineDepthStencilStateCreateInfo>();
        depth_stencil_sci.depthTestEnable = VK_FALSE;
        depth_stencil_sci.depthWriteEnable = VK_FALSE;
        depth_stencil_sci.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        depth_stencil_sci.front = depth_stencil_sci.back;
        depth_stencil_sci.back.compareOp = VK_COMPARE_OP_ALWAYS;

        // TODO: Turn all this into a builder pattern?
        auto viewport_sci = wrapper::make_info<VkPipelineViewportStateCreateInfo>();
        viewport_sci.viewportCount = 1;
        viewport_sci.scissorCount = 1;

        auto multisample_sci = wrapper::make_info<VkPipelineMultisampleStateCreateInfo>();
        multisample_sci.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        const std::vector<VkDynamicState> dynamic_states = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

        const auto dynamic_state_ci = wrapper::make_info(dynamic_states);

        // TODO: Move this code block to ...?
        struct CubemapVertex {
            glm::vec3 pos;
            glm::vec3 normal;
            glm::vec2 uv0;
            glm::vec2 uv1;
            glm::vec4 joint0;
            glm::vec4 weight0;
        };

        const std::vector<VkVertexInputBindingDescription> vertex_input_binding{
            {0, sizeof(CubemapVertex), VK_VERTEX_INPUT_RATE_VERTEX}};

        const std::vector<VkVertexInputAttributeDescription> vertex_input_attributes{
            {0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};

        const VkPipelineVertexInputStateCreateInfo vertex_input_state_ci =
            wrapper::make_info(vertex_input_binding, vertex_input_attributes);

        std::vector<VkPipelineShaderStageCreateInfo> shader_stages(2);

        // TODO: Use ShaderLoader here as well?
        wrapper::Shader filtercube(device, VK_SHADER_STAGE_VERTEX_BIT, "shaders/cubemap/filtercube.vert.spv",
                                   "filtercube");

        wrapper::Shader irradiancecube(device, VK_SHADER_STAGE_FRAGMENT_BIT, "shaders/cubemap/irradiancecube.frag.spv",
                                       "irradiancecube");

        wrapper::Shader prefilterenvmap(device, VK_SHADER_STAGE_FRAGMENT_BIT,
                                        "shaders/cubemap/prefilterenvmap.frag.spv", "prefilterenvmap");

        shader_stages[0] = wrapper::make_info(filtercube);

        switch (target) {
        case IRRADIANCE:
            shader_stages[1] = wrapper::make_info(irradiancecube);
            break;
        case PREFILTEREDENV:
            shader_stages[1] = wrapper::make_info(prefilterenvmap);
            break;
        };

        const auto pipeline_ci = wrapper::make_info(pipeline_layout, renderpass, shader_stages, &vertex_input_state_ci,
                                                    &input_assembly_sci, &viewport_sci, &rast_sci, &multisample_sci,
                                                    &depth_stencil_sci, &color_blend_sci, &dynamic_state_ci);

        VkPipeline pipeline;
        if (const auto result =
                vkCreateGraphicsPipelines(device.device(), nullptr, 1, &pipeline_ci, nullptr, &pipeline);
            result != VK_SUCCESS) {
            throw VulkanException("Failed to create graphics pipeline (vkCreateGraphicsPipelines)!", result);
        }

        VkClearValue clearValues[1];
        clearValues[0].color = {{0.0f, 0.0f, 0.2f, 0.0f}};

        auto renderpass_bi = wrapper::make_info<VkRenderPassBeginInfo>();
        renderpass_bi.renderPass = renderpass;
        renderpass_bi.framebuffer = m_offscreen_framebuffer->framebuffer();
        renderpass_bi.renderArea.extent.width = dim;
        renderpass_bi.renderArea.extent.height = dim;
        renderpass_bi.clearValueCount = 1;
        renderpass_bi.pClearValues = clearValues;

        const std::vector<glm::mat4> matrices = {
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(0.0f, 1.0f, 0.0f)),
                        glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(-90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(1.0f, 0.0f, 0.0f)),
            glm::rotate(glm::mat4(1.0f), glm::radians(180.0f), glm::vec3(0.0f, 0.0f, 1.0f)),
        };

        VkViewport viewport{};
        viewport.width = static_cast<float>(dim);
        viewport.height = static_cast<float>(dim);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.extent.width = dim;
        scissor.extent.height = dim;

        {
            wrapper::OnceCommandBuffer single_command(device);
            single_command.create_command_buffer();
            single_command.start_recording();

            m_cubemap_texture->transition_image_layout(
                single_command.command_buffer(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, miplevel_count, CUBE_FACE_COUNT);

            single_command.end_recording_and_submit_command();
        }

        // TODO: Implement graphics pipeline builder
        // TODO: Split up in setup of pipeline and rendering of cubemaps
        for (std::uint32_t mip_level = 0; mip_level < miplevel_count; mip_level++) {
            for (std::uint32_t face = 0; face < CUBE_FACE_COUNT; face++) {

                wrapper::OnceCommandBuffer cmd_buf(device);

                cmd_buf.create_command_buffer();
                cmd_buf.start_recording();

                viewport.width = static_cast<float>(dim * std::pow(0.5f, mip_level));
                viewport.height = static_cast<float>(dim * std::pow(0.5f, mip_level));

                vkCmdSetViewport(cmd_buf.command_buffer(), 0, 1, &viewport);
                vkCmdSetScissor(cmd_buf.command_buffer(), 0, 1, &scissor);

                // Render scene from cube face's point of view
                vkCmdBeginRenderPass(cmd_buf.command_buffer(), &renderpass_bi, VK_SUBPASS_CONTENTS_INLINE);

                switch (target) {
                case IRRADIANCE:
                    pushBlockIrradiance.mvp =
                        // TODO: Use static cast
                        glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[face];

                    vkCmdPushConstants(cmd_buf.command_buffer(), pipeline_layout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                       sizeof(PushBlockIrradiance), &pushBlockIrradiance);
                    break;
                case PREFILTEREDENV:
                    pushBlockPrefilterEnv.mvp =
                        // TODO: Use static cast
                        glm::perspective((float)(M_PI / 2.0), 1.0f, 0.1f, 512.0f) * matrices[face];
                    pushBlockPrefilterEnv.roughness = (float)mip_level / (float)(miplevel_count - 1);

                    vkCmdPushConstants(cmd_buf.command_buffer(), pipeline_layout,
                                       VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                                       sizeof(PushBlockPrefilterEnv), &pushBlockPrefilterEnv);
                    break;
                };

                vkCmdBindPipeline(cmd_buf.command_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

                vkCmdBindDescriptorSets(cmd_buf.command_buffer(), VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0,
                                        1, &m_descriptor->descriptor_set(), 0, nullptr);

                VkDeviceSize offsets[1] = {0};

                // TODO: draw skybox!
                // TODO: How to make this part a parameter? Meaning we can control what's in the skybox? rendergraph?

                vkCmdEndRenderPass(cmd_buf.command_buffer());

                m_offscreen_framebuffer->transition_image_layout(cmd_buf.command_buffer(),
                                                                 VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

                m_cubemap_texture->copy_from_image(cmd_buf.command_buffer(), m_offscreen_framebuffer->image(), face,
                                                   mip_level, static_cast<std::uint32_t>(viewport.width),
                                                   static_cast<std::uint32_t>(viewport.height));

                m_offscreen_framebuffer->transition_image_layout(cmd_buf.command_buffer(),
                                                                 VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                cmd_buf.end_recording_and_submit_command();
            }
        }

        {
            wrapper::OnceCommandBuffer single_command(device);
            single_command.create_command_buffer();
            single_command.start_recording();

            m_cubemap_texture->transition_image_layout(single_command.command_buffer(),
                                                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, miplevel_count,
                                                       CUBE_FACE_COUNT);

            single_command.end_recording_and_submit_command();
        }

        // TODO: Create RAII wrappers for these
        vkDestroyRenderPass(device.device(), renderpass, nullptr);
        vkDestroyPipeline(device.device(), pipeline, nullptr);
        vkDestroyPipelineLayout(device.device(), pipeline_layout, nullptr);
    }
}

} // namespace inexor::vulkan_renderer::cubemap
