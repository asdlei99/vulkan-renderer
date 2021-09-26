#include "inexor/vulkan-renderer/gltf/model.hpp"

#include <glm/gtc/type_ptr.hpp>
#include <spdlog/spdlog.h>

#include <cassert>

namespace inexor::vulkan_renderer::gltf {

Model::Model(const wrapper::Device &device, const tinygltf::Model &model) : m_device(device), m_model(model) {
    assert(m_device.device());
    load_textures();
    load_materials();
    load_nodes();
}

Model::Model(const wrapper::Device &device, const ModelFile &model_file) : Model(device, model_file.model()) {}

void Model::load_textures() {
    spdlog::trace("Loading {} glTF2 model textures", m_model.images.size());

    // Preallocate memory for the model images.
    m_textures.reserve(m_model.images.size());

    for (auto texture : m_model.images) {
        // The size of the texture if it had 4 channels (rgba).
        const std::size_t texture_size =
            static_cast<std::size_t>(texture.width) * static_cast<std::size_t>(texture.height) * 4;

        // We need to convert RGB-only images to RGBA format, because most devices don't support rgb-formats in Vulkan.
        switch (texture.component) {
        case 3: {
            std::vector<std::array<std::uint32_t, 3>> rgb_source;
            rgb_source.reserve(texture_size);

            // Copy the memory into the vector so we can safely perform std::transform on it.
            std::memcpy(rgb_source.data(), &texture.image[0], texture_size);

            std::vector<std::array<std::uint32_t, 4>> rgba_target;
            rgba_target.reserve(texture_size);

            std::transform(rgb_source.begin(), rgb_source.end(), rgba_target.begin(),
                           [](const std::array<std::uint32_t, 3> a) {
                               // convert RGB-only to RGBA.
                               return std::array<std::uint32_t, 4>{a[0], a[1], a[2], 1};
                           });

            std::string texture_name = texture.name.empty() ? "glTF2 model texture" : texture.name;

            // Create a texture using the data which was converted to RGBA.
            m_textures.emplace_back(m_device, rgba_target.data(), texture_size, texture.width, texture.height,
                                    texture.component, 1, texture_name);
            break;
        }
        case 4: {
            std::string texture_name = texture.name.empty() ? "glTF2 model texture" : texture.name;

            // Create a texture using RGBA data.
            m_textures.emplace_back(m_device, &texture.image[0], texture_size, texture.width, texture.height,
                                    texture.component, 1, texture_name);
            break;
        }
        default: {
            spdlog::error("Can't load texture with {} channels!", texture.component);
            spdlog::warn("Generating error texture as a replacement.");

            // Generate an error texture.
            m_textures.emplace_back(m_device, wrapper::CpuTexture());
            break;
        }
        }
    }

    spdlog::trace("Loading {} glTF2 model texture indices", m_model.textures.size());

    // Preallocate memory for the texture indices.
    m_texture_indices.reserve(m_model.textures.size());

    for (const auto &texture : m_model.textures) {
        m_texture_indices.emplace_back(texture.source);
    }
}

void Model::load_materials() {
    spdlog::trace("Loading {} glTF2 model materials", m_model.materials.size());

    // Preallocate memory for the model materials.
    m_materials.resize(m_model.materials.size());

    ModelMaterial model_material{};

    for (auto material : m_model.materials) {
        if (material.values.find("baseColorFactor") != material.values.end()) {
            model_material.base_color_factor = glm::make_vec4(material.values["baseColorFactor"].ColorFactor().data());
        }
        if (material.values.find("baseColorTexture") != material.values.end()) {
            model_material.base_color_texture_index = material.values["baseColorTexture"].TextureIndex();
        }
        // TODO: Extract more material data from the glTF2 file as needed.

        m_materials.emplace_back(model_material);
    }
}

void Model::load_node(const tinygltf::Node &start_node, ModelNode *parent, std::vector<ModelVertex> &vertices,
                      std::vector<std::uint32_t> &indices) {

    ModelNode new_node{};
    new_node.matrix = glm::mat4(1.0f);

    // Get the local node matrix: It's either made up from translation, rotation, scale or a 4x4 matrix.
    if (start_node.translation.size() == 3) {
        new_node.matrix = glm::translate(new_node.matrix, glm::vec3(glm::make_vec3(start_node.translation.data())));
    }
    if (start_node.rotation.size() == 4) {
        glm::quat q = glm::make_quat(start_node.rotation.data());
        new_node.matrix *= glm::mat4(q);
    }
    if (start_node.scale.size() == 3) {
        new_node.matrix = glm::scale(new_node.matrix, glm::vec3(glm::make_vec3(start_node.scale.data())));
    }
    if (start_node.matrix.size() == 16) {
        new_node.matrix = glm::make_mat4x4(start_node.matrix.data());
    }

    // Load child nodes recursively.
    if (!start_node.children.empty()) {
        for (const auto &child_node : start_node.children) {
            load_node(m_model.nodes[child_node], parent, vertices, indices);
        }
    }

    if (start_node.name == "Light") {
        spdlog::trace("Loading lights from glTF2 models is not supported yet.");
    } else if (start_node.name == "Camera") {
        spdlog::trace("Loading cameras from glTF2 models is not supported yet.");
    }
    // If the node contains mesh data, we load vertices and indices from the buffers.
    // In glTF this is done via accessors and buffer views.
    else if (start_node.mesh > -1) {

        // Preallocate memory for the meshes of the model.
        new_node.mesh.reserve(m_model.meshes[start_node.mesh].primitives.size());

        // Iterate through all primitives of this node's mesh and load its vertices and indices.
        for (const auto &primitive : m_model.meshes[start_node.mesh].primitives) {
            const auto attr = primitive.attributes;
            const float *position_buffer = nullptr;
            const float *normals_buffer = nullptr;
            const float *texture_coordinate_buffer = nullptr;

            std::size_t vertex_count = 0;

            // TODO: Iterate through the map and list up all unsupported attributes.

            // Get buffer data for vertex normals.
            if (attr.find("POSITION") != attr.end()) {
                const auto &accessor = m_model.accessors[attr.find("POSITION")->second];
                const auto &view = m_model.bufferViews[accessor.bufferView];
                position_buffer = reinterpret_cast<const float *>( // NOLINT
                    &(m_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
                vertex_count = accessor.count;
            }

            // Get buffer data for vertex normals.
            if (attr.find("NORMAL") != attr.end()) {
                const auto &accessor = m_model.accessors[attr.find("NORMAL")->second];
                const auto &view = m_model.bufferViews[accessor.bufferView];
                normals_buffer = reinterpret_cast<const float *>( // NOLINT
                    &(m_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }

            // Get buffer data for vertex texture coordinates.
            // glTF supports multiple sets, we currently only load the first one.
            if (attr.find("TEXCOORD_0") != attr.end()) {
                const auto &accessor = m_model.accessors[attr.find("TEXCOORD_0")->second];
                const auto &view = m_model.bufferViews[accessor.bufferView];
                texture_coordinate_buffer = reinterpret_cast<const float *>( // NOLINT
                    &(m_model.buffers[view.buffer].data[accessor.byteOffset + view.byteOffset]));
            }

            vertices.reserve(vertices.size() + vertex_count);

            // Append data to model's vertex buffer.
            for (std::size_t vertex_number = 0; vertex_number < vertex_count; vertex_number++) {
                ModelVertex new_vertex{};
                new_vertex.pos = glm::vec4(glm::make_vec3(&position_buffer[vertex_number * 3]), 1.0f); // NOLINT

                if (normals_buffer != nullptr) {
                    new_vertex.normal = glm::normalize(glm::make_vec3(&normals_buffer[vertex_number * 3])); // NOLINT
                } else {
                    new_vertex.normal = glm::normalize(glm::vec3(0.0f));
                }

                if (texture_coordinate_buffer != nullptr) {
                    new_vertex.uv = glm::make_vec2(&texture_coordinate_buffer[vertex_number * 2]); // NOLINT
                } else {
                    new_vertex.uv = glm::vec3(0.0f);
                }

                new_vertex.color = glm::vec3(1.0f);

                vertices.emplace_back(new_vertex);
            }

            // Load indices.
            const auto &accessor = m_model.accessors[primitive.indices];
            const auto &buffer_view = m_model.bufferViews[accessor.bufferView];
            const auto &buffer = m_model.buffers[buffer_view.buffer];
            const auto vertex_start = static_cast<std::uint32_t>(vertices.size());

            // glTF2 supports different component types of indices.
            switch (accessor.componentType) {
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_INT: {
                std::vector<std::uint32_t> index_data;
                index_data.resize(accessor.count);

                std::memcpy(index_data.data(), &buffer.data[accessor.byteOffset + buffer_view.byteOffset],
                            accessor.count * sizeof(std::uint32_t));

                // TODO: Can we rewrite this as .insert()?
                indices.reserve(indices.size() + accessor.count);

                for (std::size_t index = 0; index < accessor.count; index++) {
                    indices.emplace_back(index_data[index] + vertex_start);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_SHORT: {
                std::vector<std::uint16_t> index_data;
                index_data.resize(accessor.count);

                std::memcpy(index_data.data(), &buffer.data[accessor.byteOffset + buffer_view.byteOffset],
                            accessor.count * sizeof(std::uint16_t));

                indices.reserve(indices.size() + accessor.count);

                // TODO: Can we rewrite this as .insert()?
                for (std::size_t index = 0; index < accessor.count; index++) {
                    indices.emplace_back(index_data[index] + vertex_start);
                }
                break;
            }
            case TINYGLTF_PARAMETER_TYPE_UNSIGNED_BYTE: {
                std::vector<std::uint8_t> index_data;
                index_data.resize(accessor.count);

                std::memcpy(index_data.data(), &buffer.data[accessor.byteOffset + buffer_view.byteOffset],
                            accessor.count * sizeof(std::uint8_t));

                indices.reserve(indices.size() + accessor.count);

                // TODO: Can we rewrite this as .insert()?
                for (std::size_t index = 0; index < accessor.count; index++) {
                    indices.emplace_back(index_data[index] + vertex_start);
                }
                break;
            }
            default:
                spdlog::error("Index component type {} is not supported!", accessor.componentType);
            }

            ModelPrimitive new_primitive{};
            new_primitive.first_index = static_cast<std::uint32_t>(indices.size());
            new_primitive.index_count = static_cast<std::uint32_t>(accessor.count);
            new_primitive.material_index = primitive.material;

            new_node.mesh.emplace_back(new_primitive);
        }
    } else {
        spdlog::trace("Unknown node type: {}", start_node.name);
    }

    if (parent != nullptr) {
        parent->children.push_back(new_node);
    } else {
        m_nodes.push_back(new_node);
    }
}

void Model::load_nodes() {
    spdlog::trace("Loading {} glTF2 model scenes", m_model.scenes.size());

    // Preallocate memory for the model model.
    m_scenes.resize(m_model.scenes.size());

    // Iterate through all scenes and load the model nodes scene by scene.
    for (std::size_t scene_index = 0; scene_index < m_model.scenes.size(); scene_index++) {
        for (std::size_t node_index = 0; node_index < m_model.scenes[scene_index].nodes.size(); node_index++) {
            load_node(m_model.nodes[m_model.scenes[scene_index].nodes[node_index]], nullptr,
                      m_scenes[scene_index].vertices, m_scenes[scene_index].indices);
        }
    }
}

} // namespace inexor::vulkan_renderer::gltf
