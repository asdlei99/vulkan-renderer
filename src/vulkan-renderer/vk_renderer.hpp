#pragma once

#ifdef _WIN32
#ifndef VK_USE_PLATFORM_WIN32_KHR
#define VK_USE_PLATFORM_WIN32_KHR
#endif
#endif

#include "error-handling/vk_error_handling.hpp"
#include "GPU-info/vk_gpu_info.hpp"
#include "window-manager/window_manager.hpp"
#include "availability-checks/vk_availability_checks.hpp"
#include "settings-decision-maker/vk_settings_decision_maker.hpp"
#include "shader-manager/vk_shader_manager.hpp"
#include "fence-manager/vk_fence_manager.hpp"
#include "semaphore-manager/vk_semaphore_manager.hpp"
#include "vertex/vk_vertex.hpp"
#include "mesh-buffer-manager/vk_mesh_buffer_manager.hpp"
#include "uniform-buffer-manager/vk_uniform_buffer_manager.hpp"
#include "debug-marker/vk_debug_marker_manager.hpp"
#include "queue-manager/vk_queue_manager.hpp"
#include "time-step/inexor_time_step.hpp"
#include "texture-manager/vk_texture_manager.hpp"
#include "mesh-buffer/vk_mesh_buffer.hpp"
#include "depth-buffer/vk_depth_buffer.hpp"
#include "uniform-buffer/vk_uniform_buffer.hpp"
#include "descriptor-set-manager/vk_descriptor_set_manager.hpp"
#include "gltf-models/inexor_gltf_model_manager.hpp"

// Vulkan Memory Allocator.
// https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator
// License: MIT
#include "../third_party/vma/vk_mem_alloc.h"

#include <spdlog/spdlog.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

#include <vector>
#include <string>
#include <vector>
#include <iostream>
using namespace std;

// The maximum number of images to process simultaneously.
// TODO: Refactoring! That is triple buffering essentially!
#define INEXOR_MAX_FRAMES_IN_FLIGHT 3


namespace inexor {
namespace vulkan_renderer {


