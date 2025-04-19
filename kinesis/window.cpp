#include "window.h"
#include "renderer.h" // Include renderer for initialization order
#include "GUI.h"      // Include GUI for initialization order

#define GLM_FORCE_RADIANS           // Ensure GLM uses radians
#define GLM_FORCE_DEPTH_ZERO_TO_ONE // Vulkan depth range is [0, 1]
#include <glm/glm.hpp>
#include "gameobject.h"
#include <stdexcept> // Required for std::runtime_error
#include <iostream>  // Required for std::cerr
#include <vector>    // Required for std::vector

// Forward declare SwapChain if not included via kinesis.h/renderer.h
// namespace Kinesis { class SwapChain; }

namespace Kinesis::Window
{

    GLFWwindow *window = nullptr;
    ImGui_ImplVulkanH_Window g_MainWindowData; // Keep this struct, ImGui might still use parts of it.
    uint32_t width = 600;
    uint32_t height = 600;
    bool fbResized = false;
    ImGui_ImplVulkanH_Window *wd = nullptr;

    // --- Raytracing Feature/Extension Structures ---
    // We need to query and enable these if supported
    VkPhysicalDeviceAccelerationStructureFeaturesKHR enabledAccelerationStructureFeatures{};
    VkPhysicalDeviceRayTracingPipelineFeaturesKHR enabledRayTracingPipelineFeatures{};
    VkPhysicalDeviceBufferDeviceAddressFeatures enabledBufferDeviceAddressFeatures{};
    // ---

    void glfw_error_callback(int error, const char *description) { fprintf(stderr, "GLFW Error %d: %s\n", error, description); }

    void check_vk_result(VkResult err)
    {
        if (err == VK_SUCCESS)
        {
            return;
        }
        fprintf(stderr, "[vulkan] Error: VkResult = %d\n", err);
        if (err < 0)
        {
            // Consider more graceful error handling than abort()
            // throw std::runtime_error("Vulkan Error: " + std::to_string(err));
            abort();
        }
    }

#ifdef APP_USE_VULKAN_DEBUG_REPORT
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT objectType, uint64_t object, size_t location, int32_t messageCode, const char *pLayerPrefix, const char *pMessage, void *pUserData)
    {
        (void)flags;
        (void)object;
        (void)location;
        (void)messageCode;
        (void)pUserData;
        (void)pLayerPrefix; // Unused arguments
        // Use std::cerr for consistency?
        std::cerr << "[vulkan] Debug report from ObjectType: " << objectType << "\nMessage: " << pMessage << "\n\n";
        // fprintf(stderr, "[vulkan] Debug report from ObjectType: %i\nMessage: %s\n\n", objectType, pMessage);
        return VK_FALSE;
    }
#else
    // Provide a stub if debug report is not used, otherwise linker might complain if called
    VKAPI_ATTR VkBool32 VKAPI_CALL debug_report(VkDebugReportFlagsEXT, VkDebugReportObjectTypeEXT, uint64_t, size_t, int32_t, const char *, const char *, void *)
    {
        return VK_FALSE;
    }
