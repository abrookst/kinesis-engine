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

    void SetupVulkan(ImVector<const char *> instance_extensions)
    {
        VkResult err;
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
        volkInitialize();
#endif

        // Create Vulkan Instance
        {
            // --- ADD THIS ---
            VkApplicationInfo appInfo{}; // Create the struct
            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = "Kinesis Engine";           // Example name
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0); // Example version
            appInfo.pEngineName = "Kinesis";                       // Example engine name
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);      // Example engine version
            appInfo.apiVersion = VK_API_VERSION_1_4;               // Set the desired Vulkan API version (e.g., Vulkan 1.2)
                                                                   // Or VK_API_VERSION_1_0, VK_API_VERSION_1_1, VK_API_VERSION_1_3 etc.
            // --- END ADD ---

            VkInstanceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            create_info.pApplicationInfo = &appInfo; // <<< Set pApplicationInfo here

            // Enumerate available extensions
            uint32_t properties_count;
            ImVector<VkExtensionProperties> properties;
            vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, nullptr);
            properties.resize(properties_count);
            err = vkEnumerateInstanceExtensionProperties(nullptr, &properties_count, properties.Data);
            check_vk_result(err);

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
            create_info.enabledLayerCount = 0;
#endif

            // Create Vulkan Instance
            create_info.enabledExtensionCount = (uint32_t)instance_extensions.Size;
            create_info.ppEnabledExtensionNames = instance_extensions.Data;
            err = vkCreateInstance(&create_info, g_Allocator, &g_Instance); // Pass the modified create_info
            check_vk_result(err);
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
            volkLoadInstance(g_Instance);
#endif

            // Setup the debug report callback
