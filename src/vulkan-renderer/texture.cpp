#include "inexor/vulkan-renderer/texture.hpp"

namespace inexor::vulkan_renderer {

void Texture::destroy_texture(const VkDevice &device, const VmaAllocator &vma_allocator) {
    vkDestroySampler(device, sampler, nullptr);

    vmaDestroyImage(vma_allocator, image, allocation);

    vkDestroyImageView(device, image_view, nullptr);

    // We don't need to destroy any buffers in here.

    file_name = "";
    name = "";
    image = VK_NULL_HANDLE;
    image_view = VK_NULL_HANDLE;
    sampler = VK_NULL_HANDLE;
}

void Texture::update_descriptor() {
    descriptor.sampler = sampler;
    descriptor.imageView = image_view;
    descriptor.imageLayout = image_layout;
}

} // namespace inexor::vulkan_renderer
