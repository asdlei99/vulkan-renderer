#pragma once

#include "inexor/vulkan-renderer/gltf/model_file.hpp"
#include "inexor/vulkan-renderer/wrapper/device.hpp"
#include "inexor/vulkan-renderer/wrapper/gpu_texture.hpp"

#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>

#include <vector>

namespace inexor::vulkan_renderer::gltf {

/// @brief A struct for glTF2 model materials.
struct ModelMaterial {
    glm::vec4 base_color_factor{1.0f};
    std::uint32_t base_color_texture_index{0};
};

/// @brief A struct for glTF2 model vertices.
struct ModelVertex {
    ModelVertex() {}
    ///
    ///
    ///
    ModelVertex(const glm::vec3 position, const glm::vec3 color_rgb) : pos(position), color(color_rgb) {}
    glm::vec3 pos{};
    glm::vec3 normal{};
    glm::vec2 uv{};
    glm::vec3 color{};
};

/// @brief A struct for glTF2 model primitives.
struct ModelPrimitive {
    std::uint32_t first_index{0};
    std::uint32_t index_count{0};
    std::int32_t material_index{0};
};

struct ModelScene {
    std::vector<ModelVertex> vertices;
    std::vector<uint32_t> indices;
};

/// @brief A struct for glTF2 model nodes.
/// @warning Since glTF2 models can be very huge, we could run into out of stack problems when calling the destructors
/// of the tree's nodes.
struct ModelNode {
    ModelNode *parent{nullptr};
    std::vector<ModelNode> children;
    std::vector<ModelPrimitive> mesh;
    glm::mat4 matrix{};
};

/// @brief A wrapper class for glTF2 models.
/// Loading the glTF2 file is separated from parsing its data.
/// This allows for better task-based parallelization.
class Model {
private:
    const tinygltf::Model &m_model;
    const wrapper::Device &m_device;

    std::vector<wrapper::GpuTexture> m_textures;
    std::vector<std::uint32_t> m_texture_indices;
    std::vector<ModelMaterial> m_materials;
    std::vector<ModelNode> m_nodes;
    std::vector<ModelScene> m_scenes;

    /// @brief Load a glTF2 model node.
    /// @param start_node The node to begin with
    /// @param parent The parent node
    /// @param vertices The model node's vertices
    /// @param indices The model node's indices
    void load_node(const tinygltf::Node &start_node, ModelNode *parent, std::vector<ModelVertex> &vertices,
                   std::vector<std::uint32_t> &indices);

    void load_materials();
    void load_textures();
    void load_nodes();

    // TODO: load animations
    // TODO: load animation skins
    // TODO: load pbr (physically based rendering) settings
    // TODO: load multiple texture coordinate sets
    // TODO: try to .reserve() memory for vectors and use emplace_back()

public:
    /// @brief Extract the model data from a model file.
    /// @paran device The device wrapper
    /// @param model The glTF2 model
    Model(const wrapper::Device &device, const tinygltf::Model &model);

    /// @brief Overloaded constructor which accepts ModelFile as argument
    /// @paran device The device wrapper
    /// @param model_file The glTF2 model file
    Model(const wrapper::Device &device, const ModelFile &model_file);

    [[nodiscard]] std::size_t texture_count() const noexcept {
        return m_textures.size();
    }

    [[nodiscard]] std::size_t texture_index_count() const noexcept {
        return m_texture_indices.size();
    }

    [[nodiscard]] std::size_t material_count() const noexcept {
        return m_materials.size();
    }

    [[nodiscard]] std::size_t node_count() const noexcept {
        return m_nodes.size();
    }

    [[nodiscard]] std::size_t scene_count() const noexcept {
        return m_scenes.size();
    }

    [[nodiscard]] auto scene_vertices(const std::size_t scene_index) const {
        assert(scene_index < m_scenes.size());
        if (scene_index > m_scenes.size()) {
            // TODO: Is this worth an exception?
            spdlog::error("No vertices for scene index {}!", scene_index);
            return std::vector<ModelVertex>{};
        }
        return m_scenes[scene_index].vertices;
    }

    [[nodiscard]] auto scene_indices(const std::size_t scene_index) const {
        assert(scene_index < m_scenes.size());
        if (scene_index > m_scenes.size()) {
            // TODO: Is this worth an exception?
            spdlog::error("No vertices for scene index {}!", scene_index);
            return std::vector<std::uint32_t>{};
        }
        return m_scenes[scene_index].indices;
    }
};

} // namespace inexor::vulkan_renderer::gltf
