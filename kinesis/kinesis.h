#ifndef KINESIS_H
#define KINESIS_H

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"
#include <stdio.h>  // printf, fprintf
#include <stdlib.h> // abort
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

// Volk headers
#ifdef IMGUI_IMPL_VULKAN_USE_VOLK
#define VOLK_IMPLEMENTATION
#include <volk.h>
#endif

// #define APP_USE_UNLIMITED_FRAME_RATE
#ifdef _DEBUG
#define APP_USE_VULKAN_DEBUG_REPORT
#endif

namespace Kinesis
{
    // Data
    extern VkAllocationCallbacks *g_Allocator;
    extern VkInstance g_Instance;
    extern VkPhysicalDevice g_PhysicalDevice;
    extern VkDevice g_Device;
    extern uint32_t g_QueueFamily;
    extern VkQueue g_Queue;
    extern VkDebugReportCallbackEXT g_DebugReport;
    extern VkPipelineCache g_PipelineCache;
    extern VkDescriptorPool g_DescriptorPool;

    extern ImGui_ImplVulkanH_Window g_MainWindowData;
    extern uint32_t g_MinImageCount;
    extern bool g_SwapChainRebuild;

    //namespace functions
    bool run();
    void initialize(int width = 600, int height = 600);
}

#endif