	/// @class VulkanInitialisation
	/// @brief A class for initialisation of the Vulkan API.
	class VulkanRenderer :  public VulkanGraphicsCardInfoViewer,
							public VulkanWindowManager,
							public VulkanShaderManager,
							public VulkanFenceManager,
							public VulkanSemaphoreManager,
							public VulkanQueueManager,
							public VulkanDescriptorSetManager
							// TODO: VulkanSwapchainManager, VulkanPipelineManager, VulkanRenderPassManager?
	{
		public:

			VulkanRenderer();

			~VulkanRenderer();


		public:
			
			// Although many drivers and platforms trigger VK_ERROR_OUT_OF_DATE_KHR automatically after a window resize,
			// it is not guaranteed to happen. That�s why we�ll add some extra code to also handle resizes explicitly.
			bool frame_buffer_resized = false;


		protected:

			std::shared_ptr<VulkanUniformBufferManager> uniform_buffer_manager = std::make_shared<VulkanUniformBufferManager>();

			std::shared_ptr<VulkanTextureManager> texture_manager = std::make_shared<VulkanTextureManager>();
			
			std::shared_ptr<InexorMeshBufferManager> mesh_buffer_manager = std::make_shared<InexorMeshBufferManager>();

			std::shared_ptr<VulkanDebugMarkerManager> debug_marker_manager;

			std::shared_ptr<glTF2_models::InexorModelManager> gltf_model_manager;

			VmaAllocator vma_allocator;

			VkInstance instance;

			VkDevice device;

			VkSurfaceKHR surface;
			
			VkPhysicalDevice selected_graphics_card;

			VkPresentModeKHR selected_present_mode;

			VkSwapchainKHR swapchain;

			uint32_t number_of_images_in_swapchain = 0;

			VkSubmitInfo submit_info;
			
			VkPresentInfoKHR present_info = {};

			std::vector<VkImage> swapchain_images;
			
			std::vector<VkImageView> swapchain_image_views;

			VkPipelineLayout pipeline_layout = {};

			VkFormat selected_image_format = {};
			
			VkExtent2D selected_swapchain_image_extent = {};

			VkColorSpaceKHR selected_color_space = {};

			std::vector<VkPipelineShaderStageCreateInfo> shader_stages;

			VkRenderPass render_pass = VK_NULL_HANDLE;

			VkPipeline pipeline = VK_NULL_HANDLE;

			std::vector<VkFramebuffer> frame_buffers;

			VkCommandPool command_pool = VK_NULL_HANDLE;

			std::vector<VkCommandBuffer> command_buffers;
			
			std::vector<std::shared_ptr<VkSemaphore>> image_available_semaphores;
			
			std::vector<std::shared_ptr<VkSemaphore>> rendering_finished_semaphores;
			
			std::vector<std::shared_ptr<VkFence>> in_flight_fences;
			
			std::vector<std::shared_ptr<VkFence>> images_in_flight;

			VkDebugReportCallbackEXT debug_report_callback = {};

			bool debug_report_callback_initialised = false;

			InexorDepthBuffer depth_buffer;

			uint32_t vma_dump_index = 0;

			InexorTimeStep time_step;


		public:

			/// @brief Run Vulkan memory allocator's memory statistics.
			VkResult calculate_memory_budget();


		protected:


			/// @brief Creates a Vulkan instance.
			/// @param application_name The name of the application
			/// @param engine_name The name of the engine.
			/// @param application_version The version of the application encoded as an unsigned 32 bit integer.
			/// @param engine_version The version of the engine encoded as an unsigned 32 bit integer.
			/// @param enable_validation_layers True if validation is enabled.
			VkResult create_vulkan_instance(const std::string& application_name, const std::string& engine_name, const uint32_t application_version, const uint32_t engine_version, bool enable_validation_instance_layers = true, bool enable_renderdoc_instance_layer = false);


			/// @brief Create a window surface.
			/// @param vulkan_instance The instance of Vulkan.
			/// @param window The GLFW window.
			/// @param vulkan_surface The Vulkan (window) surface.
			VkResult create_window_surface(const VkInstance& vulkan_instance, GLFWwindow* window, VkSurfaceKHR& vulkan_surface);


			/// @brief Create a physical device handle.
			/// @param graphics_card The regarded graphics card.
			VkResult create_physical_device(const VkPhysicalDevice& graphics_card, const bool enable_debug_markers = true);

			
			/// @brief Creates an instance of VulkanDebugMarkerManager
			VkResult initialise_debug_marker_manager(const bool enable_debug_markers = true);


			/// @brief Initialises glTF 2.0 model manager.
			VkResult initialise_glTF2_model_manager();


			/// @brief Initialise allocator of Vulkan Memory Allocator library.
			VkResult create_vma_allocator();


			/// @brief Create depth image.
			VkResult create_depth_buffer();


			/// @brief Creates the command buffers.
			VkResult create_command_buffers();


			/// @brief Records the command buffers.
			VkResult record_command_buffers(const std::vector<InexorMeshBuffer>& buffers);


			/// @brief Creates the semaphores neccesary for synchronisation.
			VkResult create_synchronisation_objects();


			/// @brief Creates the swapchain.
			VkResult create_swapchain();
			
			
			/// @brief Cleans the swapchain.
			VkResult cleanup_swapchain();
			
			
			/// @brief Creates the uniform buffers.
			VkResult create_uniform_buffers();

			
			/// @brief Creates the descriptor set.
			VkResult create_descriptor_sets();
			

			/// @brief Updates the uniform buffer.
			virtual VkResult update_uniform_buffer(const std::size_t current_image) = 0;


			/// @brief Recreates the swapchain.
			VkResult recreate_swapchain(std::vector<InexorMeshBuffer>& mesh_buffers);


			/// @brief Creates the command pool.
			VkResult create_command_pool();

			// TODO
			VkResult create_descriptor_pool();

			// TODO
			VkResult create_descriptor_set_layout();

			/// @brief Creates the frame buffers.
			VkResult create_frame_buffers();


			/// @brief Creates the rendering pipeline.
			VkResult create_pipeline();


			/// @brief Creates the image views.
			VkResult create_swapchain_image_views();


			/// @brief Destroys all Vulkan objects.
			VkResult shutdown_vulkan();

	};
	
};
};
