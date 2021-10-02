#include "inexor/vulkan-renderer/wrapper/image.hpp"

#include "inexor/vulkan-renderer/exception.hpp"
#include "inexor/vulkan-renderer/wrapper/device.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"
#include "inexor/vulkan-renderer/wrapper/once_command_buffer.hpp"

#include <spdlog/spdlog.h>

#include <utility>

namespace inexor::vulkan_renderer::wrapper {

void Image::create_image(const VkImageCreateInfo image_ci) {
    VmaAllocationCreateInfo vma_allocation_ci{};
    vma_allocation_ci.usage = VMA_MEMORY_USAGE_GPU_ONLY;

#if VMA_RECORDING_ENABLED
    vma_allocation_ci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_USER_DATA_COPY_STRING_BIT;
    vma_allocation_ci.pUserData = m_name.data();
#else
    vma_allocation_ci.flags = VMA_ALLOCATION_CREATE_MAPPED_BIT;
#endif

    if (const auto result = vmaCreateImage(m_device.allocator(), &image_ci, &vma_allocation_ci, &m_image, &m_allocation,
                                           &m_allocation_info);
        result != VK_SUCCESS) {
        throw VulkanException("Error: vmaCreateImage failed for image " + m_name + "!", result);
    }

    // Assign an internal name using Vulkan debug markers.
    m_device.set_debug_marker_name(m_image, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_EXT, m_name);
}

void Image::create_image_view(const VkImageViewCreateInfo image_view_ci) {
    if (const auto result = vkCreateImageView(m_device.device(), &image_view_ci, nullptr, &m_image_view);
        result != VK_SUCCESS) {
        throw VulkanException("Error: vkCreateImageView failed for image view " + m_name + "!", result);
    }

    // Assign an internal name using Vulkan debug markers.
    m_device.set_debug_marker_name(m_image_view, VK_DEBUG_REPORT_OBJECT_TYPE_IMAGE_VIEW_EXT, m_name);
}

Image::Image(const Device &device, const VkImageCreateInfo image_ci, const VkImageViewCreateInfo image_view_ci,
             std::string name)

    : m_device(device), m_format(image_ci.format), m_name(std::move(name)) {

    assert(device.device());
    assert(device.physical_device());
    assert(device.allocator());

    create_image(image_ci);
    create_image_view(image_view_ci);
}

Image::Image(const Device &device, const VkImageCreateFlags flags, const VkImageType image_type, const VkFormat format,
             const std::uint32_t width, const std::uint32_t height, const std::uint32_t miplevel_count,
             const std::uint32_t layer_count, const VkSampleCountFlagBits sample_count,
             const VkImageUsageFlags usage_flags, const VkImageViewType view_type,
             const VkComponentMapping view_components, const VkImageAspectFlags aspect_mask, std::string name)

    : m_device(device), m_format(format), m_name(std::move(name)) {

    assert(device.device());
    assert(device.physical_device());
    assert(device.allocator());
    assert(layer_count > 0);
    assert(miplevel_count > 0);
    assert(width > 0);
    assert(height > 0);
    assert(!m_name.empty());

    auto image_ci = make_info<VkImageCreateInfo>();
    image_ci.flags = flags;
    image_ci.imageType = image_type;
    image_ci.format = format;
    image_ci.extent.width = width;
    image_ci.extent.height = height;
    image_ci.extent.depth = 1;
    image_ci.mipLevels = miplevel_count;
    image_ci.arrayLayers = layer_count;
    image_ci.samples = sample_count;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = usage_flags;
    image_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    create_image(image_ci);

    auto image_view_ci = make_info<VkImageViewCreateInfo>();
    image_view_ci.image = m_image;
    image_view_ci.viewType = view_type;
    image_view_ci.format = format;
    image_view_ci.components = view_components;
    image_view_ci.subresourceRange.aspectMask = aspect_mask;
    image_view_ci.subresourceRange.baseMipLevel = 0;
    image_view_ci.subresourceRange.levelCount = miplevel_count;
    image_view_ci.subresourceRange.baseArrayLayer = 0;
    image_view_ci.subresourceRange.layerCount = layer_count;

    create_image_view(image_view_ci);
}

Image::Image(const Device &device, const VkImageCreateFlags flags, const VkImageType image_type, const VkFormat format,
             const std::uint32_t width, const std::uint32_t height, const std::uint32_t miplevel_count,
             const std::uint32_t layer_count, const VkSampleCountFlagBits sample_count,
             const VkImageUsageFlags usage_flags, const VkImageViewType view_type, const VkImageAspectFlags aspect_mask,
             std::string name)
    : Image(device, flags, image_type, format, width, height, miplevel_count, layer_count, sample_count, usage_flags,
            view_type, {}, aspect_mask, name) {}

Image::Image(const Device &device, const VkImageType image_type, const VkFormat format, const std::uint32_t width,
             const std::uint32_t height, const std::uint32_t miplevel_count, const std::uint32_t layer_count,
             const VkSampleCountFlagBits sample_count, const VkImageUsageFlags usage_flags,
             const VkImageViewType view_type, const VkImageAspectFlags aspect_mask, std::string name)
    : Image(device, {}, image_type, format, width, height, miplevel_count, layer_count, sample_count, usage_flags,
            view_type, {}, aspect_mask, name) {}

Image::Image(const Device &device, const VkFormat format, const std::uint32_t width, const std::uint32_t height,
             const VkImageUsageFlags usage_flags, const VkImageAspectFlags aspect_mask, std::string name)
    : Image(device, {}, VK_IMAGE_TYPE_2D, format, width, height, 1, 1, VK_SAMPLE_COUNT_1_BIT, usage_flags,
            VK_IMAGE_VIEW_TYPE_2D, {}, aspect_mask, name) {}

void Image::transition_image_layout(const VkImageLayout old_layout, const VkImageLayout new_layout) {

    auto barrier = make_info<VkImageMemoryBarrier>();
    barrier.oldLayout = old_layout;
    barrier.newLayout = new_layout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = m_image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags source_stage = 0;
    VkPipelineStageFlags destination_stage = 0;

    if (VK_IMAGE_LAYOUT_UNDEFINED == old_layout && VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == new_layout) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        source_stage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL == old_layout &&
               VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL == new_layout) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        source_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
        throw std::runtime_error("Error: unsupported layout transition!");
    }

    spdlog::debug("Recording pipeline barrier for image layer transition");

    OnceCommandBuffer image_transition_change(m_device, m_device.graphics_queue(),
                                              m_device.graphics_queue_family_index());

    image_transition_change.create_command_buffer();
    image_transition_change.start_recording();

    vkCmdPipelineBarrier(image_transition_change.command_buffer(), source_stage, destination_stage, 0, 0, nullptr, 0,
                         nullptr, 1, &barrier);

    image_transition_change.end_recording_and_submit_command();
}

