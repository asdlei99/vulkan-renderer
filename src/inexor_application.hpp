﻿#pragma once

// toml11: TOML for Modern C++
// https://github.com/ToruNiina/toml11
// License: MIT.
#include <toml11/toml.hpp>

#include "vulkan-renderer/camera/camera.hpp"
#include "vulkan-renderer/debug-callback/debug_callback.hpp"
#include "vulkan-renderer/error-handling/error_handling.hpp"
#include "vulkan-renderer/mesh-buffer/mesh_buffer.hpp"
#include "vulkan-renderer/renderer.hpp"
#include "vulkan-renderer/shader-manager/shader_manager.hpp"
#include "vulkan-renderer/thread-pool/thread_pool.hpp"
#include "vulkan-renderer/tools/argument-parser/cla_parser.hpp"
#include "vulkan-renderer/uniform-buffer/standard_ubo.hpp"

#include <GLFW/glfw3.h>

#include <spdlog/spdlog.h>

namespace inexor {
namespace vulkan_renderer {

class InexorApplication : public VulkanRenderer, public tools::InexorCommandLineArgumentParser {
public:
    InexorApplication() = default;

    ~InexorApplication() = default;

private:
    std::string application_name = "";

    std::string engine_name = "";

    uint32_t application_version = 0;

    uint32_t engine_version = 0;

    // The core concept of paralellization in Inexor is to use a
    // C++17 threadpool implementation which spawns worker threads.
    // A task system is used to distribute work over worker threads.
    // Call thread_pool->execute(); to order new tasks to be worked on.
    std::shared_ptr<InexorThreadPool> thread_pool;

    std::vector<std::shared_ptr<InexorTexture>> textures;

    std::size_t current_frame = 0;

    // TODO: Refactor into a manger class.
    struct InexorShaderSetup {
        VkShaderStageFlagBits shader_type;
        std::string shader_file_name;
    };

    std::vector<std::string> vertex_shader_files;

    std::vector<std::string> fragment_shader_files;

    std::vector<std::string> texture_files;

    std::vector<std::string> shader_files;

    std::vector<std::string> gltf_model_files;

private:
    /// @brief Loads the configuration of the renderer from a TOML configuration file.
    /// @brief TOML_file_name [in] The TOML configuration file.
    /// @note It was collectively decided not to use JSON for configuration files.
    VkResult load_TOML_configuration_file(const std::string &TOML_file_name);

    ///
    VkResult load_textures();

    ///
    VkResult load_shaders();

    ///
    VkResult load_models();

    ///
    VkResult check_application_specific_features();

    ///
    VkResult render_frame();

    /// @brief Implementation of the uniform buffer update method.
    /// @param current_image [in] The current image index.
    VkResult update_uniform_buffers(const std::size_t current_image);

    VkResult update_keyboard_input();

public:
    VkResult init();

    /// @brief Keyboard input callback.
    /// @param window [in] The glfw window.
    /// @param key [in] The key which was pressed or released.
    /// @param scancode [in] The system-specific scancode of the key.
    /// @param action [in] The key action: GLFW_PRESS, GLFW_RELEASE or GLFW_REPEAT.
    /// @param mods [in] Bit field describing which modifier keys were held down.
    void keyboard_input_callback(GLFWwindow *window, int key, int scancode, int action, int mods);

    void run();

    void cleanup();
};

}; // namespace vulkan_renderer
}; // namespace inexor
