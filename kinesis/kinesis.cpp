#include "kinesis.h"
#include "window.h"
#include "GUI.h"

using namespace Kinesis;

namespace Kinesis {

    VkAllocationCallbacks *g_Allocator = nullptr;
    VkInstance g_Instance = VK_NULL_HANDLE;
    VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
    VkDevice g_Device = VK_NULL_HANDLE;
    uint32_t g_QueueFamily = (uint32_t)-1;
    VkQueue g_Queue = VK_NULL_HANDLE;
    VkDebugReportCallbackEXT g_DebugReport = VK_NULL_HANDLE;
    VkPipelineCache g_PipelineCache = VK_NULL_HANDLE;
    VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

    ImGui_ImplVulkanH_Window g_MainWindowData;
    uint32_t g_MinImageCount = 2;
    bool g_SwapChainRebuild = false;


    void initialize(int width, int height){
        Kinesis::Window::initialize(width, height);
        Kinesis::GUI::initialize();
    }
    
    // Main code
    bool run()
    {
        // Main loop
        //TODO: add check to see if end user ran init before run, give a clear error message instead of throwing some vulkan errors. low priority cuz im the only one using this
        if(glfwWindowShouldClose(Kinesis::Window::window)){
            vkDeviceWaitIdle(g_Device);
            Kinesis::Window::cleanup();
            return false;
        }
        else{
            // Poll and handle events (inputs, window resize, etc.)
            // You can read the io.WantCaptureMouse, io.WantCaptureKeyboard flags to tell if dear imgui wants to use your inputs.
            // - When io.WantCaptureMouse is true, do not dispatch mouse input data to your main application, or clear/overwrite your copy of the mouse data.
            // - When io.WantCaptureKeyboard is true, do not dispatch keyboard input data to your main application, or clear/overwrite your copy of the keyboard data.
            glfwPollEvents();
            Kinesis::GUI::update_imgui();
            Kinesis::Window::drawFrame();
            
    
            // Main Vulkan/Rendering Logic here
            //Kinesis::Rendering::update_render() or something
            
    
            return true;
        }
    }
}