void Image::place_pipeline_barrier(const VkCommandBuffer command_buffer, const VkImageLayout old_layout,
                                   const VkImageLayout new_layout, const VkAccessFlags src_access_mask,
                                   const VkAccessFlags dest_access_mask,
                                   const VkImageSubresourceRange subresource_range) {

    VkImageMemoryBarrier mem_barrier{};
    mem_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    mem_barrier.image = m_image;
    mem_barrier.oldLayout = old_layout;
    mem_barrier.newLayout = new_layout;
    mem_barrier.srcAccessMask = src_access_mask;
    mem_barrier.dstAccessMask = dest_access_mask;
    mem_barrier.subresourceRange = subresource_range;

    vkCmdPipelineBarrier(command_buffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 0,
                         nullptr, 0, nullptr, 1, &mem_barrier);
}

void Image::place_pipeline_barrier(const VkCommandBuffer command_buffer, const VkImageLayout old_layout,
                                   const VkImageLayout new_layout, const VkAccessFlags src_access_mask,
                                   const VkAccessFlags dest_access_mask) {

    place_pipeline_barrier(command_buffer, old_layout, new_layout, src_access_mask, dest_access_mask,
                           {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1});
}

void Image::copy_from_image(const VkCommandBuffer command_buffer, const VkImage src_image, const std::uint32_t width,
                            const std::uint32_t height, const std::uint32_t miplevel_count,
                            const std::uint32_t layer_count, const std::uint32_t base_array_layer,
                            const std::uint32_t mip_level) {

    VkImageSubresourceRange subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = miplevel_count;
    subresourceRange.layerCount = layer_count;

    place_pipeline_barrier(command_buffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                           VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
                           VK_ACCESS_TRANSFER_READ_BIT);

    VkImageCopy copy_region{};
    copy_region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.srcSubresource.baseArrayLayer = 0;
    copy_region.srcSubresource.mipLevel = 0;
    copy_region.srcSubresource.layerCount = layer_count;
    copy_region.srcOffset = {0, 0, 0};
    copy_region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    copy_region.dstSubresource.baseArrayLayer = base_array_layer;
    copy_region.dstSubresource.mipLevel = mip_level;
    copy_region.dstSubresource.layerCount = layer_count;
    copy_region.dstOffset = {0, 0, 0};
    copy_region.extent.width = width;
    copy_region.extent.height = height;
    copy_region.extent.depth = 1;

    vkCmdCopyImage(command_buffer, src_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, m_image,
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_region);

    place_pipeline_barrier(command_buffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                           VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_ACCESS_TRANSFER_READ_BIT,
                           VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT);
}

void Image::copy_from_buffer(const VkCommandBuffer command_buffer, const VkBuffer src_buffer, const std::uint32_t width,
                             const std::uint32_t height) {

    VkBufferImageCopy image_copy{};
    image_copy.bufferOffset = 0;
    image_copy.bufferRowLength = 0;
    image_copy.bufferImageHeight = 0;
    image_copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    image_copy.imageSubresource.mipLevel = 0;
    image_copy.imageSubresource.baseArrayLayer = 0;
    image_copy.imageSubresource.layerCount = 1;
    image_copy.imageOffset = {0, 0, 0};
    image_copy.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(command_buffer, src_buffer, m_image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &image_copy);
}

Image::Image(Image &&other) noexcept : m_device(other.m_device) {
    m_allocation = other.m_allocation;
    m_allocation_info = other.m_allocation_info;
    m_image = other.m_image;
    m_format = other.m_format;
    m_image_view = other.m_image_view;
    m_name = std::move(other.m_name);
}

Image::~Image() {
    vkDestroyImageView(m_device.device(), m_image_view, nullptr);
    vmaDestroyImage(m_device.allocator(), m_image, m_allocation);
}

} // namespace inexor::vulkan_renderer::wrapper
