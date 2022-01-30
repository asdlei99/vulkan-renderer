#include "inexor/vulkan-renderer/cubemap/gpu_cubemap.hpp"

#include "inexor/vulkan-renderer/exception.hpp"
#include "inexor/vulkan-renderer/texture/sampler.hpp"
#include "inexor/vulkan-renderer/vk_tools/representation.hpp"
#include "inexor/vulkan-renderer/wrapper/image.hpp"
#include "inexor/vulkan-renderer/wrapper/make_info.hpp"
#include "inexor/vulkan-renderer/wrapper/once_command_buffer.hpp"

#include <ktx.h>
#include <ktxvulkan.h>

#include <algorithm>
#include <cassert>

namespace inexor::vulkan_renderer::cubemap {

VkImageCreateInfo GpuCubemap::fill_image_ci(const VkFormat format, const std::uint32_t width,
                                            const std::uint32_t height, const std::uint32_t miplevel_count) {
    assert(width > 0);
    assert(height > 0);
    assert(miplevel_count > 0);

    auto image_ci = wrapper::make_info<VkImageCreateInfo>();
    image_ci.imageType = VK_IMAGE_TYPE_2D;
    image_ci.format = format;
    image_ci.extent.width = width;
    image_ci.extent.height = height;
    image_ci.extent.depth = 1;
    image_ci.mipLevels = miplevel_count;
    image_ci.arrayLayers = FACE_COUNT;
    image_ci.samples = VK_SAMPLE_COUNT_1_BIT;
    image_ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    image_ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    image_ci.usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    image_ci.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    return image_ci;
}

VkImageCreateInfo GpuCubemap::fill_image_ci(const VkFormat format, const texture::CpuTexture &cpu_cubemap) {
    return fill_image_ci(format, cpu_cubemap.width(), cpu_cubemap.height(), cpu_cubemap.miplevel_count());
}

VkImageViewCreateInfo GpuCubemap::fill_image_view_ci(const VkFormat format, const std::uint32_t miplevel_count) {
    assert(miplevel_count > 0);

    auto image_view_ci = wrapper::make_info<VkImageViewCreateInfo>();

    image_view_ci.viewType = VK_IMAGE_VIEW_TYPE_CUBE;
    image_view_ci.format = format;
    image_view_ci.components = {VK_COMPONENT_SWIZZLE_R, VK_COMPONENT_SWIZZLE_G, VK_COMPONENT_SWIZZLE_B,
                                VK_COMPONENT_SWIZZLE_A};
    image_view_ci.subresourceRange = {};
    image_view_ci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    image_view_ci.subresourceRange.levelCount = miplevel_count;
    image_view_ci.subresourceRange.layerCount = FACE_COUNT;
    // Note hat the image will be filled out later
    return image_view_ci;
}

VkSamplerCreateInfo GpuCubemap::fill_sampler_ci(const std::uint32_t miplevel_count) {
    assert(miplevel_count > 0);

    auto sampler_ci = wrapper::make_info<VkSamplerCreateInfo>();
    sampler_ci.magFilter = VK_FILTER_LINEAR;
    sampler_ci.minFilter = VK_FILTER_LINEAR;
    sampler_ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    sampler_ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    sampler_ci.minLod = 0.0f;
    sampler_ci.maxLod = 10.0f;
    sampler_ci.anisotropyEnable = true;
    sampler_ci.maxAnisotropy = 16.0f;
    sampler_ci.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
    return sampler_ci;
}

GpuCubemap::GpuCubemap(const wrapper::Device &device, const VkFormat format, const texture::CpuTexture &cpu_cubemap,
                       std::string name)
    : m_device(device), m_name(cpu_cubemap.name()),
      Image(device, fill_image_ci(format, cpu_cubemap), fill_image_view_ci(format, cpu_cubemap.miplevel_count()),
            name) {

    // Make sure the cpu texture which is passed in is a ktx texture
    // TODO: Do not use assertions, generate an error cubemap instead!
    assert(cpu_cubemap.ktx_wrapper() != nullptr);

    wrapper::StagingBuffer texture_staging_buffer(m_device, cpu_cubemap.ktx_texture_data_size(),
                                                  cpu_cubemap.ktx_texture_data(), m_name);

    // Setup buffer copy regions for each face including all of its mip levels
    std::vector<VkBufferImageCopy> copy_regions;

    copy_regions.reserve(cpu_cubemap.miplevel_count() * FACE_COUNT);

    for (std::uint32_t face = 0; face < FACE_COUNT; face++) {
        for (std::uint32_t mip_level = 0; mip_level < cpu_cubemap.miplevel_count(); mip_level++) {
            ktx_size_t offset = 0;

            if (const auto result = ktxTexture_GetImageOffset(cpu_cubemap.ktx_wrapper(), mip_level, 0, face, &offset);
                result != KTX_SUCCESS) {
                throw KtxException("Error: ktxTexture_GetImageOffset failed!", result);
            }

            VkBufferImageCopy copy_region{};
            copy_region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            copy_region.imageSubresource.mipLevel = mip_level;
            copy_region.imageSubresource.baseArrayLayer = face;
            copy_region.imageSubresource.layerCount = 1;
            copy_region.imageExtent.width = cpu_cubemap.ktx_wrapper()->baseWidth >> mip_level;
            copy_region.imageExtent.height = cpu_cubemap.ktx_wrapper()->baseHeight >> mip_level;
            copy_region.imageExtent.depth = 1;
            copy_region.bufferOffset = offset;

            copy_regions.push_back(copy_region);
        }
    }

    // Image barrier for optimal image (target)
    // Set initial layout for all array layers (faces) of the optimal (target) tiled texture
    VkImageSubresourceRange subresourceRange = {};
    subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    subresourceRange.baseMipLevel = 0;
    subresourceRange.levelCount = cpu_cubemap.miplevel_count();
    subresourceRange.layerCount = FACE_COUNT;

    wrapper::OnceCommandBuffer copy_command(m_device, [&](const VkCommandBuffer cmd_buf) {
        // TODO: Move transition_image_layout into vkCmdCopyBufferToImage as well
        transition_image_layout(cmd_buf, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, cpu_cubemap.miplevel_count(),
                                FACE_COUNT);

        // TODO: Move into cmd_buf wrapper
        vkCmdCopyBufferToImage(cmd_buf, texture_staging_buffer.buffer(), image(), VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                               static_cast<std::uint32_t>(copy_regions.size()), copy_regions.data());

        transition_image_layout(cmd_buf, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, cpu_cubemap.miplevel_count(),
                                FACE_COUNT);
    });

    m_sampler = std::make_unique<texture::Sampler>(m_device, fill_sampler_ci(cpu_cubemap.miplevel_count()), m_name);
}

GpuCubemap::GpuCubemap(const wrapper::Device &device, VkImageCreateInfo image_ci, VkImageViewCreateInfo image_view_ci,
                       VkSamplerCreateInfo sampler_ci, std::string name)
    : m_device(device), m_image_ci(image_ci), m_image_view_ci(image_view_ci), m_sampler_ci(sampler_ci),
      m_name(name), wrapper::Image(device, image_ci, image_view_ci, name) {

    m_sampler = std::make_unique<texture::Sampler>(device, m_sampler_ci, m_name);
    descriptor_image_info.sampler = m_sampler->sampler();
}

GpuCubemap::GpuCubemap(const wrapper::Device &device, const VkFormat format, const std::uint32_t width,
                       const std::uint32_t height, const std::uint32_t miplevel_count, const std::string name)
    : GpuCubemap(device, fill_image_ci(format, width, height, miplevel_count),
                 fill_image_view_ci(format, miplevel_count), fill_sampler_ci(miplevel_count), name) {}

GpuCubemap::GpuCubemap(const wrapper::Device &device, VkFormat format, std::uint32_t width, std::uint32_t height,
                       std::string name)
    : GpuCubemap(device, format, width, height, 1, name) {}

void GpuCubemap::copy_from_image(const VkCommandBuffer cmd_buf, const VkImage source_image, const std::uint32_t face,
                                 const std::uint32_t mip_level, const std::uint32_t width, const std::uint32_t height) {
    VkImageCopy region{};

    region.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.srcSubresource.baseArrayLayer = 0;
    region.srcSubresource.mipLevel = 0;
    region.srcSubresource.layerCount = 1;
    region.srcOffset = {0, 0, 0};

    region.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.dstSubresource.baseArrayLayer = face;
    region.dstSubresource.mipLevel = mip_level;
    region.dstSubresource.layerCount = 1;
    region.dstOffset = {0, 0, 0};

    region.extent.width = width;
    region.extent.height = height;
    region.extent.depth = 1;

    // TODO: move into cmd_buf wrapper!
    vkCmdCopyImage(cmd_buf, source_image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image(),
                   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
}

} // namespace inexor::vulkan_renderer::cubemap
