#include "inexor/vulkan-renderer/exception.hpp"

#include "inexor/vulkan-renderer/vk_tools/representation.hpp"

namespace inexor::vulkan_renderer {

VulkanException::VulkanException(std::string message, const VkResult result)
    : InexorException(message.append(" (")
                          .append(vk_tools::as_string<VkResult>(result))
                          .append(": ")
                          .append(vk_tools::result_to_description(result))
                          .append(")")) {}

KtxException::KtxException(std::string message, const ktx_error_code_e result)
    : InexorException(message.append(" (")
                          .append(vk_tools::as_string(result))
                          .append(": ")
                          .append(vk_tools::result_to_description(result))
                          .append(")")) {}

} // namespace inexor::vulkan_renderer