#endif // APP_USE_VULKAN_DEBUG_REPORT

    bool IsExtensionAvailable(const ImVector<VkExtensionProperties> &properties, const char *extension)
    {
        for (const VkExtensionProperties &p : properties)
        {
            if (strcmp(p.extensionName, extension) == 0)
            {
                return true;
            }
        }
        return false;
    }

    // --- SetupVulkan ---
    // This function seems correct for setting up Instance, PhysicalDevice, Device, Queue, DescriptorPool.
    void SetupVulkan(ImVector<const char *> instance_extensions)
    {
        VkResult err;
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
        volkInitialize();
#endif

        // Create Vulkan Instance
        {
            VkInstanceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;

            // Enumerate available extensions
            uint32_t properties_count;
            ImVector<VkExtensionProperties> properties;
            vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
            properties.resize(properties_count);
            err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
            check_vk_result(err);

            // --- Raytracing Instance Extension Check --- START ---
            // Check for necessary instance extensions first (some RT extensions are instance-level)
            // Although the core RT extensions are device-level, some dependencies might be instance level.
            // Let's assume for now the primary checks are device-level based on the original code structure.
            // --- Raytracing Instance Extension Check --- END ---

            // Enable required extensions
            if (IsExtensionAvailable(properties, VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME))
                instance_extensions.push_back(VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME);
#ifdef VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME
            if (IsExtensionAvailable(properties, VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME))
            {
                instance_extensions.push_back(VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME);
                create_info.flags |= VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
            }
#endif

            // Enabling validation layers
#ifdef APP_USE_VULKAN_DEBUG_REPORT
            const char *layers[] = {"VK_LAYER_KHRONOS_validation"};
            // Check layer support before enabling (good practice)
            uint32_t layerCount;
            vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
            std::vector<VkLayerProperties> availableLayers(layerCount);
            vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
            bool layerFound = false;
            for (const auto &layerProperties : availableLayers)
            {
                if (strcmp(layers[0], layerProperties.layerName) == 0)
                {
                    layerFound = true;
                    break;
                }
            }

            if (layerFound)
            {
                create_info.enabledLayerCount = 1;
                create_info.ppEnabledLayerNames = layers;
                instance_extensions.push_back("VK_EXT_debug_report");
                std::cout << "Validation layers enabled." << std::endl;
            }
            else
            {
                std::cerr << "Warning: Validation layer VK_LAYER_KHRONOS_validation not found." << std::endl;
                create_info.enabledLayerCount = 0;
            }
#else
            create_info.enabledLayerCount = 0; // Ensure layers are off if debug report is off
#endif

            // Create Vulkan Instance
            create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
            create_info.ppEnabledExtensionNames = instance_extensions.Data;
            err = vkCreateInstance(&create_info, g_Allocator, &g_Instance);
            check_vk_result(err);
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
            volkLoadInstance(g_Instance);
#endif

            // Setup the debug report callback
#ifdef APP_USE_VULKAN_DEBUG_REPORT
            if (layerFound)
            { // Only setup callback if layers were enabled
                auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
                IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr); // Use IM_ASSERT for ImGui context consistency
                VkDebugReportCallbackCreateInfoEXT debug_report_ci = {};
                debug_report_ci.sType = VK_STRUCTURE_TYPE_DEBUG_REPORT_CALLBACK_CREATE_INFO_EXT;
                debug_report_ci.flags = VK_DEBUG_REPORT_ERROR_BIT_EXT | VK_DEBUG_REPORT_WARNING_BIT_EXT | VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT;
                debug_report_ci.pfnCallback = debug_report;
                debug_report_ci.pUserData = nullptr;
                err = f_vkCreateDebugReportCallbackEXT(g_Instance, &debug_report_ci, g_Allocator, &g_DebugReport);
                check_vk_result(err);
            }
#endif
        }

        // Select Physical Device (GPU)
        g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
        if (g_PhysicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Failed to find suitable GPU!");
        }

        // --- Check for Raytracing Support --- START ---
        // Enumerate device extensions
        uint32_t device_properties_count;
        ImVector<VkExtensionProperties> device_properties;
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &device_properties_count, nullptr);
        device_properties.resize(device_properties_count);
        vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &device_properties_count, device_properties.Data);

        // Define required ray tracing extensions
        const std::vector<const char*> required_rt_extensions = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            // Dependencies that might be needed:
            VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME
        };

        // Check if all required extensions are available
        Kinesis::GUI::raytracing_available = true; // Assume available initially
        for (const char* ext_name : required_rt_extensions) {
            if (!IsExtensionAvailable(device_properties, ext_name)) {
                Kinesis::GUI::raytracing_available = false;
                std::cout << "Raytracing Skipped: Missing required device extension: " << ext_name << std::endl;
                break; // No need to check further if one is missing
            }
        }

        // If extensions are available, query for ray tracing features
        if (Kinesis::GUI::raytracing_available) {
            // Initialize feature structures with sType and pNext chain
            enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

            enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures; // Chain features

            enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddressFeatures; // Chain features

            // Add other feature structs like VkPhysicalDeviceDescriptorIndexingFeaturesEXT if needed and chain them here

            VkPhysicalDeviceFeatures2 deviceFeatures2{};
            deviceFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            deviceFeatures2.pNext = &enabledAccelerationStructureFeatures; // Point to the head of the features chain

            vkGetPhysicalDeviceFeatures2(g_PhysicalDevice, &deviceFeatures2);

            // Check if the core features are supported by the device
            if (!enabledAccelerationStructureFeatures.accelerationStructure ||
                !enabledRayTracingPipelineFeatures.rayTracingPipeline ||
                !enabledBufferDeviceAddressFeatures.bufferDeviceAddress) {
                Kinesis::GUI::raytracing_available = false;
                std::cout << "Raytracing Skipped: Required features not supported by the device." << std::endl;
            } else {
                 std::cout << "Raytracing Supported and Enabled!" << std::endl;
            }
        }
        // --- Check for Raytracing Support --- END ---


        // Select graphics queue family
        // Note: Ensure this finds a queue that supports graphics AND presentation (WSI).
        g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
        if (g_QueueFamily == (uint32_t)-1)
        {
            throw std::runtime_error("Failed to find suitable queue family!");
        }

        // Create Logical Device (with 1 queue)
        {
            ImVector<const char *> device_extensions;
            device_extensions.push_back("VK_KHR_swapchain"); // Swapchain extension is mandatory

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            if (IsExtensionAvailable(device_properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME)) // Use already enumerated properties
                device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif
            // --- Add Raytracing Device Extensions if Available ---
            if (Kinesis::GUI::raytracing_available)
            {
                for (const char* ext_name : required_rt_extensions) {
                     // We already checked availability, so just add them
                    device_extensions.push_back(ext_name);
                }
                 std::cout << "Enabling Raytracing Device Extensions..." << std::endl;
            }
            // ---

            const float queue_priority[] = {1.0f};
            VkDeviceQueueCreateInfo queue_info[1] = {};
            queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info[0].queueFamilyIndex = g_QueueFamily;
            queue_info[0].queueCount = 1;
            queue_info[0].pQueuePriorities = queue_priority;

            // Base device features (enable features needed by rasterization pipeline)
            VkPhysicalDeviceFeatures deviceFeatures = {};
            // Example: deviceFeatures.samplerAnisotropy = VK_TRUE; // Enable if needed

            VkDeviceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
            create_info.pQueueCreateInfos = queue_info;
            create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
            create_info.ppEnabledExtensionNames = device_extensions.Data;
            create_info.pEnabledFeatures = &deviceFeatures; // Base features for rasterization

            // --- Chain Raytracing Features if Available ---
            void* pNextChain = nullptr; // Pointer to the start of the feature chain
            if (Kinesis::GUI::raytracing_available)
            {
                // The feature structs (enabledAccelerationStructureFeatures, etc.) are already populated
                // from vkGetPhysicalDeviceFeatures2 earlier. We just need to point pNext to them.
                create_info.pNext = &enabledAccelerationStructureFeatures; // Point to the head of the RT feature chain
                std::cout << "Linking Raytracing Features to Device Creation..." << std::endl;
            }
            // ---

            err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
            check_vk_result(err);
            vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
        }

        // Create Descriptor Pool
        // Increased pool sizes and added types potentially needed for raytracing
        {
            std::vector<VkDescriptorPoolSize> pool_sizes =
                {
                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}
                };
            // Add types needed for raytracing if available
             if (Kinesis::GUI::raytracing_available) {
                 pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 10 }); // Example size
                 pool_sizes.push_back({ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 }); // Often used for vertex/index/material data in RT
                 // Add other types like storage image if needed
            }

            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT; // UPDATE_AFTER_BIND might be needed for descriptor indexing
            pool_info.maxSets = 2000 + (Kinesis::GUI::raytracing_available ? 100 : 0); // Increase max sets if RT is enabled
            pool_info.poolSizeCount = static_cast<uint32_t>(pool_sizes.size());
            pool_info.pPoolSizes = pool_sizes.data();
            err = vkCreateDescriptorPool(g_Device, &pool_info, g_Allocator, &g_DescriptorPool);
            check_vk_result(err);
        }
    }

    // --- SetupVulkanWindow ---

    void SetupVulkanWindow(ImGui_ImplVulkanH_Window *window_data, VkSurfaceKHR surface, int /*width*/, int /*height*/)
    {
        window_data->Surface = surface; // Assign the surface created via GLFW

        // Check for WSI support (Presentation support) on the selected queue family
        VkBool32 res;
        vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, window_data->Surface, &res);
        if (res != VK_TRUE)
        {
            // Use std::cerr or throw for better error reporting
            std::cerr << "Error: Selected queue family (" << g_QueueFamily << ") does not support presentation (WSI) on the created surface." << std::endl;
            throw std::runtime_error("No WSI support on selected queue family for the surface!");
        }
    }

    void CleanupVulkan()
    {
        // Ensure device is idle before destroying resources
        if (g_Device != VK_NULL_HANDLE)
        {
            // It's generally safer to wait for idle here, though cleanup functions might do it too.
             vkDeviceWaitIdle(g_Device); // Wait here before destroying anything device-related
        }

        // Destroy descriptor pool before device
        if (g_DescriptorPool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(g_Device, g_DescriptorPool, g_Allocator);
            g_DescriptorPool = VK_NULL_HANDLE;
        }

        // Destroy device before instance
        if (g_Device != VK_NULL_HANDLE)
        {
            vkDestroyDevice(g_Device, g_Allocator);
            g_Device = VK_NULL_HANDLE; // Nullify handles after destruction
        }

        // Destroy debug report callback *after* device, but *before* instance
#ifdef APP_USE_VULKAN_DEBUG_REPORT
        if (g_DebugReport != VK_NULL_HANDLE && g_Instance != VK_NULL_HANDLE)
        { // Check instance too
            auto f_vkDestroyDebugReportCallbackEXT = (PFN_vkDestroyDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkDestroyDebugReportCallbackEXT");
            // Check if the function pointer is valid before calling
            if (f_vkDestroyDebugReportCallbackEXT)
            {
                f_vkDestroyDebugReportCallbackEXT(g_Instance, g_DebugReport, g_Allocator);
            }
            g_DebugReport = VK_NULL_HANDLE;
        }
#endif // APP_USE_VULKAN_DEBUG_REPORT


        // Destroy the surface *before* the instance
        if (g_Instance != VK_NULL_HANDLE && g_MainWindowData.Surface != VK_NULL_HANDLE)
        {
                vkDestroySurfaceKHR(g_Instance, g_MainWindowData.Surface, g_Allocator);
                g_MainWindowData.Surface = VK_NULL_HANDLE;
        }

        // Finally, destroy the instance
        if (g_Instance != VK_NULL_HANDLE)
        {
            vkDestroyInstance(g_Instance, g_Allocator);
            g_Instance = VK_NULL_HANDLE;
        }
    }

    // --- CleanupVulkanWindow ---
    // Now only responsible for destroying the resources managed by ImGui's helper struct,
    // specifically *excluding* the swapchain which is managed externally.
    void CleanupVulkanWindow()
    {
        // ImGui_ImplVulkanH_DestroyWindow should be called BEFORE vkDestroyDevice
        // It cleans up swapchain-related resources managed by ImGui's helpers,
        // but not the swapchain itself (which Renderer::Cleanup handles).
        // We need the Device and Instance handles to be valid here.
        if (g_Instance != VK_NULL_HANDLE && g_Device != VK_NULL_HANDLE)
        { // Check handles
            // Ensure device is idle before destroying window-specific resources that might be in use
             vkDeviceWaitIdle(g_Device);
            ImGui_ImplVulkanH_DestroyWindow(g_Instance, g_Device, &g_MainWindowData, g_Allocator);
        }
        // Surface destruction is handled in CleanupVulkan.
    }

    // --- findMemoryType ---
    // Correct helper function for finding memory types.
    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties)
    {
        if (g_PhysicalDevice == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Physical device not selected or invalid in findMemoryType!");
        }
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(g_PhysicalDevice, &memProperties);
        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
        {
            // Check if the bit for this memory type index is set in the typeFilter
            // AND Check if all the required property flags are present in this memory type
            if ((typeFilter & (1 << i)) &&
                (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
            {
                return i; // Found a suitable memory type
            }
        }

        throw std::runtime_error("failed to find suitable memory type!");
    }

    // --- createBuffer ---
    // Correct helper function for buffer creation.
    void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer &buffer, VkDeviceMemory &bufferMemory)
    {
        if (g_Device == VK_NULL_HANDLE)
        {
            throw std::runtime_error("Logical device not created or invalid in createBuffer!");
        }

        // Add required usage flags if raytracing is enabled
        if (Kinesis::GUI::raytracing_available) {
             usage |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT; // Required for RT
             // Potentially add VK_BUFFER_USAGE_ACCELERATION_STRUCTURE_BUILD_INPUT_READ_ONLY_BIT_KHR if this buffer holds geometry for BVH builds
             // Potentially add VK_BUFFER_USAGE_STORAGE_BUFFER_BIT if used as storage buffer in shaders
        }


        VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Assume exclusive access unless otherwise specified

        if (vkCreateBuffer(g_Device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create buffer!");
        }

        // Get memory requirements for the buffer
        VkMemoryRequirements memReccs;
        vkGetBufferMemoryRequirements(g_Device, buffer, &memReccs);

        // Allocate memory
        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memReccs.size;
        // Find the appropriate memory type index
        allocInfo.memoryTypeIndex = findMemoryType(memReccs.memoryTypeBits, properties);

        // --- Add MemoryAllocateFlagsInfo if needed for buffer device address ---
         VkMemoryAllocateFlagsInfo allocFlagsInfo{};
         if (Kinesis::GUI::raytracing_available) {
            allocFlagsInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_FLAGS_INFO;
            allocFlagsInfo.flags = VK_MEMORY_ALLOCATE_DEVICE_ADDRESS_BIT;
            allocInfo.pNext = &allocFlagsInfo; // Chain this info
         }
         // ---

        if (vkAllocateMemory(g_Device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS)
        {
            // Clean up buffer if memory allocation failed
            vkDestroyBuffer(g_Device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
            throw std::runtime_error("failed to allocate buffer memory!");
        }

        // Bind the allocated memory to the buffer
        // The last parameter (memoryOffset) is usually 0 unless using dedicated allocations or memory aliasing
        if (vkBindBufferMemory(g_Device, buffer, bufferMemory, 0) != VK_SUCCESS)
        {
            // Clean up buffer and memory if binding failed
            vkDestroyBuffer(g_Device, buffer, nullptr);
            buffer = VK_NULL_HANDLE;
            vkFreeMemory(g_Device, bufferMemory, nullptr);
            bufferMemory = VK_NULL_HANDLE;
            throw std::runtime_error("failed to bind buffer memory!");
        }
    }

    // --- fbResizeCallback ---
    // Correctly sets the flag for the main loop to handle resizing.
    void fbResizeCallback(GLFWwindow * /*window*/, int n_width, int n_height)
    {
        // Prevent setting resize flag for 0 dimensions (e.g., minimization)
        if (n_width > 0 && n_height > 0)
        {
            fbResized = true;
            // Update stored width/height. Use static_cast for safety.
            width = static_cast<uint32_t>(n_width);
            height = static_cast<uint32_t>(n_height);
        }
    }

    int initialize(int Swidth, int Sheight)
    {
        width = Swidth;
        height = Sheight;

        glfwSetErrorCallback(glfw_error_callback);
        if (!glfwInit())
        {
            std::cerr << "Failed to initialize GLFW!" << std::endl;
            return 1;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // Specify no OpenGL/ES context
        // glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE); // Resizing enabled by default usually
        window = glfwCreateWindow(width, height, "Kinesis Engine", nullptr, nullptr);
        if (!window)
        {
            std::cerr << "Failed to create GLFW window!" << std::endl;
            glfwTerminate();
            return 1;
        }

        if (!glfwVulkanSupported())
        {
            std::cerr << "GLFW: Vulkan Not Supported!" << std::endl;
            glfwDestroyWindow(window);
            glfwTerminate();
            return 1;
        }
        std::cout << "GLFW Initialized and Vulkan Supported." << std::endl;

        glfwSetWindowUserPointer(window, nullptr); // Or pass 'this' if in a class context
        glfwSetFramebufferSizeCallback(window, fbResizeCallback);

        ImVector<const char *> extensions;
        uint32_t extensions_count = 0;
        const char **glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
        for (uint32_t i = 0; i < extensions_count; i++)
            extensions.push_back(glfw_extensions[i]);
        SetupVulkan(extensions); // Creates g_Instance, g_PhysicalDevice, g_Device, g_Queue, g_DescriptorPool
                                 // Also checks for and enables Raytracing features/extensions

        VkSurfaceKHR surface;
        VkResult err = glfwCreateWindowSurface(g_Instance, window, g_Allocator, &surface);
        check_vk_result(err);
        std::cout << "Vulkan Surface Created." << std::endl;
        SetupVulkanWindow(&g_MainWindowData, surface, width, height); // g_MainWindowData gets the surface

        // This wd assignment seems redundant if g_MainWindowData is the primary holder
        int w, h;
        glfwGetFramebufferSize(window, &w, &h);
        wd = &g_MainWindowData;
        // SetupVulkanWindow(wd, surface, w, h); // Called above, likely not needed twice

        Kinesis::Renderer::Initialize(); // Now creates Kinesis::Renderer::SwapChain internally
        std::cout << "Kinesis Renderer Initialized." << std::endl;

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void)io;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls (Optional)
        // io.ConfigFlags |= ImGuiConfigFlags_DockingEnable; // Example: Enable Docking
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable; // Example: Enable Viewports

        // Setup ImGui style (Dark recommended for development)
        ImGui::StyleColorsDark();
        ImGui_ImplGlfw_InitForVulkan(window, true); // Initialize GLFW backend

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = g_Instance;
        init_info.PhysicalDevice = g_PhysicalDevice;
        init_info.Device = g_Device;
        init_info.QueueFamily = g_QueueFamily;
        init_info.Queue = g_Queue;
        init_info.PipelineCache = g_PipelineCache;   // Optional: Can improve startup time
        init_info.DescriptorPool = g_DescriptorPool; // Use the main descriptor pool

        // Get required info from the created SwapChain object via the Renderer
        assert(Kinesis::Renderer::SwapChain != nullptr && "Swapchain must be initialized before ImGui Vulkan init!");
        init_info.RenderPass = Kinesis::Renderer::SwapChain->getRenderPass(); // Use the swapchain's render pass
        init_info.Subpass = 0;                                                // Assuming ImGui renders in the first subpass
                                                                              // Use MAX_FRAMES_IN_FLIGHT for MinImageCount and ImageCount, as ImGui manages resources for this many frames.
        init_info.MinImageCount = Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT;
        init_info.ImageCount = Kinesis::SwapChain::MAX_FRAMES_IN_FLIGHT;
        init_info.MSAASamples = VK_SAMPLE_COUNT_1_BIT; // Assuming no MSAA for now
        init_info.Allocator = g_Allocator;
        init_info.CheckVkResultFn = Window::check_vk_result; // Use our check function
        ImGui_ImplVulkan_Init(&init_info);                   // Initialize Vulkan backend
         std::cout << "ImGui Vulkan Backend Initialized." << std::endl;

        Kinesis::GUI::initialize();
         std::cout << "Kinesis GUI Initialized." << std::endl;

        // TODO: Add font loading here if needed

        std::cout << "Kinesis Window Initialization Complete." << std::endl;
        return 0; // Success
    }

    // --- cleanup ---
    // Adjusted order for proper resource destruction.
    void cleanup()
    {
        // Ensure all Vulkan commands have finished before cleanup
        // Wait for idle here, before destroying anything Vulkan-related.
        if (g_Device != VK_NULL_HANDLE)
        {
             vkDeviceWaitIdle(g_Device); // Explicitly wait here
        }

        ImGui_ImplVulkan_Shutdown(); // Shuts down Vulkan backend (needs valid device)
        ImGui_ImplGlfw_Shutdown();   // Shuts down GLFW backend
        ImGui::DestroyContext();     // Destroys ImGui context

        // Cleanup Renderer (handles swapchain destruction) before cleaning up Pipeline/Vulkan Core
        Kinesis::Renderer::Cleanup();

        // Cleanup pipeline resources (needs valid device)
        Kinesis::Pipeline::cleanup();

        // Cleanup core Vulkan objects (calls vkDeviceWaitIdle internally again, but safe)
        // CleanupVulkan destroys Device, Instance, Surface, Pool, Debug Report in correct order
        CleanupVulkan();

        // Cleanup GLFW window and terminate GLFW
        if (window != nullptr)
        {
            glfwDestroyWindow(window);
            window = nullptr;
        }
        glfwTerminate();
         std::cout << "Kinesis Window Cleanup Complete." << std::endl;
    }
} // namespace Kinesis::Window