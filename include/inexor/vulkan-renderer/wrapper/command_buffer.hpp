#pragma once

#include <vulkan/vulkan_core.h>

#include <cassert>
#include <cstdint>
#include <string>
#include <vector>

namespace inexor::vulkan_renderer::wrapper {

// Forward declarations
class Device;
class ResourceDescriptor;

/// @brief RAII wrapper class for VkCommandBuffer.
/// @todo Make trivially copyable (this class doesn't really "own" the command buffer, more just an OOP wrapper).
class CommandBuffer {
protected:
    const Device &m_device;
    VkCommandBuffer m_command_buffer{VK_NULL_HANDLE};
    std::string m_name;

    CommandBuffer(const Device &device);
    void create_command_buffer(const VkCommandPool command_pool);

public:
    /// @brief Default constructor
    /// @param device The const reference to the device RAII wrapper class
    /// @param command_pool The command pool from which the command buffer will be allocated
    /// @param name The internal debug marker name of the command buffer. This must not be an empty string
    CommandBuffer(const Device &device, VkCommandPool command_pool, std::string name);

    CommandBuffer(const CommandBuffer &) = delete;
    CommandBuffer(CommandBuffer &&) noexcept;
    ~CommandBuffer() = default;

    CommandBuffer &operator=(const CommandBuffer &) noexcept = default;
    CommandBuffer &operator=(CommandBuffer &&) noexcept = default;

    /// @brief Call vkBeginCommandBuffer
    /// @note Sometimes it's useful to pass VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT to specify that a command
    /// buffer can be resubmitted to a queue while it is in the pending state, and recorded into multiple primary
    /// command buffers. Otherwise, synchronization must be done using a VkFence
    /// @param flags The command buffer usage flags, 0 by default
    void begin(VkCommandBufferUsageFlags flags = 0) const;

    /// @brief Call vkCmdBindDescriptorSets
    /// @param descriptor_set The const reference to the resource descriptor RAII wrapper instance
    /// @param layout The pipeline layout which will be used to bind the resource descriptor
    /// @param first_set The first set to use
    void bind_descriptor_set(VkDescriptorSet descriptor_set, VkPipelineLayout layout,
                             VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS) const;

    /// @brief Call vkCmdBindDescriptorSets
    /// @param descriptor_sets
    /// @param layout
    /// @param bind_point
    void bind_descriptor_sets(const std::vector<VkDescriptorSet> &descriptor_sets, VkPipelineLayout layout,
                              VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS) const;

    /// @brief Update push constant data
    /// @tparam T the type of the push constants
    /// @param data The data
    /// @param layout The pipeline layout
    /// @param stage_flags The shader stage(s) that will accept the push constants
    /// @param offset The data offset
    template <typename T>
    void push_constants(const T *data, const VkPipelineLayout layout, const VkShaderStageFlags stage_flags,
                        const std::uint32_t offset = 0) const {
        assert(data);
        assert(layout);
        vkCmdPushConstants(m_command_buffer, layout, stage_flags, offset, sizeof(T), data);
    }

    /// @brief Update push constant data
    /// @tparam T the type of the push constants
    /// @param data The data
    /// @param layout The pipeline layout
    /// @param stage_flags The shader stage(s) that will accept the push constants
    /// @param offset The data offset
    template <typename T>
    void push_constants(const T &data, const VkPipelineLayout layout, const VkShaderStageFlags stage_flags,
                        const std::uint32_t offset = 0) const {
        assert(layout);
        vkCmdPushConstants(m_command_buffer, layout, stage_flags, offset, sizeof(T), &data);
    }

    /// @brief Call vkEndCommandBuffer
    void end() const;

    // Graphics commands
    // TODO(): Switch to taking in OOP wrappers when we have them (e.g. bind_vertex_buffers takes in a VertexBuffer)

    void copy_buffer(VkBuffer source_buffer, VkBuffer target_buffer, const VkBufferCopy &copy_region) const;

