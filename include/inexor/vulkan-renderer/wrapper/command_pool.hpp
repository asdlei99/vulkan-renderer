#pragma once

#include "inexor/vulkan-renderer/wrapper/command_buffer.hpp"

namespace inexor::vulkan_renderer::wrapper {

// Forward declaration
class Device;

/// @brief RAII wrapper class for VkCommandPool.
class CommandPool {
    std::string m_name;
    const Device &m_device;
    VkCommandPool m_cmd_pool{VK_NULL_HANDLE};

    /// The command buffers which can be requested by the current thread
    std::vector<std::unique_ptr<CommandBuffer>> m_cmd_bufs;

public:
    /// Default constructor
    /// @param device The device wrapper instance
    /// @param name The internal debug marker name which will be assigned to this command pool
    CommandPool(const Device &device, std::string name);

    CommandPool(const CommandPool &) = delete;
    CommandPool(CommandPool &&) noexcept;

    ~CommandPool();

    CommandPool &operator=(const CommandPool &) = delete;
    CommandPool &operator=(CommandPool &&) = delete;

    [[nodiscard]] VkCommandPool get() const {
        return m_cmd_pool;
    }

    [[nodiscard]] const VkCommandPool *ptr() const {
        return &m_cmd_pool;
    }

    /// Request a command buffer
    /// @param name The internal debug name which will be assigned to this command buffer (must not be empty)
    /// @return A command buffer handle instance which allows access to the requested command buffer
    [[nodiscard]] const CommandBuffer &request_command_buffer(const std::string &name);
};

} // namespace inexor::vulkan_renderer::wrapper
