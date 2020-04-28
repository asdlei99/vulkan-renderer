#pragma once

#include "inexor/vulkan-renderer/gpu_memory_buffer.hpp"

#include <vulkan/vulkan.h>

#include <string>

namespace inexor::vulkan_renderer {

class Texture {
public:
    Texture() = default;

    ~Texture() = default;

    std::string name = "";

    std::string file_name = "";

    VkImageViewCreateInfo view_create_info = {};

    VkImageCreateInfo image_create_info = {};

    VmaAllocation allocation = VK_NULL_HANDLE;

    VmaAllocationInfo allocation_info = {};

    VkBufferCreateInfo create_info = {};

    VmaAllocationCreateInfo allocation_create_info = {};

    VkImage image = {};

    VkFormat format = {};

    VkImageView image_view = {};

    VkImageLayout image_layout = {};

    VkDescriptorImageInfo descriptor = {};

    VkSampler sampler = {};

    std::uint32_t layer_count = 0;

    std::uint32_t mip_levels = 0;

    std::uint32_t width = 0;

    std::uint32_t height = 0;

    // TODO: Refactoring: Remove methods! only let VulkanTextureManager change data.

    /// @brief Destroys a texture.
    /// @param device [in] The Vulkan device.
    /// @param vma_allocator [in] The Vulkan memory allocator handle.
    void destroy_texture(const VkDevice &device, const VmaAllocator &vma_allocator);

    /// @brief Updates the texture sampler's descriptor.
    void update_descriptor();
};

} // namespace inexor::vulkan_renderer