    void copy_buffer(VkBuffer source_buffer, VkBuffer target_buffer, VkDeviceSize source_buffer_size) const;

    void copy_buffer_to_image(VkBuffer src_buffer, VkImage target_image, const VkBufferImageCopy &regions) const;

    void copy_buffer_to_image(VkBuffer src_buffer, VkImage target_image,
                              const std::vector<VkBufferImageCopy> &regions) const;

    void copy_image(VkImage source_image, VkImage target_image, const VkImageCopy &copy_region) const;

    void set_scissor(const VkRect2D &scissor) const;

    void set_scissor(std::uint32_t width, std::uint32_t height) const;

    void set_viewport(const VkViewport &viewport) const;

    void set_viewport(std::uint32_t width, std::uint32_t height, float min_depth = 0.0f, float max_depth = 1.0f) const;

    void pipeline_barrier(VkPipelineStageFlags source_stage_flags, VkPipelineStageFlags destination_stage_flags,
                          const VkImageMemoryBarrier &barrier) const;

    /// @brief Call vkCmdBeginRenderPass
    /// @param render_pass_bi The const reference to the VkRenderPassBeginInfo which is used
    void begin_render_pass(const VkRenderPassBeginInfo &render_pass_bi) const;

    /// @brief Call vkCmdBindPipeline
    /// @param pipeline The graphics pipeline to bind
    /// @param bind_point The pipeline bind point, ``VK_PIPELINE_BIND_POINT_GRAPHICS`` by default
    void bind_graphics_pipeline(VkPipeline pipeline,
                                VkPipelineBindPoint bind_point = VK_PIPELINE_BIND_POINT_GRAPHICS) const;

    /// @brief Call vkCmdBindIndexBuffer
    /// @param buffer The index buffer to bind
    /// @param index_type The index type, ``VK_INDEX_TYPE_UINT32`` by default
    /// @param offset The buffer offset, ``0`` by default
    void bind_index_buffer(VkBuffer buffer, VkIndexType index_type = VK_INDEX_TYPE_UINT32,
                           std::uint32_t offset = 0) const;

    /// @brief Call vkCmdBindVertexBuffers
    /// @param buffers A std::vector of vertex buffers to bind
    /// @todo Expose more parameters from vkCmdBindVertexBuffers as method arguments
    void bind_vertex_buffer(VkBuffer buffers) const;

    /// @brief Call vkCmdBindVertexBuffers
    /// @param buffers A std::vector of vertex buffers to bind
    /// @todo Expose more parameters from vkCmdBindVertexBuffers as method arguments
    void bind_vertex_buffers(const std::vector<VkBuffer> &buffers) const;

    /// @brief Call vkCmdDraw
    /// @param vertex_count The number of vertices to draw
    /// @param first_vertex
    /// @param instance_count
    /// @param first_instance
    void draw(std::size_t vertex_count, std::uint32_t first_vertex = 0, std::uint32_t instance_count = 1,
              std::uint32_t first_instance = 0) const;

    /// @brief Call vkCmdDrawIndexed
    /// @param index_count The number of indices to draw
    /// @param first_index The first index, ``0`` by default
    /// @param vertex_offset The vertex offset, ``0`` by default
    /// @param instance_count The number of instances to draw, ``1`` by default
    /// @param first_instance The first instance, ``0`` by default
    void draw_indexed(std::size_t index_count, std::uint32_t first_index = 0, std::uint32_t vertex_offset = 0,
                      std::uint32_t instance_count = 1, std::uint32_t first_instance = 0) const;

    /// @brief Call vkCmdEndRenderPass
    void end_render_pass() const;

    // TODO: Refactor: unified get syntax!
    [[nodiscard]] const VkCommandBuffer *ptr() const {
        return &m_command_buffer;
    }
};

} // namespace inexor::vulkan_renderer::wrapper