#ifdef APP_USE_VULKAN_DEBUG_REPORT
            if (layerFound)
            {
                auto f_vkCreateDebugReportCallbackEXT = (PFN_vkCreateDebugReportCallbackEXT)vkGetInstanceProcAddr(g_Instance, "vkCreateDebugReportCallbackEXT");
                IM_ASSERT(f_vkCreateDebugReportCallbackEXT != nullptr);
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

        // Enumerate all physical devices
        uint32_t gpu_count = 0;
        vkEnumeratePhysicalDevices(g_Instance, &gpu_count, nullptr);
        if (gpu_count == 0)
        {
            throw std::runtime_error("No Vulkan-compatible GPUs found!");
        }
        std::vector<VkPhysicalDevice> gpus(gpu_count);
        vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus.data());

        // Define required ray tracing extensions
        const std::vector<const char *> required_rt_extensions = {
            VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME,
            VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME,
            VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME,
            VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME,
            VK_KHR_SPIRV_1_4_EXTENSION_NAME,
            VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME};

        // Print information about all available GPUs
        std::cout << "\nAvailable Vulkan GPUs:\n";
        std::cout << "----------------------\n";
        for (size_t i = 0; i < gpus.size(); i++)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(gpus[i], &props);

            std::cout << "\nGPU #" << i << ": " << props.deviceName << "\n";
            std::cout << "  API Version: " << VK_VERSION_MAJOR(props.apiVersion) << "."
                      << VK_VERSION_MINOR(props.apiVersion) << "."
                      << VK_VERSION_PATCH(props.apiVersion) << "\n";
            std::cout << "  Driver Version: " << props.driverVersion << "\n";
            std::cout << "  Vendor ID: " << props.vendorID << "\n";
            std::cout << "  Device ID: " << props.deviceID << "\n";
            std::cout << "  Device Type: ";
            switch (props.deviceType)
            {
            case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
                std::cout << "Integrated GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
                std::cout << "Discrete GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
                std::cout << "Virtual GPU";
                break;
            case VK_PHYSICAL_DEVICE_TYPE_CPU:
                std::cout << "CPU";
                break;
            default:
                std::cout << "Other";
                break;
            }
            std::cout << "\n";

            // Print extensions
            uint32_t extension_count = 0;
            vkEnumerateDeviceExtensionProperties(gpus[i], nullptr, &extension_count, nullptr);
            std::vector<VkExtensionProperties> extensions(extension_count);
            vkEnumerateDeviceExtensionProperties(gpus[i], nullptr, &extension_count, extensions.data());

            // Print some feature support
            VkPhysicalDeviceFeatures features;
            vkGetPhysicalDeviceFeatures(gpus[i], &features);
            std::cout << "  Features:\n";
            std::cout << "    - Geometry Shader: " << (features.geometryShader ? "Yes" : "No") << "\n";
            std::cout << "    - Tessellation Shader: " << (features.tessellationShader ? "Yes" : "No") << "\n";
            std::cout << "    - MultiViewport: " << (features.multiViewport ? "Yes" : "No") << "\n";
        }
        std::cout << "\n";

        // Try to find a GPU with raytracing support
        bool found_rt_gpu = false;
        for (const auto &gpu : gpus)
        {
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(gpu, &props);

            // Check device extensions
            uint32_t device_properties_count;
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &device_properties_count, nullptr);
            std::vector<VkExtensionProperties> device_properties(device_properties_count);
            vkEnumerateDeviceExtensionProperties(gpu, nullptr, &device_properties_count, device_properties.data());

            // Check if all required extensions are available
            bool all_extensions_available = true;
            for (const char *ext_name : required_rt_extensions)
            {
                bool extension_found = false;
                for (const auto &prop : device_properties)
                {
                    if (strcmp(ext_name, prop.extensionName) == 0)
                    {
                        extension_found = true;
                        break;
                    }
                }
                if (!extension_found)
                {
                    all_extensions_available = false;
                    std::cout << "Raytracing extension missing on " << props.deviceName << ": " << ext_name << "\n";
                }
            }

            if (!all_extensions_available)
            {
                continue;
            }

            // Check for required features
            VkPhysicalDeviceAccelerationStructureFeaturesKHR accel_features{};
            accel_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;

            VkPhysicalDeviceRayTracingPipelineFeaturesKHR rt_pipeline_features{};
            rt_pipeline_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
            accel_features.pNext = &rt_pipeline_features;

            VkPhysicalDeviceBufferDeviceAddressFeatures buffer_addr_features{};
            buffer_addr_features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
            rt_pipeline_features.pNext = &buffer_addr_features;

            VkPhysicalDeviceFeatures2 device_features2{};
            device_features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
            device_features2.pNext = &accel_features;

            vkGetPhysicalDeviceFeatures2(gpu, &device_features2);

            if (accel_features.accelerationStructure &&
                rt_pipeline_features.rayTracingPipeline &&
                buffer_addr_features.bufferDeviceAddress)
            {
                g_PhysicalDevice = gpu;
                found_rt_gpu = true;
                Kinesis::GUI::raytracing_available = true;
                std::cout << "\nSelected GPU with raytracing support: " << props.deviceName << "\n";

                // Print raytracing-specific features
                std::cout << "Raytracing Features:\n";
                std::cout << "  - Acceleration Structure: " << (accel_features.accelerationStructure ? "Yes" : "No") << "\n";
                std::cout << "  - Ray Tracing Pipeline: " << (rt_pipeline_features.rayTracingPipeline ? "Yes" : "No") << "\n";
                std::cout << "  - Buffer Device Address: " << (buffer_addr_features.bufferDeviceAddress ? "Yes" : "No") << "\n";
                break;
            }
            else
            {
                std::cout << "GPU " << props.deviceName << " has extensions but missing required features:\n";
                std::cout << "  - Acceleration Structure: " << (accel_features.accelerationStructure ? "Yes" : "No") << "\n";
                std::cout << "  - Ray Tracing Pipeline: " << (rt_pipeline_features.rayTracingPipeline ? "Yes" : "No") << "\n";
                std::cout << "  - Buffer Device Address: " << (buffer_addr_features.bufferDeviceAddress ? "Yes" : "No") << "\n";
            }
        }

        // If no GPU with raytracing support was found, fall back to any suitable GPU
        if (!found_rt_gpu)
        {
            Kinesis::GUI::raytracing_available = false;
            std::cout << "\nNo GPU with full raytracing support found. Falling back to basic Vulkan support.\n";

            // Select first suitable GPU (ImGui's default selection logic)
            g_PhysicalDevice = ImGui_ImplVulkanH_SelectPhysicalDevice(g_Instance);
            if (g_PhysicalDevice == VK_NULL_HANDLE)
            {
                throw std::runtime_error("Failed to find suitable GPU!");
            }

            // Print selected GPU name
            VkPhysicalDeviceProperties props;
            vkGetPhysicalDeviceProperties(g_PhysicalDevice, &props);
            std::cout << "Selected GPU: " << props.deviceName << "\n";
        }

        // Select graphics queue family
        g_QueueFamily = ImGui_ImplVulkanH_SelectQueueFamilyIndex(g_PhysicalDevice);
        if (g_QueueFamily == (uint32_t)-1)
        {
            throw std::runtime_error("Failed to find suitable queue family!");
        }

        // Create Logical Device
        {
            ImVector<const char *> device_extensions;
            device_extensions.push_back("VK_KHR_swapchain"); // Swapchain extension is mandatory
            device_extensions.push_back(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

#ifdef VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME
            // Check for and enable portability subset if available
            uint32_t device_properties_count;
            vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &device_properties_count, nullptr);
            std::vector<VkExtensionProperties> device_properties(device_properties_count);
            vkEnumerateDeviceExtensionProperties(g_PhysicalDevice, nullptr, &device_properties_count, device_properties.data());

            if (IsExtensionAvailable(device_properties, VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME))
                device_extensions.push_back(VK_KHR_PORTABILITY_SUBSET_EXTENSION_NAME);
#endif

            // Add raytracing extensions if available
            if (Kinesis::GUI::raytracing_available)
            {
                for (const char *ext_name : required_rt_extensions)
                {
                    device_extensions.push_back(ext_name);
                }
                std::cout << "Enabling raytracing device extensions..." << std::endl;
            }

            if (Kinesis::GUI::raytracing_available) {
                // Add required RT extensions to device_extensions vector
                device_extensions.push_back(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
                device_extensions.push_back(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);
                device_extensions.push_back(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
                device_extensions.push_back(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
                device_extensions.push_back(VK_KHR_SPIRV_1_4_EXTENSION_NAME); // Check if needed by shaders
                device_extensions.push_back(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME); // Check if needed
            
                std::cout << "Enabling required ray tracing device extensions." << std::endl;
            }

            const float queue_priority[] = {1.0f};
            VkDeviceQueueCreateInfo queue_info[1] = {};
            queue_info[0].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queue_info[0].queueFamilyIndex = g_QueueFamily;
            queue_info[0].queueCount = 1;
            queue_info[0].pQueuePriorities = queue_priority;

            VkPhysicalDeviceFeatures deviceFeatures = {}; // Basic features, we enable specific ones via pNext

            VkDeviceCreateInfo create_info = {};
            create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            create_info.queueCreateInfoCount = sizeof(queue_info) / sizeof(queue_info[0]);
            create_info.pQueueCreateInfos = queue_info;
            create_info.enabledExtensionCount = (uint32_t)device_extensions.Size;
            create_info.ppEnabledExtensionNames = device_extensions.Data;
            create_info.pEnabledFeatures = &deviceFeatures; // Link basic features here

            // Chain raytracing features if available
            if (Kinesis::GUI::raytracing_available)
            {
                // Query the actual supported features again before enabling them
                VkPhysicalDeviceAccelerationStructureFeaturesKHR supportedAccelFeatures{};
                supportedAccelFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
                VkPhysicalDeviceRayTracingPipelineFeaturesKHR supportedRtPipelineFeatures{};
                supportedRtPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
                supportedAccelFeatures.pNext = &supportedRtPipelineFeatures;
                VkPhysicalDeviceBufferDeviceAddressFeatures supportedBufferAddrFeatures{};
                supportedBufferAddrFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
                supportedRtPipelineFeatures.pNext = &supportedBufferAddrFeatures;

                VkPhysicalDeviceFeatures2 supportedFeatures2{};
                supportedFeatures2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
                supportedFeatures2.pNext = &supportedAccelFeatures;
                vkGetPhysicalDeviceFeatures2(g_PhysicalDevice, &supportedFeatures2);

                // Initialize the enabled features structs
                enabledAccelerationStructureFeatures = {}; // Zero-initialize
                enabledAccelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
                enabledAccelerationStructureFeatures.accelerationStructure = supportedAccelFeatures.accelerationStructure; // Enable IF supported
            
                enabledRayTracingPipelineFeatures = {}; // Zero-initialize
                enabledRayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
                enabledRayTracingPipelineFeatures.rayTracingPipeline = supportedRtPipelineFeatures.rayTracingPipeline; // Enable IF supported
            
                enabledBufferDeviceAddressFeatures = {}; // Zero-initialize
                enabledBufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
                enabledBufferDeviceAddressFeatures.bufferDeviceAddress = supportedBufferAddrFeatures.bufferDeviceAddress; // Enable IF supported
            
                // **Chain the ENABLED features**
                enabledAccelerationStructureFeatures.pNext = &enabledRayTracingPipelineFeatures;
                enabledRayTracingPipelineFeatures.pNext = &enabledBufferDeviceAddressFeatures;
                enabledBufferDeviceAddressFeatures.pNext = nullptr; // End of RT chain
            
                // **Link the start of the chain to the main create_info**
                create_info.pNext = &enabledAccelerationStructureFeatures;
            
                std::cout << "Chaining enabled Raytracing features for logical device creation." << std::endl;
            }
            else{
                create_info.pNext = nullptr;
            }


            err = vkCreateDevice(g_PhysicalDevice, &create_info, g_Allocator, &g_Device);
            check_vk_result(err);
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
            if(g_Device) volkLoadDevice(g_Device); // <<< Ensure Volk loads device functions >>>
#endif
            vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);
        }

        // Create Descriptor Pool
        {
            std::vector<VkDescriptorPoolSize> pool_sizes =
                {
                    // Existing types...
                    {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
                    {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
                    // Add/ensure types for Ray Tracing if available
                    // Ensure counts are sufficient for your needs!
                    {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000} // Increase count if needed
                };
        
            // Add types needed for raytracing if available
            if (Kinesis::GUI::raytracing_available) // Use your flag indicating RT support
            {
                pool_sizes.push_back({VK_DESCRIPTOR_TYPE_ACCELERATION_STRUCTURE_KHR, 10}); // For TLAS/BLAS
                pool_sizes.push_back({VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 10});             // For RT output image
                // Add other types if needed (e.g., more storage buffers)
                std::cout << "Adding Raytracing descriptor types to the pool." << std::endl;
            }
        
            VkDescriptorPoolCreateInfo pool_info = {};
            pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT | VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
            // Increase maxSets if needed to account for the new RT descriptor set(s)
            pool_info.maxSets = 2000 + (Kinesis::GUI::raytracing_available ? 100 : 0); // Example increase
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
        if (Kinesis::GUI::raytracing_available)
        {
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
        if (Kinesis::GUI::raytracing_available)
        {
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