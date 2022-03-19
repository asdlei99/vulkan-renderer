#pragma once

#include "inexor/vulkan-renderer/wrapper/device.hpp"

#include <vulkan/vulkan_core.h>

#include <string>

namespace inexor::vulkan_renderer::wrapper {

// TODO: Add a wrapper for Vulkan compute pipelines

class GraphicsPipeline {
private:
    const Device &m_device;
    std::string m_name;

    VkPipeline m_pipeline;

public:
    GraphicsPipeline(const Device &device, const VkGraphicsPipelineCreateInfo &graphics_pipeline_ci, std::string name);

    GraphicsPipeline(const GraphicsPipeline &) = delete;
    GraphicsPipeline(GraphicsPipeline &&) noexcept;
    ~GraphicsPipeline();

    GraphicsPipeline &operator=(const GraphicsPipeline &) = delete;
    GraphicsPipeline &operator=(GraphicsPipeline &&) noexcept = default;

    [[nodiscard]] VkPipeline pipeline() const {
        return m_pipeline;
    }
};

} // namespace inexor::vulkan_renderer::wrapper